/* SPDX-License-Identifier: MIT */
#include "rocks_logic.h"

#include <string.h>

#define WQ ((int32_t)ROCKS_W * ROCKS_Q)
#define HQ ((int32_t)ROCKS_H * ROCKS_Q)

/* round(16384 sin(15 a degrees)) and cos, LAND's */
const int16_t rocks_sin[24] = {0,      4240,   8192,   11585,  14189,  15826,  16384,  15826,
                               14189,  11585,  8192,   4240,   0,      -4240,  -8192,  -11585,
                               -14189, -15826, -16384, -15826, -14189, -11585, -8192,  -4240};
const int16_t rocks_cos[24] = {16384,  15826,  14189,  11585,  8192,   4240,   0,      -4240,
                               -8192,  -11585, -14189, -15826, -16384, -15826, -14189, -11585,
                               -8192,  -4240,  0,      4240,   8192,   11585,  14189,  15826};

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint32_t rocks_draw(rocks_game *g, uint32_t n)
{
    g->rng = mix64(g->rng);
    return (uint32_t)(g->rng % n);
}

int rocks_radius(const rocks_game *g, int size)
{
    if (g->full)
        return size == ROCKS_L ? 20 : size == ROCKS_M ? 10 : 5;
    return size == ROCKS_L ? 24 : 12;
}

static int32_t wrap(int32_t v, int32_t m)
{
    v %= m;
    return v < 0 ? v + m : v;
}

/* a difference taken the nearest way round, then in 1/256 px */
static int64_t near(int32_t d, int32_t m)
{
    d = wrap(d, m);
    if (d >= m / 2)
        d -= m;
    return d / 256;
}

static bool within(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int r_px)
{
    int64_t dx = near(x1 - x2, WQ), dy = near(y1 - y2, HQ), r = (int64_t)r_px * 256;
    return dx * dx + dy * dy < r * r;
}

static int32_t speed(rocks_game *g, int size)
{
    if (!g->full)
        return size == ROCKS_L ? 16384 + (int32_t)rocks_draw(g, 16385) : 26214 + (int32_t)rocks_draw(g, 19661);
    int32_t b = 32768 + 3277 * (g->wave - 1 < 10 ? g->wave - 1 : 10);
    int32_t base = size == ROCKS_L ? b : size == ROCKS_M ? 3 * b / 2 : 2 * b;
    return base + (int32_t)rocks_draw(g, (uint32_t)(b / 2));
}

static void heading(rocks_rock *r, int32_t s, int h)
{
    r->vx = (int32_t)((int64_t)s * rocks_sin[h] / 16384);
    r->vy = (int32_t)(-(int64_t)s * rocks_cos[h] / 16384);
}

static void wave_start(rocks_game *g)
{
    int n = g->full ? (3 + g->wave < 6 ? 3 + g->wave : 6) : (g->wave == 1 ? 3 : 4);
    for (int i = 0, slot = 0; i < n; i++, slot++) {
        while (slot < ROCKS_SLOTS && g->r[slot].size != ROCKS_NONE)
            slot++;
        if (slot == ROCKS_SLOTS)
            return;
        rocks_rock *r = &g->r[slot];
        int h = (int)rocks_draw(g, 24);
        int32_t s = speed(g, ROCKS_L);
        r->size = ROCKS_L;
        r->outline = (uint8_t)rocks_draw(g, 4);
        r->x = (int32_t)rocks_draw(g, ROCKS_W) * ROCKS_Q;
        r->y = 0;
        if (within(r->x, r->y, g->x, g->y, 80 + rocks_radius(g, ROCKS_L)))
            r->x = wrap(r->x + 200 * ROCKS_Q, WQ);
        heading(r, s, h);
    }
}

static void ship_start(rocks_game *g)
{
    g->x = 200 * ROCKS_Q;
    g->y = 114 * ROCKS_Q;
    g->vx = g->vy = 0;
    g->a = 0;
    g->shield = g->full ? 40 : 60;
}

