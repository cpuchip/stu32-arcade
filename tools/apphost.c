/* The APPS framework on the host, for Babal: plays an app script through apps.h's calls in their
 * order (load, start, then key or tick, a draw after each tick and each key that asks, then save,
 * stop) and judges the app three ways, each against something the app's code does not share:
 *
 * 1. The rules, against babalcore: every game is written as an oracle/ref script (PREFIX.gN.oracle:
 *    its seed, skill and the saved best, then the keys and steps by this file's own copy of the key
 *    spec: 31 left, 33 right, 26 and 18 jump on the press, steps clamped to 100 ms), and every tick
 *    of it printed here as oracle/out.c prints it, under a "game N" line, for oracle/app_check.sh.
 * 2. The app's own rules, against the spec below (exit 5): at the title a soft key k starts skill k,
 *    ENTER and 8 start the shown skill, 4 and 6 step it, every other key and every release does
 *    nothing; in a game no key ends it, and a tick ends it exactly when its events hold FINISH;
 *    tick never asks for no more ticks, nor more than 100 ms apart in a game; no two games of a
 *    session share a seed; the finish screen holds 600 ms; the scores list (babalgame's Scores.gd:
 *    the five highest over all skills, highest first) gets each finished game's score, and an
 *    unfinished one's when the app is left, once; its top is the best given to the rules; the finish
 *    says "New record!" exactly when the rules' high-score event came in that game, else "Game
 *    over"; one tone a tick, the first of the tone table the tick's events name; the windows'
 *    counts 0 at each game's end; the save's bytes as the spec lays them out, a corrupt save taken
 *    not at all, a first-format save's bests kept as the list.
 * 3. The picture, against tools/render_ref.c (exit 6): every frame drawn again from the model by
 *    the reference renderer, with the HUD and the title composed here from the spec.
 *
 * The script is played as two recorded sessions: from no save, then from a save holding a full list
 * (so a first game after power-on runs with a best); games are numbered on across both. Then once
 * more, unrecorded, from a save holding only game 1's score, to make a tie (no record).
 * App script words: `key K D MS` (key K down 1 or up 0 at MS on the app's clock, never before the
 * last tick) and `tick MS`. Frame hashes (FNV-1a 64), one a tick of the first session, go to FRAMES.
 * Usage: apphost APPSCRIPT PREFIX FRAMES */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "babal_app.h"
#include "oracle.h"
#include "render.h"

/* The ctx limit judged against: apps.h's (v1.1, 256 KB, at FIRMWARE_PIN). Babal holds the whole
 * level and log, about 165 KB. */
#define BABAL_CTX_LIMIT APP_CTX_MAX

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
static const char *const SKILL_WORDS[] = {"noob", "easy", "medium", "hard", "expert"};

/* The scores list as the spec keeps it, apart from the app's: babalgame's five highest over all
 * skills, highest first, a new score after the equal ones. */
typedef struct {
    int32_t points;
    int skill;
} score;
static score spec[5];
static int spec_n;
static bool seen_high;       /* the rules' high-score event came in this game */
static int32_t finish_score; /* the finish event's score */

static void spec_add(int32_t points, int skill)
{
    score all[6];
    int n = 0, placed = 0;
    for (int i = 0; i < spec_n; i++) {
        if (!placed && points > spec[i].points) {
            all[n++] = (score){points, skill};
            placed = 1;
        }
        all[n++] = spec[i];
    }
    if (!placed)
        all[n++] = (score){points, skill};
    spec_n = n < 5 ? n : 5;
    memcpy(spec, all, sizeof spec[0] * (size_t)spec_n);
}

static int32_t spec_best(void)
{
    return spec_n ? spec[0].points : 0;
}

static uint32_t get32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

/* A save as the spec lays it out: 'B', 2, the skill, the count, five (points LE32, skill), games LE32. */
static void make_save(uint8_t *s, int skill, const score *list, int n, uint32_t games_played)
{
    memset(s, 0, APP_SAVE_SIZE);
    s[0] = 'B';
    s[1] = 2;
    s[2] = (uint8_t)skill;
    s[3] = (uint8_t)n;
    for (int i = 0; i < n; i++) {
        for (int b = 0; b < 4; b++)
            s[4 + 5 * i + b] = (uint8_t)((uint32_t)list[i].points >> (8 * b));
        s[4 + 5 * i + 4] = (uint8_t)list[i].skill;
    }
    for (int b = 0; b < 4; b++)
        s[29 + b] = (uint8_t)(games_played >> (8 * b));
}

/* What the spec takes from a save: the list (a v2 save whole, a v1 save's bests above 0 in skill
 * order, a corrupt one nothing). */
