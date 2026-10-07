/* The APPS framework on the host, for Babal: plays an app script through apps.h's calls in their
 * order (load, start, then key or tick, a draw after each tick and each key that asks, then save,
 * stop) and checks the app against babalcore end to end.
 *
 * App script words: `key K D MS` (key K down 1 or up 0 at MS on the app's clock, never before the
 * last tick) and `tick MS`. For each tick taken while a game runs it prints the oracle's tick and
 * state lines (oracle/out.c); the frames' hashes go to FRAMES. It also writes ORACLE, the same game
 * as an oracle/ref script, from its own copy of the key spec (31 left, 33 right, 26 and 18 jump,
 * steps clamped to 100 ms), so the app's own mapping is what is judged.
 * Usage: apphost APPSCRIPT ORACLE FRAMES */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "babal_app.h"
#include "oracle.h"

static uint32_t clock_ms;
static int beeps;
void app_beep(uint16_t hz, uint16_t ms)
{
    (void)hz, (void)ms;
    beeps++;
}
uint32_t app_now_ms(void)
{
    return clock_ms;
}

static babal_ctx ctx;
static ls027_frame frame;
static FILE *oracle, *frames;
static uint64_t ticks;
static int games; /* games seen start; only the first is written to ORACLE */

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

static void print_tick(void)
{
    char b[6][17], fb[6][9];
    const bb_player *p = &ctx.player;
    outf("t %" PRIu64 " ev=", ticks);
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

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: apphost APPSCRIPT ORACLE FRAMES\n");
        return 2;
    }
    FILE *in = fopen(argv[1], "r");
    oracle = fopen(argv[2], "w");
    frames = fopen(argv[3], "w");
    if (!in || !oracle || !frames) {
        perror("apphost");
        return 1;
    }
    const app_def *app = &babal_app;
    if (app->ctx_size > APP_CTX_MAX || strlen(app->name) > 5) {
        fprintf(stderr, "app_def out of bounds: ctx %u, name %s\n", app->ctx_size, app->name);
        return 1;
    }
    app->load(&ctx, NULL, 0);
    app->start(&ctx);
    char line[128];
    while (fgets(line, sizeof line, in)) {
        int k, d;
        unsigned ms;
        if (sscanf(line, "key %d %d %u", &k, &d, &ms) == 3) {
            if (ms < clock_ms) {
                fprintf(stderr, "key before the last tick: %s", line);
                return 1;
            }
            int was = ctx.state;
            if (app->key(&ctx, k, d != 0, ms))
                app->draw(&ctx, &frame);
            if (was == BABAL_TITLE && ctx.state == BABAL_PLAY && ++games == 1)
                fprintf(oracle, "seed %" PRIu64 "\nskill %s\n", ctx.seed, bb_skill_as_str(ctx.skill));
            if (was == BABAL_PLAY && games == 1) {
                if (k == 31)
                    fprintf(oracle, "at %" PRIu64 "\nL %d\n", (uint64_t)ms * 1000000, d);
                else if (k == 33)
                    fprintf(oracle, "at %" PRIu64 "\nR %d\n", (uint64_t)ms * 1000000, d);
                else if ((k == 26 || k == 18) && d)
                    fprintf(oracle, "at %" PRIu64 "\nJ\n", (uint64_t)ms * 1000000);
            }
        } else if (sscanf(line, "tick %u", &ms) == 1) {
            int was = ctx.state;
            clock_ms += ms;
            app->tick(&ctx, ms);
            app->draw(&ctx, &frame);
            frame_hash();
            if (was == BABAL_PLAY && games == 1) {
                double delta = (double)(ms > 100 ? 100 : ms) / 1000.0;
                fprintf(oracle, "at %" PRIu64 "\ntickd %.17g\n", (uint64_t)clock_ms * 1000000, delta);
                print_tick();
                ticks++;
            }
        }
    }
    uint8_t save[APP_SAVE_SIZE] = {0};
    app->save(&ctx, save, APP_SAVE_SIZE);
    app->stop(&ctx);
    /* the save comes back as it went (a fresh ctx, as the framework gives) */
    static babal_ctx again;
    app->load(&again, save, APP_SAVE_SIZE);
    if (again.skill != ctx.skill || again.games != ctx.games || memcmp(again.best, ctx.best, sizeof ctx.best)) {
        fprintf(stderr, "save did not round-trip\n");
        return 4;
    }
    fprintf(stderr, "apphost: %" PRIu64 " ticks played, %d games, %d beeps, state %d, ctx %u bytes, depth %zu/%zu\n",
            ticks, games, beeps, ctx.state, app->ctx_size, ctx.level.max_read_depth,
            ctx.player.points_counter.log_max_depth);
    if (ctx.level.out_of_window || ctx.player.points_counter.log_out_of_window || ctx.steer.dropped_keys)
        return 3;
    fclose(oracle);
    fclose(frames);
    return 0;
}