void rocks_init(rocks_game *g, bool full, uint32_t seed, uint32_t game)
{
    memset(g, 0, sizeof *g);
    g->rng = mix64((uint64_t)seed ^ mix64(game));
    g->full = full;
    g->ships = 3;
    g->next = full ? 5000 : 0;
    g->wave = 1;
    ship_start(g);
    wave_start(g);
}

void rocks_key(rocks_game *g, int key, bool down)
{
    if (key < 0 || key >= ROCKS_KEYS)
        return;
    g->held = (uint16_t)(down ? g->held | 1u << key : g->held & ~(1u << key));
    if (!down)
        return;
    if (key <= ROCKS_KS1)
        g->latch_l = true;
    else if (key <= ROCKS_KS6)
        g->latch_r = true;
    else if (key <= ROCKS_KS2)
        g->latch_t = true;
    else if (key <= ROCKS_KS5)
        g->latch_f = true;
}

static bool held(const rocks_game *g, int from, int to)
{
    for (int k = from; k <= to; k++)
        if (g->held & 1u << k)
            return true;
    return false;
}

static void score(rocks_game *g, int size)
{
    if (!g->full) {
        g->score++;
        return;
    }
    g->score += size == ROCKS_L ? 10 : size == ROCKS_M ? 25 : 50;
    while (g->score >= g->next) {
        g->next += 5000;
        if (g->ships < 5) {
            g->ships++;
            g->ev_extra++;
        }
    }
}

/* a rock breaks: two of the next size, or nothing */
static void split(rocks_game *g, int i)
{
    rocks_rock *r = &g->r[i];
    g->ev_breaks++;
    g->ev_break_size = r->size;
    g->ev_break_x = r->x;
    g->ev_break_y = r->y;
    int next = r->size + 1;
    if ((!g->full && next > ROCKS_M) || next > ROCKS_S) {
        r->size = ROCKS_NONE;
        return;
    }
    int j = -1;
    for (int k = 0; k < ROCKS_SLOTS && j < 0; k++)
        if (g->r[k].size == ROCKS_NONE)
            j = k;
    int h1 = (int)rocks_draw(g, 24);
    r->size = (uint8_t)next;
    heading(r, speed(g, next), h1);
    r->outline = (uint8_t)rocks_draw(g, 4);
    rocks_rock q = {r->x, r->y, 0, 0, (uint8_t)next, 0};
    heading(&q, speed(g, next), (h1 + 12) % 24);
    q.outline = (uint8_t)rocks_draw(g, 4);
    if (j >= 0)               /* always, in play: a wave's large rocks fit the slots with all their pieces */
        g->r[j] = q;
}

/* a shot's point against the rocks in slot order: the first it is in breaks, and the shot is spent */
static bool hit(rocks_game *g, const rocks_shot *s)
{
    for (int i = 0; i < ROCKS_SLOTS; i++) {
        rocks_rock *r = &g->r[i];
        if (r->size != ROCKS_NONE && within(s->x, s->y, r->x, r->y, rocks_radius(g, r->size))) {
            int size = r->size;
            split(g, i);
            score(g, size);
            return true;
        }
    }
    return false;
}

static bool any_rock(const rocks_game *g)
{
    for (int i = 0; i < ROCKS_SLOTS; i++)
        if (g->r[i].size != ROCKS_NONE)
            return true;
    return false;
}