static void spec_from_save(const uint8_t *s, uint32_t n)
{
    spec_n = 0;
    if (!s || n < 28 || s[0] != 'B' || s[2] > 4)
        return;
    if (s[1] == 1) {
        for (int i = 0; i < 5; i++)
            if ((int32_t)get32(s + 4 + 4 * i) < 0)
                return;
        for (int i = 0; i < 5; i++)
            if ((int32_t)get32(s + 4 + 4 * i) > 0)
                spec_add((int32_t)get32(s + 4 + 4 * i), i);
        return;
    }
    if (n < 33 || s[1] != 2 || s[3] > 5)
        return;
    for (int i = 0; i < s[3]; i++) {
        int32_t v = (int32_t)get32(s + 4 + 5 * i);
        if (v < 0 || s[4 + 5 * i + 4] > 4 || (i > 0 && v > (int32_t)get32(s + 4 + 5 * (i - 1))))
            return;
    }
    for (int i = 0; i < s[3]; i++)
        spec[spec_n++] = (score){(int32_t)get32(s + 4 + 5 * i), s[4 + 5 * i + 4]};
}

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
static bool recording = true;   /* the tie replay writes nothing */
static bool keep_frames = true; /* the frame hashes are the first session's (the device bench replays it) */
static int game_no;             /* games recorded so far, across sessions: names the oracle scripts */
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
        if (spec_best() > 0) {
            number(text, "BEST ", spec_best());
            ref_text(&want, centred((int)strlen(text), 2), 150, 2, text);
        }
        ref_text(&want, centred(24, 2), 196, 2, "4 6 SKILL   8 ENTER JUMP");
        ref_text(&want, centred(20, 2), 218, 2, "ENTER PLAYS   C ENDS");
        return;
    }
    const bb_player *p = &ctx.player;
    babal_hud hud = {bb_player_score(p), 0, 0, NULL}; /* the score and the progress, no best */
    hud.progress = (int32_t)(bb_player_time_progress_done(p, &ctx.level) * 1000.0 / bb_player_time_progress_total(p));
    if (ctx.state == BABAL_DONE) {
        hud.message = seen_high ? "New record!" : "Game over";
        hud.score = finish_score;
    }
    ref_render(&want, &ctx.level, p, &hud);
    if (ctx.state == BABAL_DONE) { /* the list: "N. skill - points", centred on a light box */
        for (int y = 128; y < 128 + 18 * 5 + 6; y++)
            for (int x = 40; x < LS027_W - 40; x++)
                want.px[y][x >> 3] &= (uint8_t)~(1u << (x & 7));
        for (int i = 0; i < spec_n; i++) {
            snprintf(text, sizeof text, "%d. %s - %" PRId32, i + 1, SKILL_WORDS[spec[i].skill], spec[i].points);
            ref_text(&want, centred((int)strlen(text), 2), 132 + 18 * i, 2, text);
        }
    }
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

static void spec_compare(const char *when)
{
    if (ctx.n_scores != spec_n)
        fail(5, "%s: the app's list has %d scores, the spec's %d", when, ctx.n_scores, spec_n);
    for (int i = 0; i < spec_n; i++)
        if (ctx.scores[i].points != spec[i].points || ctx.scores[i].skill != spec[i].skill)
            fail(5, "%s: score %d is %d (skill %d), the spec's %d (skill %d)", when, i, ctx.scores[i].points,
                 ctx.scores[i].skill, spec[i].points, spec[i].skill);
}

