/* SPDX-License-Identifier: MIT */
/* MOLES's rules (docs/moles.md): the bonks, the clock, the holes and the moles. No drawing and no framework calls, so
 * the host checks drive it directly (oracle/moles, tests/moles_trace.c). */
#ifndef MOLES_LOGIC_H
#define MOLES_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define MOLES_STEP_MS 50
#define MOLES_HOLES 9             /* in the order 7 8 9 4 5 6 1 2 3 */
#define MOLES_QUEUE 16            /* the downs kept for the next step */
#define MOLES_RISING 4
#define MOLES_BONKED 8
#define MOLES_SINKING 4
#define MOLES_STUN 6

enum moles_hole { MOLES_EMPTY, MOLES_RISE, MOLES_UP, MOLES_BONK, MOLES_SINK };
enum moles_level { MOLES_GENTLE, MOLES_MIDDLE, MOLES_FULL, MOLES_LEVELS };

typedef struct {
    uint8_t state;                /* enum moles_hole */
    uint32_t n;                   /* that state's steps so far (a gentle mole waits without end) */
} moles_hole;

typedef struct {
    uint64_t rng;
    uint32_t step, acc;
    uint8_t level;                /* enum moles_level */
    uint32_t score, bonks;
    uint8_t lives;                /* 0 in gentle */
    uint8_t wait, stay, gap, due, stun;
    int8_t last;                  /* the last hole a mole left from, -1 before any */
    bool over;
    moles_hole h[MOLES_HOLES];
    uint16_t held;                /* a bit for each hole's key */
    int8_t queue[MOLES_QUEUE];    /* holes, in the order their keys came down */
    uint8_t nq;
    /* what the last step did, for the app's sound and pictures (not part of the rules) */
    uint8_t ev_bonks, ev_wrong, ev_rises, ev_gone, ev_ten;
    uint16_t ev_wrong_at;         /* a bit for each hole a wrong key puffed */
} moles_game;

/* the hole of a digit key 1-9, -1 for anything else */
int moles_hole_of(int digit);
void moles_init(moles_game *g, int level, uint32_t seed, uint32_t game);
/* a hole's key (0-8; anything else is ignored) down or up */
void moles_key(moles_game *g, int hole, bool down);
int moles_tick(moles_game *g, uint32_t ms, void (*each_step)(const moles_game *g, void *arg), void *arg);
void moles_step(moles_game *g);
uint32_t moles_draw(moles_game *g, uint32_t n);

#endif
