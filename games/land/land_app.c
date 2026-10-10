/* SPDX-License-Identifier: MIT */
/* LAND under APPS (docs/land.md): set the ship down on a pad, upright and slowly. The physics, the contact test and
 * the terrain are land_logic's; this file turns keys and the clock into steps, keeps the score and the fuel, and
 * draws. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "land_logic.h"

enum { TITLE, FLY, ENDED, OVER };
#define SAVE_MAGIC 'L'
#define SAVE_VERSION 1
#define SAVE_BYTES 16   /* magic, version, best (4), landings (4), crashes (4), 2 zeros */
#define TICK_MAX_MS 100 /* a long tick is taken as this, so nothing jumps */
#define ENDED_MS 1500   /* LANDED or CRASHED shows this long */
#define AGAIN_MS 500    /* GAME OVER takes ENTER after this */
#define LOW_FUEL 200
#define HUD_H 22

typedef struct {
    int state;
    uint64_t seed;
    uint32_t best, landings, crashes; /* kept */
    uint32_t score;
    int points;                       /* the last round's: > 0 landed, 0 crashed */
    bool low_beeped;
    uint32_t now, end_at, acc, steps;
    land_terrain t;
    land_ship s;
} land_ctx;

static void put32(uint8_t *b, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        b[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static void app_load_cb(void *ctx, const uint8_t *save, uint32_t n)
{
    land_ctx *c = ctx;
    /* taken whole or not at all */
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION)
        return;
    c->best = get32(save + 2);
    c->landings = get32(save + 6);
    c->crashes = get32(save + 10);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    land_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    put32(save + 2, c->best);
    put32(save + 6, c->landings);
    put32(save + 10, c->crashes);
}

static void app_start_cb(void *ctx)
{
    land_ctx *c = ctx;
    c->seed = (uint64_t)app_seed() << 32;
    c->state = TITLE;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

/* the next round, with the fuel left; a key still held stays held */
static void new_round(land_ctx *c, int32_t fuel)
{
    uint8_t held = c->s.held;
    land_make_terrain(&c->t, c->seed, c->landings + c->crashes);
    land_ship_init(&c->s, &c->t, fuel);
    c->s.held = held;
    c->acc = 0;
    c->state = FLY;
}

static void new_game(land_ctx *c)
{
    c->score = 0;
    c->low_beeped = false;
    new_round(c, LAND_FUEL);
}

static int ship_key(int key)
{
    return key == APP_KEY_4 ? LAND_KEY_LEFT : key == APP_KEY_6 ? LAND_KEY_RIGHT : key == APP_KEY_8 ? LAND_KEY_THRUST : 0;
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    land_ctx *c = ctx;
    (void)ms;
    int k = ship_key(key);
    if (k && (c->state == FLY || !down)) {
        land_key(&c->s, k, down);
        return false;
    }
    if (down && key == APP_KEY_ENTER && (c->state == TITLE || (c->state == OVER && c->now - c->end_at >= AGAIN_MS))) {
        new_game(c);
        return true;
    }
    return false;
}

static void round_end(land_ctx *c, enum land_end e, int mult)
{
    if (e == LAND_LANDED) {
        static const uint16_t up[] = {523, 80, 659, 80, 784, 80, 1047, 160};
        c->points = 50 * mult;
        c->score += (uint32_t)c->points;
        c->landings++;
        app_tune(up, 4);
    } else {
        static const uint16_t fall[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300};
        c->points = 0;
        c->s.fuel = c->s.fuel > LAND_CRASH_COST ? c->s.fuel - LAND_CRASH_COST : 0;
        c->crashes++;
        app_tune(fall, 5);
    }
    c->state = ENDED;
    c->end_at = c->now;
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    land_ctx *c = ctx;
    c->now += ms;
    if (c->state == TITLE)
        return 100;
    if (c->state == ENDED && c->now - c->end_at >= ENDED_MS) {
        /* a round starts on a tick or a key, and its first step is on the next tick */
        if (c->s.fuel > 0) {
            new_round(c, c->s.fuel);
        } else {
            c->state = OVER;
            c->end_at = c->now;
            if (c->score > c->best)
                c->best = c->score;
        }
        return LAND_STEP_MS;
    }
    if (c->state != FLY)
        return LAND_STEP_MS;
    c->acc += ms > TICK_MAX_MS ? TICK_MAX_MS : ms;
    while (c->acc >= LAND_STEP_MS && c->state == FLY) {
        c->acc -= LAND_STEP_MS;
        c->steps++;
        int mult = 0;
        enum land_end e = land_step(&c->s, &c->t, &mult);
        if (!c->low_beeped && c->s.fuel < LOW_FUEL) {
            c->low_beeped = true;
            app_beep(880, 60);
        }
        if (e != LAND_FLYING)
            round_end(c, e, mult);
    }
    return LAND_STEP_MS;
}

/* drawing */

static int px(int32_t q)
{
    return (int)(q >= 0 ? q / LAND_Q : -((-q + LAND_Q - 1) / LAND_Q)); /* floor */
}

static char *number(char *out, int32_t v)
{
    char d[12];
    int n = 0;
    uint32_t u = v < 0 ? (uint32_t)-v : (uint32_t)v;
    if (v < 0)
        *out++ = '-';
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        *out++ = d[--n];
    *out = '\0';
    return out;
}

static void label(char *out, const char *s, int32_t v)
{
    while (*s)
        *out++ = *s++;
    number(out, v);
}

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

/* a point of the ship's frame (px, y down) turned with it, in screen px, shifted by dx px */
static void turned(const land_ship *s, int hx, int hy, int dx, int *x, int *y)
{
    int32_t c = land_cos[s->a], sn = land_sin[s->a];
    *x = px(s->x + (hx * c - hy * sn) * 4) + dx;
    *y = px(s->y + (hx * sn + hy * c) * 4);
}

static void ship_line(ls027_frame *f, const land_ship *s, int dx, int ax, int ay, int bx, int by)
{
    int x0, y0, x1, y1;
    turned(s, ax, ay, dx, &x0, &y0);
    turned(s, bx, by, dx, &x1, &y1);
    draw_line(f, x0, y0, x1, y1, 1);
}

static void draw_ship(ls027_frame *f, const land_ctx *c, int dx)
{
    const land_ship *s = &c->s;
    /* the outline without its base, so the sides are the legs; the deck across */
    for (int i = 1; i < LAND_OUTLINE; i++) {
        int j = (i + 1) % LAND_OUTLINE;
        ship_line(f, s, dx, land_outline_px[i][0], land_outline_px[i][1], land_outline_px[j][0], land_outline_px[j][1]);
    }
    ship_line(f, s, dx, -6, 1, 6, 1);
    ship_line(f, s, dx, -8, 6, -4, 6);
    ship_line(f, s, dx, 4, 6, 8, 6);
    if (c->state == FLY && s->fired && (c->steps & 1)) {
        ship_line(f, s, dx, -2, 2, 0, 11);
        ship_line(f, s, dx, 2, 2, 0, 11);
    }
}

static void draw_play(ls027_frame *f, const land_ctx *c)
{
    char t[24];
    label(t, "SCORE ", (int32_t)c->score);
    draw_text(f, 4, 4, 2, t, 1);
    label(t, "FUEL ", c->s.fuel);
    draw_text(f, 140, 4, 2, t, 1);
    char *e = t;
    *e++ = 'H';
    e = number(e, c->s.vx * 50 / LAND_Q);
    *e++ = ' ';
    *e++ = 'V';
    number(e, c->s.vy * 50 / LAND_Q);
    draw_text(f, LS027_W - 4 - draw_text_width(t, 2), 4, 2, t, 1);
    draw_fill(f, 0, HUD_H, LS027_W, 1, DRAW_DARK);

    const land_terrain *tr = &c->t;
    for (int i = 1; i < tr->n; i++)
        draw_line(f, tr->v[i - 1].x, tr->v[i - 1].y, tr->v[i].x, tr->v[i].y, 1);
    for (int k = 0; k < LAND_PADS; k++) {
        const land_pad *p = &tr->pad[k];
        char m[3] = {(char)0xd7, (char)('0' + p->mult), 0}; /* the font's x sign */
        draw_fill(f, p->x0, p->y, p->x1 - p->x0 + 1, 3, DRAW_DARK);
        draw_text(f, (p->x0 + p->x1 - draw_text_width(m, 1)) / 2, p->y + 6, 1, m, 1);
    }

    int sx = px(c->s.x);
    draw_ship(f, c, 0);
    if (sx < 12)
        draw_ship(f, c, LAND_W);
    if (sx > LAND_W - 12)
        draw_ship(f, c, -LAND_W);
    if (px(c->s.y) < HUD_H - 8) { /* above the top: a mark under the top line */
        draw_line(f, sx - 4, HUD_H + 6, sx, HUD_H + 2, 1);
        draw_line(f, sx, HUD_H + 2, sx + 4, HUD_H + 6, 1);
    }
}

static void box(ls027_frame *f, int y, int h)
{
    draw_fill(f, 80, y, 240, h, DRAW_LIGHT);
    draw_frame_rect(f, 80, y, 240, h, 1);
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    land_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 40, 5, "LAND");
        centred(f, 110, 2, "4 6 TURN   8 THRUST");
        centred(f, 136, 1, "SET IT DOWN ON A PAD, UPRIGHT AND SLOW");
        centred(f, 168, 2, "ENTER PLAYS");
        if (c->best > 0) {
            label(t, "BEST ", (int32_t)c->best);
            centred(f, 200, 2, t);
        }
        return;
    }
    draw_play(f, c);
    if (c->state == ENDED) {
        box(f, 70, 50);
        if (c->points > 0) {
            centred(f, 80, 3, "LANDED");
            label(t, "+", c->points);
            centred(f, 106, 1, t);
        } else {
            centred(f, 80, 3, "CRASHED");
            centred(f, 106, 1, c->s.fuel > 0 ? "-200 FUEL" : "NO FUEL LEFT");
        }
    } else if (c->state == OVER) {
        box(f, 56, 104);
        centred(f, 66, 3, "GAME OVER");
        label(t, "SCORE ", (int32_t)c->score);
        centred(f, 96, 2, t);
        label(t, "BEST ", (int32_t)c->best);
        centred(f, 116, 2, t);
        if (c->now - c->end_at >= AGAIN_MS)
            centred(f, 142, 1, "ENTER PLAYS");
    }
}

const app_def land_app = {"LAND", sizeof(land_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                          app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
