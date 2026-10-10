/* SPDX-License-Identifier: MIT */
/* Mines' rules (docs/mines.md): the field, opening, the chord, flags, dealing and the game's solver. No drawing and
 * no framework calls, so the host checks drive it directly (oracle/mines, tests/mines_trace.c). */
#ifndef MINES_LOGIC_H
#define MINES_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define MINES_MAX_CELLS 480 /* the largest board's, 30 x 16 */
#ifndef MINES_TRIES
#define MINES_TRIES 400 /* a check builds it lower, to reach the fallback */
#endif

enum { MINES_EASY, MINES_MEDIUM, MINES_HARD, MINES_BOARDS };
typedef struct {
    uint8_t w, h, n;
} mines_board;
extern const mines_board mines_boards[MINES_BOARDS];

enum { MINES_MINE = 1, MINES_OPEN = 2, MINES_FLAG = 4, MINES_QUEUED = 8 /* the flood's, while on its stack */ };
enum mines_state { MINES_PLAY, MINES_LOST, MINES_WON };

typedef struct {
    int w, h, cells, n;         /* the board's */
    bool classic;               /* deal the first attempt, untested */
    uint8_t cell[MINES_MAX_CELLS]; /* MINES_* bits */
    uint8_t num[MINES_MAX_CELLS];  /* mines among the neighbours, once dealt */
    uint16_t opened, flags, mines; /* mines: the field's (n once dealt) */
    uint8_t state;              /* enum mines_state */
    bool dealt;
    int16_t hit;                /* the mine that lost, or -1 */
    int16_t deal_f;             /* the first open while a deal is under way, or -1 */
    uint16_t attempts;          /* the dealing's so far, 1..MINES_TRIES once dealt */
    uint64_t rng;
    /* the dealing's and the solver's scratch */
    uint16_t list[MINES_MAX_CELLS];
    uint8_t s_cell[MINES_MAX_CELLS];
    uint16_t stack[MINES_MAX_CELLS];
} mines_game;

void mines_init(mines_game *g, uint64_t seed, uint32_t game, int board, bool classic);
void mines_set(mines_game *g, int board, const uint16_t *mines, int n); /* a hand-made field, already dealt */
/* the first open at f starts a deal: false (and nothing starts) once dealt, after the end, or on a flag */
bool mines_deal_start(mines_game *g, int f);
/* up to d more attempts of the deal under way: true when the board is dealt */
bool mines_deal_step(mines_game *g, int d);
void mines_open(mines_game *g, int c); /* before the deal, deals the whole board first */
void mines_flag(mines_game *g, int c);
/* the game's solver on g's mines from opening f: true when it clears the field; *opened, its opened cells */
bool mines_solve(mines_game *g, int f, int *opened);
int mines_neighbours(const mines_game *g, int c, int16_t out[8]);

#endif
