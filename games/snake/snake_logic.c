/* SPDX-License-Identifier: MIT */
#include "snake_logic.h"

#include <string.h>

static const int8_t DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

bool snake_has(const snake_game *g, int c)
{
    return g->occ[c >> 3] >> (c & 7) & 1;
}

static void mark(snake_game *g, int c, bool on)
{
    if (on)
        g->occ[c >> 3] |= (uint8_t)(1u << (c & 7));
    else
        g->occ[c >> 3] &= (uint8_t)~(1u << (c & 7));
}

uint16_t snake_at(const snake_game *g, int i)
{
    return g->cell[(g->tail + i) % SNAKE_CELLS];
}

uint16_t snake_head(const snake_game *g)
{
    return snake_at(g, g->len - 1);
}

/* the k-th free cell in row order, k from 0 */
static bool place_food(snake_game *g)
{
    uint32_t free = SNAKE_CELLS - g->len;
    if (free == 0) {
        g->food = -1;
        return false;
    }
    g->rng = mix64(g->rng);
    uint32_t k = (uint32_t)(g->rng % free);
    for (int c = 0; c < SNAKE_CELLS; c++)
        if (!snake_has(g, c) && k-- == 0) {
            g->food = (int16_t)c;
            return true;
        }
    return false; /* not reached: free cells number free */
}

void snake_set(snake_game *g, const uint16_t *body, int n, int dir, int food)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64(mix64(0));
    for (int i = 0; i < n; i++) {
        g->cell[i] = body[i];
        mark(g, body[i], true);
    }
    g->len = (uint16_t)n;
    g->dir = (uint8_t)dir;
    g->food = (int16_t)food;
}

void snake_init(snake_game *g, uint64_t seed, uint32_t game)
{
    static const uint16_t start[4] = {10 * SNAKE_W + 7, 10 * SNAKE_W + 8, 10 * SNAKE_W + 9, 10 * SNAKE_W + 10};
    snake_set(g, start, 4, SNAKE_RIGHT, -1);
    g->rng = mix64(seed ^ mix64(game));
    place_food(g);
}

bool snake_offer(snake_game *g, int dir)
{
    int last = g->queued ? g->queue[g->queued - 1] : g->dir;
    if (g->queued >= SNAKE_QUEUE || dir == last || dir == (last + 2) % 4)
        return false;
    g->queue[g->queued++] = (uint8_t)dir;
    return true;
}

bool snake_turn(snake_game *g, bool right)
{
    int last = g->queued ? g->queue[g->queued - 1] : g->dir;
    return snake_offer(g, (last + (right ? 1 : 3)) % 4);
}

uint32_t snake_interval(uint32_t eaten)
{
    return eaten >= 18 ? 60 : 150 - 5 * eaten;
}

enum snake_end snake_step(snake_game *g)
{
    if (g->end)
        return (enum snake_end)g->end;
    if (g->queued) {
        g->dir = g->queue[0];
        g->queue[0] = g->queue[1];
        g->queued--;
    }
    int head = snake_head(g);
    int x = head % SNAKE_W + DX[g->dir], y = head / SNAKE_W + DY[g->dir];
    if (x < 0 || x >= SNAKE_W || y < 0 || y >= SNAKE_H) {
        g->end = SNAKE_DEAD;
        return SNAKE_DEAD;
    }
    int next = y * SNAKE_W + x;
    if (next == g->food) {
        g->cell[(g->tail + g->len) % SNAKE_CELLS] = (uint16_t)next;
        g->len++;
        mark(g, next, true);
        g->eaten++;
        if (!place_food(g))
            g->end = SNAKE_WON;
        return (enum snake_end)g->end;
    }
    int tail = snake_at(g, 0);
    if (snake_has(g, next) && next != tail) { /* the tail is freed first */
        g->end = SNAKE_DEAD;
        return SNAKE_DEAD;
    }
    mark(g, tail, false);
    g->tail = (uint16_t)((g->tail + 1) % SNAKE_CELLS);
    g->cell[(g->tail + g->len - 1) % SNAKE_CELLS] = (uint16_t)next;
    mark(g, next, true);
    return SNAKE_ON;
}