static void game_started(int key, int skill_before)
{
    if (key >= 0 && key <= 4 && ctx.skill != (bb_skill)key)
        fail(5, "soft key %d started skill %d", key, ctx.skill);
    if ((key == K_ENTER || key == K_JUMP) && ctx.skill != (bb_skill)skill_before)
        fail(5, "ENTER or 8 changed the skill");
    for (int g = 0; g < games && g < 256; g++) /* this session's games */
        if (seeds[g] == ctx.seed)
            fail(5, "game %d has game %d's seed", games + 1, g + 1);
    if (games < 256)
        seeds[games] = ctx.seed;
    games++;
    if (recording)
        game_no++;
    best_before = spec_best();
    seen_high = false;
    char path[512];
    snprintf(path, sizeof path, "%s.g%d.oracle", prefix, game_no);
    if (!recording)
        snprintf(path, sizeof path, "/dev/null");
    if (oracle)
        fclose(oracle);
    oracle = fopen(path, "w");
    if (!oracle)
        fail(1, "cannot write %s", path);
    fprintf(oracle, "seed %" PRIu64 "\nskill %s\nbest %" PRId32 "\n", ctx.seed, bb_skill_as_str(ctx.skill), best_before);
    outf("game %d\n", game_no);
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
    spec_from_save(save, n);
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
            if (was == BABAL_TITLE) {
                int starts = d && ((k >= 0 && k <= 4) || k == K_ENTER || k == K_JUMP);
                int want_skill = !d ? skill : k >= 0 && k <= 4 ? k : k == K_LEFT ? (skill + 4) % 5
                                            : k == K_RIGHT ? (skill + 1) % 5 : skill;
                if ((ctx.state == BABAL_PLAY) != starts || (int)ctx.skill != want_skill)
                    fail(5, "key %d %s at the title: state %d skill %d; the spec says %s, skill %d", k, d ? "down" : "up",
                         ctx.state, ctx.skill, starts ? "a game" : "the title", want_skill);
            }
            if (was == BABAL_PLAY && ctx.state != BABAL_PLAY)
                fail(5, "key %d ended a game", k);
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
            uint32_t next = app->tick(&ctx, ms);
            if (next == 0 || (ctx.state == BABAL_PLAY && next > STEP_MAX))
                fail(5, "tick asked for the next in %u ms (state %d)", next, ctx.state);
            total_beeps += (uint64_t)beeps;
            if (was == BABAL_PLAY)
                for (int e = 0; e < ctx.last_events.n; e++) {
                    if (ctx.last_events.v[e].kind == BB_EVENT_HIGH_SCORE)
                        seen_high = true;
                    if (ctx.last_events.v[e].kind == BB_EVENT_FINISH) {
                        finish_score = ctx.last_events.v[e].arg;
                        spec_add(finish_score, ctx.skill);
                    }
                }
            draw_and_check(app, at);
            if (recording && keep_frames)
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
            int finished = 0;
            for (int e = 0; e < ctx.last_events.n; e++)
                finished |= ctx.last_events.v[e].kind == BB_EVENT_FINISH;
            if ((ctx.state == BABAL_DONE) != finished)
                fail(5, "tick %" PRIu64 ": the game %s, and the tick's events %s FINISH", at,
                     ctx.state == BABAL_DONE ? "ended" : "went on", finished ? "hold" : "do not hold");
            if (ctx.state == BABAL_DONE) {
                if (ctx.final_score != finish_score || ctx.new_record != seen_high)
                    fail(5, "the finish: score %d (app %d), a high-score event %s (app's record %d)", finish_score,
                         ctx.final_score, seen_high ? "came" : "did not come", ctx.new_record);
                spec_compare("the finish");
                done_at = clock_ms;
                if (games == 1 && first_score < 0) {
                    first_score = finish_score;
                    first_skill = ctx.skill;
                }
                check_windows();
            }
        }
    }
    if (ctx.state == BABAL_PLAY) {
        check_windows();
        spec_add(bb_player_score(&ctx.player), ctx.skill); /* left unfinished: its score is kept */
    }
    /* leaving the app: the save, decoded as the spec lays it out, and loaded back into a fresh ctx */
    uint8_t s[APP_SAVE_SIZE];
    memset(s, 0xa5, sizeof s);
    app->save(&ctx, s, APP_SAVE_SIZE);
    app->stop(&ctx);
    spec_compare("leaving");
    if (s[0] != 'B' || s[1] != 2 || s[2] != ctx.skill || s[3] != spec_n)
        fail(5, "the save's head: %02x %02x %02x %02x", s[0], s[1], s[2], s[3]);
    for (int i = 0; i < spec_n; i++)
        if ((int32_t)get32(s + 4 + 5 * i) != spec[i].points || s[4 + 5 * i + 4] != spec[i].skill)
            fail(5, "the save's score %d: %d (skill %d), the spec's %d (skill %d)", i, (int32_t)get32(s + 4 + 5 * i),
                 s[8 + 5 * i], spec[i].points, spec[i].skill);
    if (get32(s + 29) != ctx.games)
        fail(5, "the save's games: %u, kept %u", get32(s + 29), ctx.games);
    static babal_ctx again;
    memset(&again, 0, sizeof again);
    app->load(&again, s, APP_SAVE_SIZE);
    if (again.skill != ctx.skill || again.games != ctx.games || again.n_scores != ctx.n_scores ||
        memcmp(again.scores, ctx.scores, sizeof ctx.scores[0] * (size_t)ctx.n_scores))
        fail(4, "save did not round-trip");
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
    if (app->ctx_size > BABAL_CTX_LIMIT || strlen(app->name) > 5)
        fail(1, "app_def out of bounds: ctx %u (limit %u), name %s", app->ctx_size, (unsigned)BABAL_CTX_LIMIT, app->name);
    /* saves the spec refuses or converts, each into a fresh ctx: the app must take the same */
    {
        static babal_ctx t;
        static const score five[5] = {{900, 4}, {700, 2}, {700, 1}, {300, 0}, {10, 3}};
        uint8_t v[APP_SAVE_SIZE];
        struct {
            const char *what;
            int byte, value;   /* one byte set after make_save (-1: none) */
            uint32_t n;        /* bytes given */
        } cases[] = {
            {"a whole save", -1, 0, APP_SAVE_SIZE}, {"the wrong magic", 0, 'X', APP_SAVE_SIZE},
            {"version 3", 1, 3, APP_SAVE_SIZE},    {"skill byte 5", 2, 5, APP_SAVE_SIZE},
            {"a count of 6", 3, 6, APP_SAVE_SIZE}, {"an entry of skill 9", 8, 9, APP_SAVE_SIZE},
            {"a negative score", 7, 0x80, APP_SAVE_SIZE}, {"scores out of order", 14, 0xff, APP_SAVE_SIZE},
            {"32 bytes", -1, 0, 32},
        };
        for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) {
            make_save(v, 3, five, 5, 77);
            if (cases[c].byte >= 0)
                v[cases[c].byte] = (uint8_t)cases[c].value;
            memset(&t, 0, sizeof t);
            app->load(&t, v, cases[c].n);
            spec_from_save(v, cases[c].n);
            int same = t.n_scores == spec_n;
            for (int k = 0; same && k < spec_n; k++)
                same = t.scores[k].points == spec[k].points && t.scores[k].skill == spec[k].skill;
            if (!same || (spec_n == 0 && t.games != 0))
                fail(5, "a save with %s: the app took %d scores, the spec %d", cases[c].what, t.n_scores, spec_n);
        }
        /* the first format: a best per skill becomes the list (bests above 0, in skill order) */
        uint8_t old[APP_SAVE_SIZE] = {'B', 1, 4, 0, 5, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0};
        memset(&t, 0, sizeof t);
        app->load(&t, old, APP_SAVE_SIZE);
        spec_from_save(old, APP_SAVE_SIZE);
        if (t.n_scores != 4 || spec_n != 4 || t.scores[0].points != 9 || t.scores[1].skill != 0 ||
            t.scores[2].skill != 4 || t.scores[3].points != 1 || t.skill != 4 || t.games != 3)
            fail(5, "a first-format save: %d scores, top %d, skill %d, games %u", t.n_scores, t.scores[0].points, t.skill,
                 t.games);
    }
    replay(in, NULL, 0, true);
    int session_a_games = games;
    uint64_t frames_a = frames_checked;
    {
        /* session B: from a save holding a full list, so game 1 after power-on has a best */
        static const score B[5] = {{320, 4}, {240, 3}, {160, 2}, {120, 1}, {40, 0}};
        uint8_t bests[APP_SAVE_SIZE];
        make_save(bests, BB_SKILL_DEFAULT, B, 5, 0);
        int32_t keep_first = first_score;
        int keep_skill = first_skill;
        if (oracle)
            fclose(oracle);
        oracle = NULL;
        keep_frames = false;
        replay(in, bests, APP_SAVE_SIZE, true);
        first_score = keep_first;
        first_skill = keep_skill;
    }
    if (oracle)
        fclose(oracle);
    oracle = NULL;
    int recorded_games = game_no;
    uint64_t recorded_frames = frames_checked, recorded_tones = total_beeps;
    if (first_score >= 0) {
        /* a tie, made on purpose: the same script again from a save holding only game 1's score
         * and 0 games, so game 1 replays to the same score: no high-score event, "Game over" */
        score one = {first_score, first_skill};
        uint8_t tie[APP_SAVE_SIZE];
        make_save(tie, BB_SKILL_DEFAULT, &one, 1, 0);
        int32_t want_score = first_score;
        replay(in, tie, APP_SAVE_SIZE, false);
        if (first_score != want_score)
            fail(5, "the tie replay scored %d, not %d", first_score, want_score);
    }
    fprintf(stderr, "apphost: %d games, %" PRIu64 " frames checked against the reference, %" PRIu64
            " tones (%d games, %" PRIu64 " frames from no save); tie replay %s; ctx %u bytes\n", recorded_games,
            recorded_frames, recorded_tones, session_a_games, frames_a,
            first_score >= 0 ? "done" : "skipped (game 1 did not finish)", app->ctx_size);
    fclose(frames);
    return 0;
}
