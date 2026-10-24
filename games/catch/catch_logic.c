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

/* each level: the basket's px a step, the lives, and the pace's steps and limits for the fall, the gap and the most
   at once (gentle has no pace) */
static const struct {
    uint8_t move, lives, fall_step, fall_limit, gap_step, gap_limit, max_step, max_limit;
} level[CATCH_LEVELS] = {{6, 0, 0, 32, 0, 30, 0, 3}, {8, 5, 4, 48, 2, 18, 1, 4}, {8, 3, 8, 96, 3, 9, 1, 6}};

void catch_init(catch_game *g, int lv, uint32_t seed, uint32_t game)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64((uint64_t)seed ^ mix64(game));
    g->level = (uint8_t)(lv == CATCH_MIDDLE || lv == CATCH_FULL ? lv : CATCH_GENTLE);
    g->bx = 176;
    g->lives = level[g->level].lives;
    g->fall = 32;
    g->gap = 30;
    g->max = 3;
    g->last = -1;
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

/* middle's and full's pace, once for each 10 catches: faster, sooner and more at once, each to its limit */
static void pace(catch_game *g)
{
    int l = g->level, fall = g->fall + level[l].fall_step, gap = g->gap - level[l].gap_step;
    int max = g->max + level[l].max_step;
    g->fall = (uint8_t)(fall > level[l].fall_limit ? level[l].fall_limit : fall);
    g->gap = (uint16_t)(gap < level[l].gap_limit ? level[l].gap_limit : gap);
    g->max = (uint8_t)(max > level[l].max_limit ? level[l].max_limit : max);
}

/* a new star's x: anywhere for a game's first, else within reach of the last (docs/catch.md, the reach) */
static int16_t star_x(catch_game *g)
{
    if (g->last < 0)
        return (int16_t)catch_draw(g, 390);
    int r = 3 * level[g->level].move * g->gap / 4;
    int lo = g->last - r < 0 ? 0 : g->last - r, hi = g->last + r > 389 ? 389 : g->last + r;
    return (int16_t)(lo + (int)catch_draw(g, (uint32_t)(hi - lo + 1)));
}

void catch_step(catch_game *g)
{
    g->step++;
    g->ev_catches = g->ev_misses = g->ev_lost = g->ev_ten = 0;

    /* the basket */
    bool left = g->held[CATCH_K4] || g->held[CATCH_KS1] || g->latch_l;
    bool right = g->held[CATCH_K6] || g->held[CATCH_KS6] || g->latch_r;
    g->latch_l = g->latch_r = false;
    int v = level[g->level].move, bx = g->bx;
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
            if (g->level != CATCH_GENTLE && g->lives > 0) {
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
        g->last = star_x(g);
        g->star[g->n++] = (catch_star){g->last, (int16_t)(-CATCH_STAR * CATCH_SUB)};
        g->count = g->gap;
    }

    for (uint32_t t = before / 10; t < g->score / 10; t++) {
        g->ev_ten = 1;
        if (g->level != CATCH_GENTLE)
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
