/* The APPS framework on the host, for Babal: plays an app script through apps.h's calls in their
 * order (load, start, then key or tick, a draw after each tick and each key that asks, then save,
 * stop) and judges the app three ways, each against something the app's code does not share:
 *
 * 1. The rules, against babalcore: every game is written as an oracle/ref script (PREFIX.gN.oracle:
 *    its seed, skill and the saved best, then the keys and steps by this file's own copy of the key
 *    spec: 31 left, 33 right, 26 and 18 jump on the press, steps clamped to 100 ms), and every tick
 *    of it printed here as oracle/out.c prints it, under a "game N" line, for oracle/app_check.sh.
 * 2. The app's own rules, against the spec below (exit 5): a soft key k starts skill k; 4 and 6 step
 *    the skill at the title; no two games share a seed; the finish screen holds 600 ms; NEW BEST
 *    only for a higher score, and the best kept is the higher; one tone a tick, the first of the
 *    tone table the tick's events name; the windows' counts 0 at each game's end; the save's bytes
 *    as the spec lays them out; a corrupt save (a bad skill byte, short) taken not at all, a best
 *    below 0 taken as none.
 * 3. The picture, against tools/render_ref.c (exit 6): every frame drawn again from the model by
 *    the reference renderer, with the HUD and the title composed here from the spec.
 *
 * App script words: `key K D MS` (key K down 1 or up 0 at MS on the app's clock, never before the
 * last tick) and `tick MS`. Frame hashes (FNV-1a 64) go to FRAMES, one a tick.
 * Usage: apphost APPSCRIPT PREFIX FRAMES */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "babal_app.h"
#include "oracle.h"
#include "render.h"

void ref_render(ls027_frame *f, const bb_level *lvl, const bb_player *p, const babal_hud *hud);
int ref_text(ls027_frame *f, int x, int y, int scale, const char *s);

/* ---- the spec, written here and not taken from the app ---- */
enum { K_LEFT = 31, K_RIGHT = 33, K_JUMP = 26, K_ENTER = 18 };
#define STEP_MAX 100
#define DONE_HOLD 600
static const struct {
    bb_event_kind kind;
    uint16_t hz, ms;
} TONES[] = {
    {BB_EVENT_FINISH, 1320, 250}, {BB_EVENT_FALL, 150, 150},       {BB_EVENT_OVERDRIVE, 990, 40},
    {BB_EVENT_BOOST, 660, 40},    {BB_EVENT_HIGH_SCORE, 1100, 80}, {BB_EVENT_THOUSAND_SCORE, 880, 30},
    {BB_EVENT_LAND, 220, 20},
};
static const char *const SKILL_NAMES[] = {"NOOB", "EASY", "MEDIUM", "HARD", "EXPERT"};

static uint32_t clock_ms;
static int beeps, beep_hz[8], beep_ms[8];
void app_beep(uint16_t hz, uint16_t ms)
{
    if (beeps < 8) {
        beep_hz[beeps] = hz;
        beep_ms[beeps] = ms;
    }
    beeps++;
}
uint32_t app_now_ms(void)
{
    return clock_ms;
}

static babal_ctx ctx;
static ls027_frame frame, want;
static FILE *frames;
static const char *prefix;
static FILE *oracle;
static bool recording = true; /* the tie replay writes nothing */
static void to_stdout(const char *s, size_t n)
{
    fwrite(s, 1, n, stdout);
}
static void to_null(const char *s, size_t n)
{
    (void)s, (void)n;
}
static uint64_t ticks_compared, frames_checked, total_beeps;
static int games;
static uint64_t seeds[256];
static int32_t best_before; /* the best for this game's skill when it started */
static uint32_t done_at;    /* when the finish screen came up */

static void fail(int code, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "apphost: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(code);
}

static void frame_hash(void)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t *b = &frame.px[0][0];
    for (size_t i = 0; i < sizeof frame.px; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    fprintf(frames, "%016" PRIx64 "\n", h);
}

static int centred(int len, int scale)
{
    return (LS027_W - len * 6 * scale) / 2;
}

static void number(char *out, const char *label, int32_t v)
{
    snprintf(out, 32, "%s%" PRId32, label, v < 0 ? 0 : v);
}

