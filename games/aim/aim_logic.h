/* SPDX-License-Identifier: MIT */
/* Aim's rules (docs/aim.md): the tiles, the entry, the stack, undo, the solver and the round generator. No
 * drawing and no framework calls, so the host checks drive it directly (oracle/aim, tests/aim_trace.c). */
#ifndef AIM_LOGIC_H
#define AIM_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t n, d; /* reduced, d > 0 */
} aim_frac;

enum aim_key {
    AIM_K0, AIM_K1, AIM_K2, AIM_K3, AIM_K4, AIM_K5, AIM_K6, AIM_K7, AIM_K8, AIM_K9,
    AIM_ENTER, AIM_PLUS, AIM_MINUS, AIM_TIMES, AIM_DIVIDE, AIM_SWAP, AIM_BACK
};

#define AIM_ENTRY_MAX 3
#define AIM_STEPS_MAX 14 /* 4 tiles, 3 operations, a swap between any two (a swap after a swap cancels) */

typedef struct {
    uint8_t kind; /* AIM_STEP_* */
    uint8_t tile;
    aim_frac a, b; /* an operation's operands, to undo it */
} aim_step;

typedef struct {
    uint8_t tiles[4];
    int32_t target;
    uint8_t used; /* bit i: tile i is on the stack or inside a value there */
    uint8_t sp;
    aim_frac stack[4];
    char entry[AIM_ENTRY_MAX + 1];
    uint8_t n_steps;
    aim_step steps[AIM_STEPS_MAX];
    bool won;
} aim_round;

void aim_round_init(aim_round *r, const uint8_t tiles[4], int32_t target);
/* one key; returns true when it beeps (a refused key changes nothing, apart from a tile an operator ended) */
bool aim_round_key(aim_round *r, enum aim_key k);

/* The solver: every value the four tiles reach, with its number of expression trees (operand order kept, a split
 * of the tiles counted once however equal tiles fall), the oracle's definition (oracle/aim/reach.py). */
#define AIM_VALS_4 922 /* the most a set of four digits reaches, measured over all 495 */
#define AIM_VALS_3 68
#define AIM_VALS_2 6

typedef struct {
    aim_frac v;
    uint32_t count;
} aim_val;

typedef struct {
    uint16_t n[16];        /* values per tile subset (mask) */
    uint16_t at[16];       /* where a subset's values start in vals */
    aim_val vals[AIM_VALS_4 + 4 * AIM_VALS_3 + 6 * AIM_VALS_2 + 4];
} aim_solver;

/* false only if a table would overflow, which the measured bounds above say cannot happen for tiles 1-9 */
bool aim_solve(aim_solver *s, const uint8_t tiles[4]);
/* the number of solutions for an integer target, 0 when unreachable */
uint32_t aim_solutions(const aim_solver *s, int32_t target);
/* one solution as the keys to press (digits, ENTER, operators), at most AIM_SHOW_MAX; returns the count, 0 if none */
#define AIM_SHOW_MAX 11 /* 4 digits, 3 ENTERs, 3 operators, and one to spare */
int aim_show(const aim_solver *s, const uint8_t tiles[4], int32_t target, enum aim_key keys[AIM_SHOW_MAX]);

/* The generator (docs/aim.md, "How the range grows"): level 0-4, round number n, the game's seed. */
#define AIM_LEVELS 5
int aim_level_hi(int level);        /* 30 + 10 * level */
uint32_t aim_level_cap(int level);  /* the most solutions a target may have, 0 meaning no limit */
/* fills tiles and the target; s is left solved for those tiles */
void aim_make_round(aim_solver *s, uint64_t seed, uint32_t n, int level, uint8_t tiles[4], int32_t *target);

#endif
