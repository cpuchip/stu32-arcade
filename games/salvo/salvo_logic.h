/* SPDX-License-Identifier: MIT */
/* Salvo's rules (docs/salvo.md): the keys' held state and latches, the clock, the step and the hits. No drawing and
 * no framework calls, so the host checks drive it directly (oracle/salvo, tests/salvo_trace.c). */
#ifndef SALVO_LOGIC_H
#define SALVO_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define SALVO_ROWS 4
#define SALVO_COLS 8
#define SALVO_ALIENS (SALVO_ROWS * SALVO_COLS)
#define SALVO_SHOTS 2
#define SALVO_ESHOTS 6
#define SALVO_STEP_MS 50

#define SALVO_SHIP_Y 228
#define SALVO_SHIP_W 15
#define SALVO_SHIP_H 8
#define SALVO_ALIEN_W 13
#define SALVO_ALIEN_H 10
#define SALVO_TOP 12              /* the play field's top row, under the top line */
/* the ship's box for hits: its body, without the nose's first two rows and the wing tips */
#define SALVO_HIT_DX 2
#define SALVO_HIT_Y 230
#define SALVO_HIT_W 11
#define SALVO_HIT_H 6

/* the keys, as the trace names them: 4, SOFT1, 6, SOFT6, then the five that fire: 8, ENTER, 9, ×, + */
enum salvo_key { SALVO_K4, SALVO_KS1, SALVO_K6, SALVO_KS6, SALVO_K8, SALVO_KE, SALVO_K9, SALVO_KX, SALVO_KP,
                 SALVO_KEYS };
enum salvo_alien { SALVO_SLOT, SALVO_DIVING, SALVO_RETURNING, SALVO_DEAD };

typedef struct {
    uint8_t state;                /* enum salvo_alien */
    int16_t x, y;                 /* diving or returning: where it is */
    int16_t k;                    /* diving: its dive's steps taken */
    int8_t vx;
    int8_t fire_at;               /* -1 once it has fired */
} salvo_alien;

typedef struct {
    int16_t x, y;
} salvo_pt;

typedef struct {
    uint64_t rng;
    uint32_t step, acc;
    uint32_t score, next;         /* next: the multiple of 10,000 the next extra ship waits for */
    int16_t sx;
    uint8_t lives;
    uint16_t wave;
    bool over;
    uint8_t ship_t, wave_t;       /* the ship's timer (0: in play) and the wave's (0: not running) */
    int16_t fx, fy;
    int8_t fdir;
    uint32_t sway;
    uint16_t dive;
    uint8_t cool;
    salvo_pt shot[SALVO_SHOTS], eshot[SALVO_ESHOTS];
    uint8_t nshot, neshot;        /* oldest first */
    salvo_alien a[SALVO_ALIENS];
    bool held[SALVO_KEYS];
    bool latch_l, latch_r, latch_f;
    /* what the last step did, for the app's sound and bursts (not part of the rules) */
    uint8_t ev_shots, ev_kills, ev_lost, ev_wave;
    int8_t ev_kill_at[SALVO_SHOTS]; /* the slots killed by shots this step, -1 none */
    salvo_pt ev_kill_pt[SALVO_SHOTS];   /* and where each was */
} salvo_game;

void salvo_init(salvo_game *g, uint32_t seed, uint32_t game);
void salvo_key(salvo_game *g, int key, bool down);
/* a tick of ms (capped at 100): runs the steps it holds, at most 2; each_step, when given, is called after each */
int salvo_tick(salvo_game *g, uint32_t ms, void (*each_step)(const salvo_game *g, void *arg), void *arg);
void salvo_step(salvo_game *g);
uint32_t salvo_draw(salvo_game *g, uint32_t n);
int16_t salvo_slot_x(const salvo_game *g, int i);
int16_t salvo_slot_y(const salvo_game *g, int i);
bool salvo_ship_alive(const salvo_game *g);

#endif
