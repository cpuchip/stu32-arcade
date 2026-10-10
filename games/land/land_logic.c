/* SPDX-License-Identifier: MIT */
#include "land_logic.h"

#include <string.h>

#define WQ ((int32_t)LAND_W * LAND_Q)
#define VMAX (4 * LAND_Q)
#define TOP (-200 * LAND_Q)

/* round(16384 sin(15 a degrees)) and cos */
const int16_t land_sin[24] = {0,      4240,   8192,   11585,  14189,  15826,  16384,  15826,
                              14189,  11585,  8192,   4240,   0,      -4240,  -8192,  -11585,
                              -14189, -15826, -16384, -15826, -14189, -11585, -8192,  -4240};
const int16_t land_cos[24] = {16384,  15826,  14189,  11585,  8192,   4240,   0,      -4240,
                              -8192,  -11585, -14189, -15826, -16384, -15826, -14189, -11585,
                              -8192,  -4240,  0,      4240,   8192,   11585,  14189,  15826};

/* the feet first; convex, in order */
const int8_t land_outline_px[LAND_OUTLINE][2] = {{-6, 6}, {6, 6}, {6, 1}, {4, -5}, {0, -8}, {-4, -5}, {-6, 1}};

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static int32_t draw(uint64_t *x, uint32_t n)
{
    *x = mix64(*x);
    return (int32_t)(*x % n);
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

void land_make_terrain(land_terrain *t, uint64_t seed, uint32_t round)
{
    static const land_pad kinds[LAND_PADS] = {{0, 24, 0, 5}, {0, 34, 0, 3}, {0, 48, 0, 2}};
    uint64_t x = mix64(seed ^ mix64(round));
    land_pad order[LAND_PADS];
    memcpy(order, kinds, sizeof order);
    for (int i = 2; i >= 1; i--) {
        int j = draw(&x, (uint32_t)i + 1);
        land_pad p = order[i];
        order[i] = order[j];
        order[j] = p;
    }
    for (int k = 0; k < LAND_PADS; k++) {
        int w = order[k].x1; /* the kind's width */
        int x0 = 133 * k + 12 + draw(&x, (uint32_t)(133 - 24 - w));
        t->pad[k].x0 = (int16_t)x0;
        t->pad[k].x1 = (int16_t)(x0 + w);
        t->pad[k].y = (int16_t)(130 + draw(&x, 80));
        t->pad[k].mult = order[k].mult;
    }
    t->n = 0;
    t->v[t->n++] = (land_vert){0, (int16_t)(150 + draw(&x, 70))};
    for (int k = 0; k <= LAND_PADS; k++) {
        int target = k < LAND_PADS ? t->pad[k].x0 : LAND_W;
        while (t->v[t->n - 1].x < target) {
            int cx = t->v[t->n - 1].x, cy = t->v[t->n - 1].y;
            int nx = cx + 12 + draw(&x, 17);
            if (nx > target - 12)
                nx = target;
            int ny = clamp(cy + draw(&x, 61) - 30, 130, 226);
            if (k < LAND_PADS && nx == target)
                ny = t->pad[k].y;
            t->v[t->n++] = (land_vert){(int16_t)nx, (int16_t)ny};
        }
        if (k < LAND_PADS)
            t->v[t->n++] = (land_vert){t->pad[k].x1, t->pad[k].y};
    }
    t->v[t->n - 1].y = t->v[0].y; /* the edges meet */
    t->start_x = (40 + draw(&x, 320)) * LAND_Q;
    int32_t speed = 19661 + draw(&x, 19661);
    t->start_vx = draw(&x, 2) == 0 ? -speed : speed;
}

void land_ship_init(land_ship *s, const land_terrain *t, int32_t fuel)
{
    memset(s, 0, sizeof *s);
    s->x = t->start_x;
    s->y = LAND_START_Y * LAND_Q;
    s->vx = t->start_vx;
    s->fuel = fuel;
}

void land_key(land_ship *s, int key, bool down)
{
    if (down) {
        s->held |= (uint8_t)key;
        s->pressed |= (uint8_t)key;
    } else {
        s->held &= (uint8_t)~key;
    }
}

int32_t land_ground(const land_terrain *t, int32_t xq)
{
    xq %= WQ;
    if (xq < 0)
        xq += WQ;
    int i = 1;
    while (i < t->n - 1 && xq >= t->v[i].x * LAND_Q)
        i++;
    int32_t x0 = t->v[i - 1].x, y0 = t->v[i - 1].y, x1 = t->v[i].x, y1 = t->v[i].y;
    return y0 * LAND_Q + (int32_t)((int64_t)(y1 - y0) * (xq - x0 * LAND_Q) / (x1 - x0));
}

void land_outline(const land_ship *s, int32_t pts[LAND_OUTLINE][2])
{
    int32_t c = land_cos[s->a], sn = land_sin[s->a];
    for (int i = 0; i < LAND_OUTLINE; i++) {
        int32_t hx = land_outline_px[i][0], hy = land_outline_px[i][1];
        pts[i][0] = s->x + (hx * c - hy * sn) * 4;
        pts[i][1] = s->y + (hx * sn + hy * c) * 4;
    }
}

/* a terrain vertex strictly inside the outline, at its x or x +- the screen's width */
static bool vertex_inside(int32_t pts[LAND_OUTLINE][2], const land_terrain *t)
{
    for (int v = 0; v < t->n; v++)
        for (int sh = -1; sh <= 1; sh++) {
            int64_t tx = (int64_t)(t->v[v].x + sh * LAND_W) * LAND_Q, ty = (int64_t)t->v[v].y * LAND_Q;
            int pos = 0, neg = 0;
            for (int i = 0; i < LAND_OUTLINE; i++) {
                const int32_t *a = pts[i], *b = pts[(i + 1) % LAND_OUTLINE];
                int64_t c = (int64_t)(b[0] - a[0]) * (ty - a[1]) - (int64_t)(b[1] - a[1]) * (tx - a[0]);
                pos += c > 0;
                neg += c < 0;
            }
            if (pos == LAND_OUTLINE || neg == LAND_OUTLINE)
                return true;
        }
    return false;
}

enum land_end land_step(land_ship *s, const land_terrain *t, int *mult)
{
    uint8_t on = s->held | s->pressed;
    if (s->timer > 0)
        s->timer--;
    if (s->timer == 0) {
        int left = !!(on & LAND_KEY_LEFT), right = !!(on & LAND_KEY_RIGHT);
        if (right - left) {
            s->a = (uint8_t)((s->a + 24 + right - left) % 24);
            s->timer = LAND_TURN_STEPS;
        }
        if (left || right)
            s->pressed &= (uint8_t)~(LAND_KEY_LEFT | LAND_KEY_RIGHT);
    }
    s->fired = (on & LAND_KEY_THRUST) && s->fuel > 0;
    if (s->fired) {
        s->vx += LAND_A * land_sin[s->a] / 16384;
        s->vy -= LAND_A * land_cos[s->a] / 16384;
        s->fuel--;
    }
    s->pressed &= (uint8_t)~LAND_KEY_THRUST;
    s->vy += LAND_G;
    s->vx = clamp(s->vx, -VMAX, VMAX);
    s->vy = clamp(s->vy, -VMAX, VMAX);
    s->x = (s->x + s->vx) % WQ;
    if (s->x < 0)
        s->x += WQ;
    s->y += s->vy;
    if (s->y < TOP) {
        s->y = TOP;
        s->vy = 0;
    }

    int32_t pts[LAND_OUTLINE][2];
    land_outline(s, pts);
    bool under = false;
    for (int i = 0; i < LAND_OUTLINE; i++)
        under |= pts[i][1] >= land_ground(t, pts[i][0]);
    bool poke = vertex_inside(pts, t);
    if (!under && !poke)
        return LAND_FLYING;
    if (s->a != 0 || s->vy > LAND_VY || s->vy < -LAND_VY || s->vx > LAND_VX || s->vx < -LAND_VX)
        return LAND_CRASHED;
    for (int k = 0; k < LAND_PADS; k++) {
        const land_pad *p = &t->pad[k];
        bool on_pad = true;
        for (int f = 0; f < 2; f++) {
            int32_t fx = pts[f][0] % WQ;
            if (fx < 0)
                fx += WQ;
            on_pad &= fx >= p->x0 * LAND_Q && fx <= p->x1 * LAND_Q;
        }
        if (on_pad) {
            s->y = (p->y - 6) * LAND_Q;
            *mult = p->mult;
            return LAND_LANDED;
        }
    }
    return LAND_CRASHED;
}
