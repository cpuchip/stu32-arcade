/* SPDX-License-Identifier: MIT */
#include "moles_logic.h"

#include <string.h>

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint32_t moles_draw(moles_game *g, uint32_t n)
{
    g->rng = mix64(g->rng);
    return (uint32_t)(g->rng % n);
}

int moles_hole_of(int digit)
{
    if (digit >= 7 && digit <= 9)
        return digit - 7;
    if (digit >= 4 && digit <= 6)
        return digit - 1;
    if (digit >= 1 && digit <= 3)
        return digit + 5;
    return -1;
}

void moles_init(moles_game *g, bool full, uint32_t seed, uint32_t game)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64((uint64_t)seed ^ mix64(game));
    g->full = full;
    g->lives = full ? 3 : 0;
    g->wait = full ? 0 : 30;
    g->stay = full ? 50 : 0;
    g->gap = full ? 20 : 0;
    g->due = 20;
    g->last = -1;
}

void moles_key(moles_game *g, int hole, bool down)
{
    if (hole < 0 || hole >= MOLES_HOLES)
        return;
    bool was = g->held & 1u << hole;
    g->held = (uint16_t)(down ? g->held | 1u << hole : g->held & ~(1u << hole));
    if (down && !was && g->nq < MOLES_QUEUE)   /* a down while held is a repeat */
        g->queue[g->nq++] = (int8_t)hole;
}

static int active(const moles_game *g)
{
    int n = 0;
    for (int i = 0; i < MOLES_HOLES; i++)
        n += g->h[i].state == MOLES_RISE || g->h[i].state == MOLES_UP || g->h[i].state == MOLES_BONK;
    return n;
}

static bool any(const moles_game *g)
{
    for (int i = 0; i < MOLES_HOLES; i++)
        if (g->h[i].state != MOLES_EMPTY)
            return true;
    return false;
}

/* a mole from a hole that is empty and not the last one left from; false with none to take */
static bool rise(moles_game *g)
{
    int cand[MOLES_HOLES], n = 0;
    for (int i = 0; i < MOLES_HOLES; i++)
        if (g->h[i].state == MOLES_EMPTY && i != g->last)
            cand[n++] = i;
    if (!n)
        return false;
    int i = cand[moles_draw(g, (uint32_t)n)];
    g->h[i] = (moles_hole){MOLES_RISE, 0};
    g->ev_rises++;
    return true;
}

void moles_step(moles_game *g)
{
    g->step++;
    g->ev_bonks = g->ev_wrong = g->ev_rises = g->ev_gone = g->ev_ten = 0;
    g->ev_wrong_at = 0;

    /* the bonks, in the order they came; what a bonk or a stun starts does not count down this step */
    uint16_t fresh = 0;
    bool stunned = g->stun > 0, started = false;
    for (int q = 0; q < g->nq && !stunned; q++) {
        int i = g->queue[q];
        moles_hole *h = &g->h[i];
        if (h->state == MOLES_RISE || h->state == MOLES_UP) {
            *h = (moles_hole){MOLES_BONK, 0};
            fresh |= (uint16_t)(1u << i);
            g->score++;
            g->bonks++;
            g->ev_bonks++;
            if (g->score % 10 == 0)
                g->ev_ten = 1;
            if (g->full) {
                if (g->stay > 16)
                    g->stay--;
                if (g->bonks >= 20)
                    g->gap = 14;
            }
        } else {
            g->ev_wrong++;
            g->ev_wrong_at |= (uint16_t)(1u << i);
            if (g->full)
                g->stun = MOLES_STUN, stunned = started = true;
        }
    }
    g->nq = 0;

    /* the timers: the next mole's count, then the holes in order, then the stun */
    if (g->due > 0)
        g->due--;
    for (int i = 0; i < MOLES_HOLES; i++) {
        moles_hole *h = &g->h[i];
        if (h->state == MOLES_EMPTY || fresh & 1u << i)
            continue;
        h->n++;
        if (h->state == MOLES_RISE && h->n >= MOLES_RISING) {
            *h = (moles_hole){MOLES_UP, 0};
        } else if (h->state == MOLES_UP && g->full && h->n >= g->stay) {
            *h = (moles_hole){MOLES_SINK, 0};
            g->ev_gone++;
            if (g->lives > 0 && --g->lives == 0)
                g->over = true;
        } else if (h->state == MOLES_BONK && h->n >= MOLES_BONKED) {
            *h = (moles_hole){MOLES_SINK, 0};
        } else if (h->state == MOLES_SINK && h->n >= MOLES_SINKING) {
            *h = (moles_hole){MOLES_EMPTY, 0};
            g->last = (int8_t)i;
            if (!g->full)
                g->due = g->wait;
        }
    }
    if (g->stun > 0 && !started)
        g->stun--;
    if (g->over) {
        for (int i = 0; i < MOLES_HOLES; i++)
            if (g->h[i].state == MOLES_RISE || g->h[i].state == MOLES_UP || g->h[i].state == MOLES_BONK)
                g->h[i] = (moles_hole){MOLES_SINK, 0};
        return;
    }

    /* a new mole, if one is due */
    if (g->due != 0)
        return;
    if (g->full) {
        if (active(g) < (g->bonks >= 20 ? 2 : 1) && rise(g))
            g->due = g->gap;
    } else if (!any(g)) {
        bool waited = g->last >= 0;
        if (rise(g) && waited && g->wait > 10)
            g->wait--;
    }
}

int moles_tick(moles_game *g, uint32_t ms, void (*each_step)(const moles_game *g, void *arg), void *arg)
{
    if (g->over)
        return 0;
    g->acc += ms > 100 ? 100 : ms;
    int n = 0;
    while (g->acc >= MOLES_STEP_MS && !g->over && n < 2) {
        g->acc -= MOLES_STEP_MS;
        moles_step(g);
        n++;
        if (each_step)
            each_step(g, arg);
    }
    return n;
}
