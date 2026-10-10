/* SPDX-License-Identifier: MIT */
/* Mines' rules (docs/mines.md): the field, opening, the chord, flags, dealing and the game's solver. No drawing and
 * no framework calls, so the host checks drive it directly (oracle/mines, tests/mines_trace.c). */
#ifndef MINES_LOGIC_H
#define MINES_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define MINES_W 24
#define MINES_H 13
#define MINES_CELLS (MINES_W * MINES_H)
#define MINES_N 48
#define MINES_TRIES 100

enum { MINES_MINE = 1, MINES_OPEN = 2, MINES_FLAG = 4, MINES_QUEUED = 8 /* the flood's, while on its stack */ };
enum mines_state { MINES_PLAY, MINES_LOST, MINES_WON };

typedef struct {
    uint8_t cell[MINES_CELLS];  /* MINES_* bits */
    uint8_t num[MINES_CELLS];   /* mines among the neighbours, once dealt */
    uint16_t opened, flags, mines; /* mines: the field's (MINES_N once dealt) */
    uint8_t state;              /* enum mines_state */
    bool dealt;
    int16_t hit;                /* the mine that lost, or -1 */
    uint8_t attempts;           /* the dealing's, 1..MINES_TRIES */
    uint64_t rng;
    /* the dealing's and the solver's scratch */
    uint16_t list[MINES_CELLS];
    uint8_t s_cell[MINES_CELLS];
    uint16_t stack[MINES_CELLS];
} mines_game;

void mines_init(mines_game *g, uint64_t seed, uint32_t game);
void mines_set(mines_game *g, const uint16_t *mines, int n); /* a hand-made field, already dealt */
void mines_open(mines_game *g, int c);
void mines_flag(mines_game *g, int c);
/* the game's solver on g's mines from opening f: true when it clears the field; *opened, its opened cells */
bool mines_solve(mines_game *g, int f, int *opened);
int mines_neighbours(int c, int16_t out[8]);

#endif
