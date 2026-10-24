/* SPDX-License-Identifier: MIT */
#include "catch_logic.h"

#include <string.h>

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint32_t catch_draw(catch_game *g, uint32_t n)
{
    g->rng = mix64(g->rng);
    return (uint32_t)(g->rng % n);
}

void catch_init(catch_game *g, bool full, uint32_t seed, uint32_t game)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64((uint64_t)seed ^ mix64(game));
    g->full = full;
    g->bx = 176;
    g->lives = full ? 3 : 0;
    g->fall = 32;
    g->gap = 30;
    g->max = 3;
}

void catch_key(catch_game *g, int key, bool down)
{
    if (key < 0 || key >= CATCH_KEYS)
        return;
    g->held[key] = down;
    if (!down)
        return;
    if (key == CATCH_K4 || key == CATCH_KS1)
        g->latch_l = true;
    else
        g->latch_r = true;
}

/* full mode's pace, once for each 10 catches: faster, sooner and one more at once, each to its limit */
static void pace(catch_game *g)
{
    g->fall = (uint8_t)(g->fall + 8 > 96 ? 96 : g->fall + 8);
    g->gap = (uint16_t)(g->gap < 12 ? 9 : g->gap - 3);
    g->max = (uint8_t)(g->max + 1 > CATCH_STARS ? CATCH_STARS : g->max + 1);
}

void catch_step(catch_game *g)
{
    g->step++;
    g->ev_catches = g->ev_misses = g->ev_lost = g->ev_ten = 0;

    /* the basket */
    bool left = g->held[CATCH_K4] || g->held[CATCH_KS1] || g->latch_l;
    bool right = g->held[CATCH_K6] || g->held[CATCH_KS6] || g->latch_r;
    g->latch_l = g->latch_r = false;
    int v = g->full ? 8 : 6, bx = g->bx;
    if (left != right) {
        bx += left ? -v : v;
        bx = bx < 0 ? 0 : bx > CATCH_BX_MAX ? CATCH_BX_MAX : bx;
    }
    g->bump = bx != g->bx && (bx == 0 || bx == CATCH_BX_MAX);
    g->bx = (int16_t)bx;

    /* the stars, oldest first */
    uint32_t before = g->score;
    int k = 0;
    for (int i = 0; i < g->n; i++) {
        catch_star s = g->star[i];
        int was = s.y + CATCH_STAR * CATCH_SUB;
        s.y = (int16_t)(s.y + g->fall);
        int now = s.y + CATCH_STAR * CATCH_SUB;
        if (was < CATCH_BASKET_Y * CATCH_SUB && now >= CATCH_BASKET_Y * CATCH_SUB && s.x < g->bx + 44
            && s.x + CATCH_STAR > g->bx + 4) {
            g->score++;
            g->ev_catches++;
            g->ev_catch_x = s.x;
            continue;
        }
        if (s.y >= CATCH_H * CATCH_SUB) {
            g->ev_misses++;
            g->ev_miss_x = s.x;
            if (g->full && g->lives > 0) {
                g->ev_lost++;
                if (--g->lives == 0) {
                    g->over = true;
                    g->n = 0;
                    return;
                }
            }
            continue;
        }
        g->star[k++] = s;
    }
    g->n = (uint8_t)k;

    /* the next star */
    if (g->count > 0)
        g->count--;
    if (g->count == 0 && g->n < g->max) {
        g->star[g->n++] = (catch_star){(int16_t)catch_draw(g, 390), (int16_t)(-CATCH_STAR * CATCH_SUB)};
        g->count = g->gap;
    }

    for (uint32_t t = before / 10; t < g->score / 10; t++) {
        g->ev_ten = 1;
        if (g->full)
            pace(g);
    }
}

int catch_tick(catch_game *g, uint32_t ms, void (*each_step)(const catch_game *g, void *arg), void *arg)
{
    if (g->over)
        return 0;
    g->acc += ms > 100 ? 100 : ms;
    int n = 0;
    while (g->acc >= CATCH_STEP_MS && !g->over) {
        g->acc -= CATCH_STEP_MS;
        catch_step(g);
        n++;
        if (each_step)
            each_step(g, arg);
    }
    return n;
}
