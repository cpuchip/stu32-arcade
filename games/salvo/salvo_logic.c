/* SPDX-License-Identifier: MIT */
#include "salvo_logic.h"

#include <string.h>

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint32_t salvo_draw(salvo_game *g, uint32_t n)
{
    g->rng = mix64(g->rng);
    return (uint32_t)(g->rng % n);
}

int16_t salvo_slot_x(const salvo_game *g, int i)
{
    return (int16_t)(g->fx + 24 * (i % SALVO_COLS));
}

int16_t salvo_slot_y(const salvo_game *g, int i)
{
    return (int16_t)(g->fy + 16 * (i / SALVO_COLS));
}

bool salvo_ship_alive(const salvo_game *g)
{
    return !g->over && g->ship_t == 0;
}

static bool overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2)
{
    return x1 < x2 + w2 && x2 < x1 + w1 && y1 < y2 + h2 && y2 < y1 + h1;
}

static int gap(const salvo_game *g)
{
    int d = 40 - 4 * (g->wave - 1);
    return d < 12 ? 12 : d;
}

static void wave_start(salvo_game *g)
{
    g->fx = 108;
    g->fy = -60;
    g->fdir = 1;
    g->sway = 0;
    g->dive = 0;
    for (int i = 0; i < SALVO_ALIENS; i++)
        g->a[i] = (salvo_alien){SALVO_SLOT, 0, 0, 0, 0, 0};
}

void salvo_init(salvo_game *g, uint32_t seed, uint32_t game)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64((uint64_t)seed ^ mix64(game));
    g->next = 10000;
    g->sx = 192;
    g->lives = 3;
    g->wave = 1;
    wave_start(g);
}

void salvo_key(salvo_game *g, int key, bool down)
{
    if (key < 0 || key >= SALVO_KEYS)
        return;
    g->held[key] = down;
    if (!down)
        return;
    if (key == SALVO_K4 || key == SALVO_KS1)
        g->latch_l = true;
    else if (key == SALVO_K6 || key == SALVO_KS6)
        g->latch_r = true;
    else
        g->latch_f = true;
}

static void kill(salvo_game *g, int i)
{
    static const uint16_t base[SALVO_ROWS] = {150, 80, 50, 50};
    uint32_t pts = base[i / SALVO_COLS];
    if (g->a[i].state == SALVO_DIVING)
        pts *= 2;
    g->a[i].state = SALVO_DEAD;
    g->score += pts;
    while (g->score >= g->next) {       /* an extra ship at each multiple of 10,000, unless 5 are held */
        g->next += 10000;
        if (g->lives < 5)
            g->lives++;
    }
}

static void lose_ship(salvo_game *g)
{
    g->lives--;
    g->nshot = g->neshot = 0;
    for (int i = 0; i < SALVO_ALIENS; i++)
        if (g->a[i].state == SALVO_DIVING || g->a[i].state == SALVO_RETURNING)
            g->a[i].state = SALVO_SLOT;
    g->ship_t = 40;
    g->ev_lost = 1;
}

static bool any_alien(const salvo_game *g)
{
    for (int i = 0; i < SALVO_ALIENS; i++)
        if (g->a[i].state != SALVO_DEAD)
            return true;
    return false;
}

static void start_dive(salvo_game *g)
{
    int diving = 0;
    for (int i = 0; i < SALVO_ALIENS; i++)
        diving += g->a[i].state == SALVO_DIVING;
    int most = 1 + g->wave < 4 ? 1 + g->wave : 4;
    if (diving >= most)
        return;
    int c = (int)salvo_draw(g, SALVO_COLS);
    for (int j = 0; j < SALVO_COLS; j++) {
        int col = (c + j) % SALVO_COLS;
        for (int r = SALVO_ROWS - 1; r >= 0; r--) {
            int i = r * SALVO_COLS + col;
            if (g->a[i].state != SALVO_SLOT)
                continue;
            salvo_alien *a = &g->a[i];
            a->state = SALVO_DIVING;
            a->x = salvo_slot_x(g, i);
            a->y = salvo_slot_y(g, i);
            a->k = 0;
            a->vx = 0;
            a->fire_at = (int8_t)(6 + salvo_draw(g, 10));
            return;
        }
    }
}

