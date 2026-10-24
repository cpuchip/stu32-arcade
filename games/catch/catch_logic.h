/* SPDX-License-Identifier: MIT */
/* CATCH's rules (docs/catch.md): the keys' held state and latches, the clock, the step and the catches. No drawing
 * and no framework calls, so the host checks drive it directly (oracle/catch, tests/catch_trace.c). */
#ifndef CATCH_LOGIC_H
#define CATCH_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define CATCH_STEP_MS 50
#define CATCH_TOP 12              /* the field's top row, under the top line */
#define CATCH_H 228               /* the field's height */
#define CATCH_STARS 6             /* the most at once in either mode */
#define CATCH_STAR 11             /* a star's side, px */
#define CATCH_BASKET_W 48
#define CATCH_BASKET_H 14
#define CATCH_BASKET_Y 202        /* the basket's top row, the mouth, in field px */
#define CATCH_BX_MAX 352
#define CATCH_SUB 16              /* a star's y is in 1/16 px */

/* the keys, as the trace names them: 4, SOFT1, 6, SOFT6 */
enum catch_key { CATCH_K4, CATCH_KS1, CATCH_K6, CATCH_KS6, CATCH_KEYS };

typedef struct {
    int16_t x, y;                 /* y in 1/16 px */
} catch_star;

typedef struct {
    uint64_t rng;
    uint32_t step, acc;
    bool full;
    int16_t bx;
    uint32_t score;
    uint8_t lives;                /* 0 in gentle */
    uint8_t fall, max;
    uint16_t gap, count;
    bool bump, over;
    catch_star star[CATCH_STARS];
    uint8_t n;                    /* oldest first */
    bool held[CATCH_KEYS];
    bool latch_l, latch_r;
    /* what the last step did, for the app's sound and flashes (not part of the rules) */
    uint8_t ev_catches, ev_misses, ev_lost, ev_ten;
    int16_t ev_catch_x;           /* where the last catch was */
} catch_game;

void catch_init(catch_game *g, bool full, uint32_t seed, uint32_t game);
void catch_key(catch_game *g, int key, bool down);
/* a tick of ms (capped at 100): runs the steps it holds, at most 2; each_step, when given, is called after each */
int catch_tick(catch_game *g, uint32_t ms, void (*each_step)(const catch_game *g, void *arg), void *arg);
void catch_step(catch_game *g);
uint32_t catch_draw(catch_game *g, uint32_t n);

#endif