/* The frame the spec says the app should show now, drawn with the reference renderer. */
static void expected_frame(void)
{
    char text[32];
    if (ctx.state == BABAL_TITLE) {
        memset(&want, 0, sizeof want);
        ref_text(&want, centred(5, 6), 30, 6, "BABAL");
        snprintf(text, sizeof text, "< %s >", SKILL_NAMES[ctx.skill]);
        ref_text(&want, centred((int)strlen(text), 3), 110, 3, text);
        if (ctx.best[ctx.skill] > 0) {
            number(text, "BEST ", ctx.best[ctx.skill]);
            ref_text(&want, centred((int)strlen(text), 2), 150, 2, text);
        }
        ref_text(&want, centred(24, 2), 196, 2, "4 6 SKILL   8 ENTER JUMP");
        ref_text(&want, centred(20, 2), 218, 2, "ENTER PLAYS   C ENDS");
        return;
    }
    const bb_player *p = &ctx.player;
    babal_hud hud = {bb_player_score(p), ctx.best[ctx.skill], 0, NULL};
    hud.progress = (int32_t)(bb_player_time_progress_done(p, &ctx.level) * 1000.0 / bb_player_time_progress_total(p));
    if (ctx.state == BABAL_DONE) {
        hud.message = ctx.new_best ? "NEW BEST" : "FINISH";
        hud.score = ctx.final_score;
    }
    ref_render(&want, &ctx.level, p, &hud);
}

static void draw_and_check(const app_def *app, uint64_t at)
{
    app->draw(&ctx, &frame);
    expected_frame();
    frames_checked++;
    if (memcmp(&frame, &want, sizeof frame))
        fail(6, "frame %" PRIu64 " (state %d, clock %u ms) differs from the reference renderer", at, ctx.state,
             clock_ms);
}

static void check_windows(void)
{
    if (ctx.level.out_of_window || ctx.player.points_counter.log_out_of_window || ctx.steer.dropped_keys)
        fail(3, "game %d read outside a window (level %u, log %u, keys dropped %u)", games, ctx.level.out_of_window,
             ctx.player.points_counter.log_out_of_window, ctx.steer.dropped_keys);
}

static void print_tick(uint64_t n)
{
    char b[6][17], fb[6][9];
    const bb_player *p = &ctx.player;
    outf("t %" PRIu64 " ev=", n);
    print_events(&ctx.last_events);
    bb_vec3f ps = bb_player_position(p), sp = bb_player_speed(p);
    outf(" p=%s,%s,%s s=%s,%s,%s col=%td row=%td dist=%s score=%" PRId32 " best_row=%td best_dist=%s boost=%d"
         " blend=%s sf=%s tpd=%s tpt=%s\n",
         b32(ps.x, fb[0]), b32(ps.y, fb[1]), b32(ps.z, fb[2]), b32(sp.x, fb[3]), b32(sp.y, fb[4]), b32(sp.z, fb[5]),
         bb_player_level_col(p, bb_level_width(&ctx.level)), bb_player_level_row(p), b64(bb_player_dist(p), b[0]),
         bb_player_score(p), bb_player_best_row(p), b64(bb_player_best_dist(p), b[1]), bb_player_is_boosting(p) ? 1 : 0,
         b64(bb_player_backing_camera_blend(p), b[2]), b64(bb_player_speed_factor(p, &ctx.level), b[3]),
         b64(bb_player_time_progress_done(p, &ctx.level), b[4]), b64(bb_player_time_progress_total(p), b[5]));
    print_state(p);
}

static void game_started(int key, int skill_before)
{
    if (key >= 0 && key <= 4 && ctx.skill != (bb_skill)key)
        fail(5, "soft key %d started skill %d", key, ctx.skill);
    if ((key == K_ENTER || key == K_JUMP) && ctx.skill != (bb_skill)skill_before)
        fail(5, "ENTER or 8 changed the skill");
    for (int g = 0; g < games && g < 256; g++)
        if (seeds[g] == ctx.seed)
            fail(5, "game %d has game %d's seed", games + 1, g + 1);
    if (games < 256)
        seeds[games] = ctx.seed;
    games++;
    best_before = ctx.best[ctx.skill];
    char path[512];
    snprintf(path, sizeof path, "%s.g%d.oracle", recording ? prefix : "/dev/null", games);
    if (!recording)
        snprintf(path, sizeof path, "/dev/null");
    if (oracle)
        fclose(oracle);
    oracle = fopen(path, "w");
    if (!oracle)
        fail(1, "cannot write %s", path);
    fprintf(oracle, "seed %" PRIu64 "\nskill %s\nbest %" PRId32 "\n", ctx.seed, bb_skill_as_str(ctx.skill), best_before);
    outf("game %d\n", games);
    ticks_compared = 0;
}