void salvo_step(salvo_game *g)
{
    if (g->over)
        return;
    g->step++;
    g->ev_shots = g->ev_kills = g->ev_lost = g->ev_wave = 0;
    for (int s = 0; s < SALVO_SHOTS; s++)
        g->ev_kill_at[s] = -1;
    bool alive = salvo_ship_alive(g);
    bool left = g->held[SALVO_K4] || g->held[SALVO_KS1] || g->latch_l;
    bool right = g->held[SALVO_K6] || g->held[SALVO_KS6] || g->latch_r;
    bool fire = g->latch_f;
    for (int k = SALVO_K8; k <= SALVO_KP; k++)
        fire = fire || g->held[k];

    /* 1. the ship */
    if (alive) {
        if (left && !right)
            g->sx = (int16_t)(g->sx - 5 < 0 ? 0 : g->sx - 5);
        else if (right && !left)
            g->sx = (int16_t)(g->sx + 5 > 385 ? 385 : g->sx + 5);
    }

    /* 2. the shots move, then one may be fired */
    int n = 0;
    for (int s = 0; s < g->nshot; s++) {
        salvo_pt p = g->shot[s];
        p.y = (int16_t)(p.y - 12);
        if (p.y + 6 > SALVO_TOP)
            g->shot[n++] = p;
    }
    g->nshot = (uint8_t)n;
    if (g->cool > 0)
        g->cool--;
    if (alive && fire && g->cool == 0 && g->nshot < SALVO_SHOTS) {
        g->shot[g->nshot++] = (salvo_pt){(int16_t)(g->sx + 7), 222};
        g->cool = 6;
        g->ev_shots = 1;
    }

    /* 3. the formation: the entry, or the sway */
    bool entry = g->fy < 20;
    if (entry) {
        g->fy = (int16_t)(g->fy + 4);
        if (g->fy >= 20) {
            g->sway = 0;
            g->dive = (uint16_t)gap(g);
        }
    } else {
        g->sway++;
        if (g->sway % 4 == 0) {
            int nfx = g->fx + 4 * g->fdir;
            if (nfx < 8 || nfx > 208) {
                g->fdir = (int8_t)-g->fdir;
                nfx = g->fx + 4 * g->fdir;
            }
            g->fx = (int16_t)nfx;
        }
    }

    /* 4. the aliens: returning, then diving, by slot order; then the dive counter */
    for (int i = 0; i < SALVO_ALIENS; i++) {
        salvo_alien *a = &g->a[i];
        if (a->state != SALVO_RETURNING)
            continue;
        a->x = salvo_slot_x(g, i);
        a->y = (int16_t)(a->y + 4);
        if (a->y >= salvo_slot_y(g, i)) {
            a->y = salvo_slot_y(g, i);
            a->state = SALVO_SLOT;
        }
    }
    for (int i = 0; i < SALVO_ALIENS; i++) {
        salvo_alien *a = &g->a[i];
        if (a->state != SALVO_DIVING)
            continue;
        if (a->k % 4 == 0) {
            int vx = (g->sx + 1 - a->x) / 12;       /* C's division truncates toward zero */
            a->vx = (int8_t)(vx < -3 ? -3 : vx > 3 ? 3 : vx);
        }
        int x = a->x + a->vx;
        a->x = (int16_t)(x < 0 ? 0 : x > 387 ? 387 : x);
        a->y = (int16_t)(a->y + 5);
        if (a->k == a->fire_at && a->y < 180 && g->neshot < SALVO_ESHOTS) {
            g->eshot[g->neshot++] = (salvo_pt){(int16_t)(a->x + 6), (int16_t)(a->y + 10)};
            a->fire_at = -1;
        }
        a->k++;
        if (a->y >= 240) {
            a->state = SALVO_RETURNING;
            a->x = salvo_slot_x(g, i);
            a->y = -10;
        }
    }
    if (!entry && alive) {
        if (g->dive > 0)
            g->dive--;
        if (g->dive == 0) {
            g->dive = (uint16_t)gap(g);
            start_dive(g);
        }
    }

    /* 5. the enemy shots move */
    n = 0;
    for (int s = 0; s < g->neshot; s++) {
        salvo_pt p = g->eshot[s];
        p.y = (int16_t)(p.y + 6);
        if (p.y < 240)
            g->eshot[n++] = p;
    }
    g->neshot = (uint8_t)n;

    /* 6. the hits */
    n = 0;
    for (int s = 0; s < g->nshot; s++) {
        salvo_pt p = g->shot[s];
        int hit = -1;
        for (int i = 0; i < SALVO_ALIENS && hit < 0; i++) {
            salvo_alien *a = &g->a[i];
            if (a->state == SALVO_DEAD)
                continue;
            int ax = a->state == SALVO_SLOT ? salvo_slot_x(g, i) : a->x;
            int ay = a->state == SALVO_SLOT ? salvo_slot_y(g, i) : a->y;
            if (overlap(p.x, p.y, 1, 6, ax, ay, SALVO_ALIEN_W, SALVO_ALIEN_H))
                hit = i;
        }
        if (hit < 0) {
            g->shot[n++] = p;
        } else {
            const salvo_alien *a = &g->a[hit];
            g->ev_kill_pt[g->ev_kills] = a->state == SALVO_SLOT ? (salvo_pt){salvo_slot_x(g, hit), salvo_slot_y(g, hit)}
                                                               : (salvo_pt){a->x, a->y};
            g->ev_kill_at[g->ev_kills++] = (int8_t)hit;
            kill(g, hit);
        }
    }
    g->nshot = (uint8_t)n;
    bool lost = false;
    if (alive) {
        for (int s = 0; s < g->neshot && !lost; s++)
            lost = overlap(g->eshot[s].x, g->eshot[s].y, 2, 4, g->sx, SALVO_SHIP_Y, SALVO_SHIP_W, SALVO_SHIP_H);
        for (int i = 0; i < SALVO_ALIENS && !lost; i++) {
            salvo_alien *a = &g->a[i];
            if (a->state == SALVO_DIVING
                && overlap(a->x, a->y, SALVO_ALIEN_W, SALVO_ALIEN_H, g->sx, SALVO_SHIP_Y, SALVO_SHIP_W, SALVO_SHIP_H)) {
                a->state = SALVO_DEAD;          /* rams the ship, scoring nothing */
                lost = true;
            }
        }
        if (lost)
            lose_ship(g);
    }

    /* 7. the timers: the ship's, then the wave's */
    if (g->ship_t > 0 && !lost) {
        if (--g->ship_t == 0) {
            if (g->lives > 0) {
                g->sx = 192;
                g->dive = 20;
            } else {
                g->over = true;
            }
        }
    }
    if (g->over) {
        /* the game is over: the wave's timer is not looked at */
    } else if (g->wave_t > 0) {
        if (--g->wave_t == 0) {
            g->wave++;
            wave_start(g);
        }
    } else if (!any_alien(g) && salvo_ship_alive(g)) {
        g->nshot = g->neshot = 0;
        g->wave_t = 30;
        g->ev_wave = 1;
    }
    g->latch_l = g->latch_r = g->latch_f = false;
}

int salvo_tick(salvo_game *g, uint32_t ms, void (*each_step)(const salvo_game *g, void *arg), void *arg)
{
    if (g->over)
        return 0;
    g->acc += ms > 100 ? 100 : ms;
    int n = 0;
    while (g->acc >= SALVO_STEP_MS && !g->over) {
        g->acc -= SALVO_STEP_MS;
        salvo_step(g);
        n++;
        if (each_step)
            each_step(g, arg);
    }
    return n;
}
