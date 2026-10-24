/* SPDX-License-Identifier: MIT */
/* 3720's rules (docs/3720.md): the keys' held state and latches, the clock, the ship, the shots, the rocks and the
 * hits. No drawing and no framework calls, so the host checks drive it directly (oracle/rocks, tests/rocks_trace.c).
 * The C names say rocks, since an identifier can't start with a digit. */
#ifndef ROCKS_LOGIC_H
#define ROCKS_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define ROCKS_STEP_MS 50
#define ROCKS_Q 65536             /* positions in 1/65,536 px */
#define ROCKS_W 400               /* the field, px */
#define ROCKS_H 228
#define ROCKS_TOP 12              /* the field's top row on the screen */
#define ROCKS_SLOTS 24
#define ROCKS_SHOTS 4             /* the most at once in either mode */
#define ROCKS_SHIP_R 6            /* the ship's radius for hits, px */
#define ROCKS_FREE 1200           /* gentle: a ship lost before this play clock costs nothing */

/* the keys, as the trace names them: 4, SOFT1, 6, SOFT6 turn; 7, 8, 9, SOFT2 thrust; GOLD, 5, ×, SOFT5 fire; ENTER and
   + have no job in play */
enum rocks_key { ROCKS_K4, ROCKS_KS1, ROCKS_K6, ROCKS_KS6, ROCKS_K7, ROCKS_K8, ROCKS_K9, ROCKS_KS2, ROCKS_KG, ROCKS_K5,
                 ROCKS_KX, ROCKS_KS5, ROCKS_KE, ROCKS_KP, ROCKS_KEYS };
enum rocks_size { ROCKS_NONE, ROCKS_L, ROCKS_M, ROCKS_S };

typedef struct {
    int32_t x, y, vx, vy;
    uint8_t size;                 /* enum rocks_size; NONE is an empty slot */
    uint8_t outline;
} rocks_rock;

typedef struct {
    int32_t x, y, vx, vy;
    uint8_t life;
} rocks_shot;

typedef struct {
    uint64_t rng;
    uint32_t step, acc;
    bool full;
    int32_t x, y, vx, vy;         /* the ship */
    uint8_t a, turn, cool, shield, lost, ships;
    uint32_t score, next;
    uint16_t wave, pause;
    uint32_t clock;
    bool over;
    rocks_shot shot[ROCKS_SHOTS];
    uint8_t nshot;                /* oldest first */
    rocks_rock r[ROCKS_SLOTS];
    uint16_t held;                /* a bit for each enum rocks_key */
    bool latch_l, latch_r, latch_t, latch_f;
    /* what the last step did, for the app's sound and bursts (not part of the rules) */
    uint8_t ev_shots, ev_breaks, ev_lost, ev_wave, ev_extra;
    uint8_t ev_sizes;             /* a bit for each size broken, 1 << enum rocks_size */
    bool thrusting;
    int32_t ev_break_x, ev_break_y;
} rocks_game;

extern const int16_t rocks_sin[24], rocks_cos[24];

void rocks_init(rocks_game *g, bool full, uint32_t seed, uint32_t game);
void rocks_key(rocks_game *g, int key, bool down);
int rocks_tick(rocks_game *g, uint32_t ms, void (*each_step)(const rocks_game *g, void *arg), void *arg);
void rocks_step(rocks_game *g);
uint32_t rocks_draw(rocks_game *g, uint32_t n);
int rocks_radius(const rocks_game *g, int size);   /* px */

#endif
