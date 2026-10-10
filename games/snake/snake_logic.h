/* SPDX-License-Identifier: MIT */
/* Snake's rules (docs/snake.md): the grid, the turn queue, the step and the food. No drawing and no framework
 * calls, so the host checks drive it directly (oracle/snake, tests/snake_trace.c). */
#ifndef SNAKE_LOGIC_H
#define SNAKE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define SNAKE_W 40
#define SNAKE_H 22
#define SNAKE_CELLS (SNAKE_W * SNAKE_H)
#define SNAKE_QUEUE 2

enum snake_dir { SNAKE_UP, SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT };
enum snake_end { SNAKE_ON, SNAKE_DEAD, SNAKE_WON };

typedef struct {
    uint16_t cell[SNAKE_CELLS];    /* a ring: cell[tail] is the tail, len cells on to the head */
    uint16_t tail, len;
    uint8_t occ[SNAKE_CELLS / 8];  /* bit c: cell c is the snake's */
    uint8_t dir;
    uint8_t queue[SNAKE_QUEUE], queued;
    int16_t food;                  /* -1: none (the grid is full) */
    uint16_t eaten;
    uint64_t rng;
    uint8_t end;                   /* enum snake_end */
} snake_game;

void snake_init(snake_game *g, uint64_t seed, uint32_t game);
/* a hand-made state for the edge cases: the body from the tail to the head, the food as given */
void snake_set(snake_game *g, const uint16_t *body, int n, int dir, int food);
bool snake_offer(snake_game *g, int dir); /* a direction key's down: true when the queue took it */
enum snake_end snake_step(snake_game *g);
uint32_t snake_interval(uint32_t eaten);  /* ms between steps */
uint16_t snake_head(const snake_game *g);
uint16_t snake_at(const snake_game *g, int i); /* the i-th cell from the tail */
bool snake_has(const snake_game *g, int cell);

#endif
