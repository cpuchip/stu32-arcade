/* SPDX-License-Identifier: MIT */
/* The drift test (soroban's merge-gate probe, #4633): a struggling player falls again and again
 * and drifts back behind the best row, so the game reads rows and scores far behind the newest.
 * Walks the ball to TARGET a row at a time (the state set directly, the rows made as the app makes
 * them), then plays SECONDS through the app's own key and tick calls: 6 held, 8 every half second.
 * Prints every tick's player state (oracle/out.c) and, at the end, the deepest reads and the
 * out-of-window counts. oracle/drift_check.sh compares a build at the app's windows with one whose
 * storage holds everything (-DBABAL_ROWS=10000 -DBABAL_LOG=16384): they must print the same.
 * Usage: drift SKILL TARGET SECONDS */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "babal_app.h"
#include "oracle.h"

void app_beep(uint16_t hz, uint16_t ms)
{
    (void)hz, (void)ms;
}
uint32_t app_now_ms(void)
{
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: drift SKILL TARGET SECONDS\n");
        return 2;
    }
    int skill = atoi(argv[1]);
    double target = atof(argv[2]);
    int ticks = atoi(argv[3]) * 20;
    babal_ctx *c = calloc(1, sizeof *c);
    if (!c)
        return 1;
    const app_def *a = &babal_app;
    a->load(c, NULL, 0);
    a->start(c);
    a->tick(c, 20);
    a->key(c, skill, true, c->now_ms); /* a soft key plays that skill */
    a->key(c, skill, false, c->now_ms);
    a->tick(c, 50);
    for (double r = 1; r <= target; r += 1) {
        bb_player *p = &c->player;
        p->position_vec.z = r * 100;
        p->position_vec.y = 0;
        p->speed_vec.y = 0;
        p->is_jumping = false;
        p->has_backing_data = false;
        size_t w = bb_level_width(&c->level);
        for (size_t k = 0; k < w; k++)
            if (bb_level_slab_kind(&c->level, (intptr_t)k, (intptr_t)r, false) == BB_KIND_FLOOR) {
                p->position_vec.x = (double)k * 500.0 / (double)w;
                break;
            }
        a->tick(c, 1);
    }
    a->key(c, BABAL_KEY_RIGHT, true, c->now_ms);
    for (int i = 0; i < ticks && c->state == BABAL_PLAY; i++) {
        if (i % 10 == 0) {
            a->key(c, BABAL_KEY_JUMP, true, c->now_ms);
            a->key(c, BABAL_KEY_JUMP, false, c->now_ms);
        }
        a->tick(c, 50);
        outf("d %d row=%td best=%td newest=%td score=%" PRId32 " ev=", i, bb_player_level_row(&c->player),
             bb_player_best_row(&c->player), bb_level_last(&c->level) - 1, bb_player_score(&c->player));
        print_events(&c->last_events);
        outf("\n");
        print_state(&c->player);
    }
    fprintf(stderr, "drift %d %.0f: row %td best %td newest %td; deepest reads %zu rows, %zu log; out of window %u level, %u log\n",
            skill, target, bb_player_level_row(&c->player), bb_player_best_row(&c->player), bb_level_last(&c->level) - 1,
            c->level.max_read_depth, c->player.points_counter.log_max_depth, c->level.out_of_window,
            c->player.points_counter.log_out_of_window);
    return c->level.out_of_window || c->player.points_counter.log_out_of_window ? 3 : 0;
}