void rocks_step(rocks_game *g)
{
    g->step++;
    g->ev_shots = g->ev_breaks = g->ev_lost = g->ev_wave = g->ev_extra = 0;
    bool in_play = g->lost == 0;
    bool left = held(g, ROCKS_K4, ROCKS_KS1) || g->latch_l, right = held(g, ROCKS_K6, ROCKS_KS6) || g->latch_r;
    bool thrust = held(g, ROCKS_K7, ROCKS_KS2) || g->latch_t, fire = held(g, ROCKS_KG, ROCKS_KS5) || g->latch_f;
    g->latch_t = g->latch_f = false;
    g->thrusting = false;

    /* 1 the ship */
    if (in_play) {
        if (g->turn > 0)
            g->turn--;
        if (g->turn == 0 && (left || right)) {
            int d = (int)right - (int)left;
            if (d) {
                g->a = (uint8_t)((g->a + 24 + d) % 24);
                g->turn = 2;
            }
            g->latch_l = g->latch_r = false;
        }
        if (thrust) {
            int32_t t = g->full ? 6554 : 3277;
            g->vx += t * rocks_sin[g->a] / 16384;
            g->vy -= t * rocks_cos[g->a] / 16384;
            g->thrusting = true;
        }
        g->vx -= g->vx / 32;
        g->vy -= g->vy / 32;
        int32_t v = g->full ? 4 * ROCKS_Q : 2 * ROCKS_Q;
        g->vx = g->vx > v ? v : g->vx < -v ? -v : g->vx;
        g->vy = g->vy > v ? v : g->vy < -v ? -v : g->vy;
        g->x = wrap(g->x + g->vx, WQ);
        g->y = wrap(g->y + g->vy, HQ);
    } else {
        g->latch_l = g->latch_r = false;
    }

    /* 2 the rocks */
    for (int i = 0; i < ROCKS_SLOTS; i++) {
        rocks_rock *r = &g->r[i];
        if (r->size != ROCKS_NONE) {
            r->x = wrap(r->x + r->vx, WQ);
            r->y = wrap(r->y + r->vy, HQ);
        }
    }

    /* 3 the shots, each in two halves, then one may fire */
    int k = 0;
    for (int i = 0; i < g->nshot; i++) {
        rocks_shot s = g->shot[i];
        s.x = wrap(s.x + s.vx / 2, WQ);
        s.y = wrap(s.y + s.vy / 2, HQ);
        if (hit(g, &s))
            continue;
        s.x = wrap(s.x + (s.vx - s.vx / 2), WQ);
        s.y = wrap(s.y + (s.vy - s.vy / 2), HQ);
        if (hit(g, &s))
            continue;
        if (--s.life == 0)
            continue;
        g->shot[k++] = s;
    }
    g->nshot = (uint8_t)k;
    int limit = g->full ? 4 : 3;
    if (in_play && fire && g->cool == 0 && g->nshot < limit) {
        rocks_shot *s = &g->shot[g->nshot++];
        s->x = wrap(g->x + 32 * rocks_sin[g->a], WQ);
        s->y = wrap(g->y - 32 * rocks_cos[g->a], HQ);
        s->vx = 28 * rocks_sin[g->a] + g->vx;
        s->vy = -28 * rocks_cos[g->a] + g->vy;
        s->life = g->full ? 16 : 20;
        g->cool = g->full ? 3 : 4;
        g->ev_shots++;
    }

    /* 4 the ship against the rocks */
    if (in_play && g->shield == 0) {
        for (int i = 0; i < ROCKS_SLOTS; i++) {
            rocks_rock *r = &g->r[i];
            if (r->size != ROCKS_NONE && within(g->x, g->y, r->x, r->y, rocks_radius(g, r->size) + ROCKS_SHIP_R)) {
                split(g, i);
                g->lost = 20;
                g->ev_lost++;
                if (g->full || g->clock >= ROCKS_FREE)
                    g->ships--;
                break;
            }
        }
    }

    /* 5 the timers */
    if (g->cool > 0)
        g->cool--;
    if (in_play && g->lost == 0 && g->shield > 0)
        g->shield--;
    if (!in_play && --g->lost == 0) {
        if (g->ships == 0) {
            g->over = true;
            return;
        }
        ship_start(g);
    }
    if (g->lost == 0)
        g->clock++;
    if (g->pause > 0 && --g->pause == 0) {
        g->wave++;
        wave_start(g);
        g->ev_wave++;
    }
    if (g->pause == 0 && g->lost == 0 && !any_rock(g)) {
        g->nshot = 0;
        g->pause = 40;
    }
}

int rocks_tick(rocks_game *g, uint32_t ms, void (*each_step)(const rocks_game *g, void *arg), void *arg)
{
    if (g->over)
        return 0;
    g->acc += ms > 100 ? 100 : ms;
    int n = 0;
    while (g->acc >= ROCKS_STEP_MS && !g->over) {
        g->acc -= ROCKS_STEP_MS;
        rocks_step(g);
        n++;
        if (each_step)
            each_step(g, arg);
    }
    return n;
}