static int32_t first_score = -1; /* game 1's final score in the last replay, -1 if it did not finish */
static int first_skill;

/* Plays the script once from SAVE (n 0: none). record: write the oracle scripts, the tick lines
 * and the frame hashes (the tie replay does not). */
static void replay(FILE *in, const uint8_t *save, uint32_t n, bool record)
{
    const app_def *app = &babal_app;
    memset(&ctx, 0, sizeof ctx);
    clock_ms = 0;
    games = 0;
    first_score = -1;
    recording = record;
    out_set_sink(record ? to_stdout : to_null);
    rewind(in);
    app->load(&ctx, save, n);
    app->start(&ctx);
    char line[128];
    uint64_t at = 0;
    while (fgets(line, sizeof line, in)) {
        int k, d;
        unsigned ms;
        if (sscanf(line, "key %d %d %u", &k, &d, &ms) == 3) {
            if (ms < clock_ms)
                fail(1, "key before the last tick: %s", line);
            int was = ctx.state, skill = ctx.skill;
            if (app->key(&ctx, k, d != 0, ms))
                draw_and_check(app, at);
            if (was == BABAL_TITLE && d && ctx.state == BABAL_TITLE) {
                int want_skill = k == K_LEFT ? (skill + 4) % 5 : k == K_RIGHT ? (skill + 1) % 5 : skill;
                if ((int)ctx.skill != want_skill)
                    fail(5, "key %d at the title: skill %d, the spec says %d", k, ctx.skill, want_skill);
            }
            if (was == BABAL_TITLE && ctx.state == BABAL_PLAY)
                game_started(k, skill);
            if (was == BABAL_DONE) {
                int leaves = d && ms - done_at >= DONE_HOLD;
                if ((ctx.state == BABAL_TITLE) != leaves)
                    fail(5, "a key %s at %u ms after the finish: the screen %s", d ? "down" : "up", ms - done_at,
                         ctx.state == BABAL_TITLE ? "left" : "stayed");
            }
            if (was == BABAL_PLAY) {
                if (k == K_LEFT)
                    fprintf(oracle, "at %" PRIu64 "\nL %d\n", (uint64_t)ms * 1000000, d);
                else if (k == K_RIGHT)
                    fprintf(oracle, "at %" PRIu64 "\nR %d\n", (uint64_t)ms * 1000000, d);
                else if ((k == K_JUMP || k == K_ENTER) && d)
                    fprintf(oracle, "at %" PRIu64 "\nJ\n", (uint64_t)ms * 1000000);
            }
        } else if (sscanf(line, "tick %u", &ms) == 1) {
            int was = ctx.state;
            clock_ms += ms;
            beeps = 0;
            app->tick(&ctx, ms);
            total_beeps += (uint64_t)beeps;
            draw_and_check(app, at);
            if (recording)
                frame_hash();
            at++;
            if (was != BABAL_PLAY) {
                if (beeps)
                    fail(5, "a tone outside a game");
                continue;
            }
            double delta = (double)(ms > STEP_MAX ? STEP_MAX : ms) / 1000.0;
            fprintf(oracle, "at %" PRIu64 "\ntickd %.17g\n", (uint64_t)clock_ms * 1000000, delta);
            print_tick(ticks_compared++);
            /* one tone: the first of the table the tick's events name */
            int t = -1;
            for (size_t i = 0; i < sizeof TONES / sizeof TONES[0] && t < 0; i++)
                for (int e = 0; e < ctx.last_events.n; e++)
                    if (ctx.last_events.v[e].kind == TONES[i].kind) {
                        t = (int)i;
                        break;
                    }
            if (t < 0 ? beeps != 0 : beeps != 1 || beep_hz[0] != TONES[t].hz || beep_ms[0] != TONES[t].ms)
                fail(5, "tick %" PRIu64 ": %d tone(s), the first %d Hz; the spec says %s %d Hz", at, beeps,
                     beeps ? beep_hz[0] : 0, t < 0 ? "none, so" : "one at", t < 0 ? 0 : TONES[t].hz);
            if (ctx.state == BABAL_DONE) {
                int32_t score = bb_player_score(&ctx.player);
                int newer = score > best_before;
                if (ctx.final_score != score || ctx.new_best != newer ||
                    ctx.best[ctx.skill] != (newer ? score : best_before))
                    fail(5, "the finish: score %d (app %d), best before %d, new best %d (app %d), best now %d", score,
                         ctx.final_score, best_before, newer, ctx.new_best, ctx.best[ctx.skill]);
                done_at = clock_ms;
                if (games == 1 && first_score < 0) {
                    first_score = score;
                    first_skill = ctx.skill;
                }
                check_windows();
            }
        }
    }
    if (ctx.state == BABAL_PLAY)
        check_windows();
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: apphost APPSCRIPT PREFIX FRAMES\n");
        return 2;
    }
    FILE *in = fopen(argv[1], "r");
    prefix = argv[2];
    frames = fopen(argv[3], "w");
    if (!in || !frames)
        fail(1, "cannot open the script or the frames file");
    const app_def *app = &babal_app;
    if (app->ctx_size > APP_CTX_MAX || strlen(app->name) > 5)
        fail(1, "app_def out of bounds: ctx %u, name %s", app->ctx_size, app->name);
    /* corrupt saves, into scratch ctxs: taken not at all, or a best below 0 as none */
    {
        static babal_ctx t;
        uint8_t s[APP_SAVE_SIZE] = {'B', 1, 9, 0, 7, 0, 0, 0};
        memset(&t, 0, sizeof t);
        app->load(&t, s, APP_SAVE_SIZE);
        if (t.skill != BB_MEDIUM || t.best[0] != 0)
            fail(5, "a save with skill byte 9 was taken (skill %d, best %d)", t.skill, t.best[0]);
        uint8_t n[APP_SAVE_SIZE] = {'B', 1, 2, 0, 0xff, 0xff, 0xff, 0xff, 5, 0, 0, 0};
        memset(&t, 0, sizeof t);
        app->load(&t, n, APP_SAVE_SIZE);
        if (t.skill != BB_MEDIUM || t.best[0] != 0 || t.best[1] != 5)
            fail(5, "a save with a best of -1: skill %d, bests %d %d", t.skill, t.best[0], t.best[1]);
        memset(&t, 0, sizeof t);
        app->load(&t, n, 27);
        if (t.best[1] != 0)
            fail(5, "a 27-byte save was taken");
    }
    replay(in, NULL, 0, true);
    if (oracle)
        fclose(oracle);
    oracle = NULL;
    /* the save, decoded byte by byte from the spec, and loaded back into a fresh ctx */
    uint8_t save[APP_SAVE_SIZE];
    memset(save, 0xa5, sizeof save);
    app->save(&ctx, save, APP_SAVE_SIZE);
    app->stop(&ctx);
    if (save[0] != 'B' || save[1] != 1 || save[2] != ctx.skill)
        fail(5, "the save's head: %02x %02x %02x", save[0], save[1], save[2]);
    for (int i = 0; i < 5; i++) {
        const uint8_t *b = save + 4 + 4 * i;
        int32_t v = (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
        if (v != ctx.best[i])
            fail(5, "the save's best for skill %d: %d, kept %d", i, v, ctx.best[i]);
    }
    uint32_t g = (uint32_t)save[24] | (uint32_t)save[25] << 8 | (uint32_t)save[26] << 16 | (uint32_t)save[27] << 24;
    if (g != ctx.games)
        fail(5, "the save's games: %u, kept %u", g, ctx.games);
    static babal_ctx again;
    app->load(&again, save, APP_SAVE_SIZE);
    if (again.skill != ctx.skill || again.games != ctx.games || memcmp(again.best, ctx.best, sizeof ctx.best))
        fail(4, "save did not round-trip");
    int recorded_games = games;
    uint64_t recorded_frames = frames_checked, recorded_tones = total_beeps;
    if (first_score >= 0) {
        /* a tie, made on purpose: the same script again from a save that holds game 1's score as
         * that skill's best and 0 games, so game 1 replays to the same score: not a new best */
        uint8_t tie[APP_SAVE_SIZE] = {'B', 1, BB_SKILL_DEFAULT};
        for (int b = 0; b < 4; b++)
            tie[4 + 4 * first_skill + b] = (uint8_t)((uint32_t)first_score >> (8 * b));
        int32_t want_score = first_score;
        replay(in, tie, APP_SAVE_SIZE, false);
        if (first_score != want_score)
            fail(5, "the tie replay scored %d, not %d", first_score, want_score);
    }
    fprintf(stderr, "apphost: %d games, %" PRIu64 " frames checked against the reference, %" PRIu64
            " tones; tie replay %s; ctx %u bytes\n", recorded_games, recorded_frames, recorded_tones,
            first_score >= 0 ? "done" : "skipped (game 1 did not finish)", app->ctx_size);
    fclose(frames);
    return 0;
}
