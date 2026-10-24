/* SPDX-License-Identifier: MIT */
/* 3720 under APPS (docs/3720.md): fly through the rocks. The ship, the shots, the rocks and the hits are rocks_logic's;
 * this file turns keys and the clock into its keys and ticks, keeps the bests, sounds the events, and draws, with
 * nothing to read in gentle play. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "kids_draw.h"
#include "rocks_logic.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC '7'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best gentle (4), best full (4), games (4), 2 zeros */
#define AGAIN_MS 500     /* the end takes ENTER after this */
#define BURSTS 6
#define BURST_STEPS 4

typedef struct {
    int16_t x, y;        /* screen px */
    uint8_t left;        /* steps it still shows */
} burst;

typedef struct {
    int state;
    bool full;           /* the mode picked on the title; it opens on gentle */
    uint32_t seed;
    uint32_t best[2], games; /* kept */
    uint32_t now, started, end_at;
    rocks_game g;
    burst b[BURSTS];
    uint16_t sound;      /* the steps of a tick, folded: 1 shot, 2-8 a break by size, 16 lost, 32 wave, 64 extra,
                            128 the shield's end near, 256 the engine's tick */
    uint32_t tune_until; /* while a tune plays, no beep starts */
} rocks_ctx;

/* life: 9 x 7 */
static const uint8_t LIFE[] = {
    0x10, 0x00,  /* ....#.... */
    0x38, 0x00,  /* ...###... */
    0x28, 0x00,  /* ...#.#... */
    0x6c, 0x00,  /* ..##.##.. */
    0xc6, 0x00,  /* .##...##. */
    0xbb, 0x01,  /* ##.###.## */
    0x01, 0x01,  /* #.......# */
};

/* the rocks' outlines: ten points 36 degrees apart, each at a radius of FACTOR / 16 of the rock's */
static const int8_t UNIT[10][2] = {{0, -16}, {9, -13}, {15, -5}, {15, 5},  {9, 13},
                                   {0, 16},  {-9, 13}, {-15, 5}, {-15, -5}, {-9, -13}};
static const uint8_t FACTOR[4][10] = {{16, 13, 15, 12, 16, 14, 13, 16, 12, 15},
                                      {14, 16, 12, 15, 13, 16, 14, 12, 16, 13},
                                      {15, 12, 16, 14, 16, 12, 15, 13, 14, 16},
                                      {13, 15, 14, 16, 12, 15, 16, 13, 15, 12}};
/* the shield's 16 dots, radius 11 */
static const int8_t RING[16][2] = {{0, -11}, {4, -10}, {8, -8}, {10, -4}, {11, 0},  {10, 4},  {8, 8},   {4, 10},
                                   {0, 11},  {-4, 10}, {-8, 8}, {-10, 4}, {-11, 0}, {-10, -4}, {-8, -8}, {-4, -10}};
static const int8_t SPARK[8][2] = {{0, -2}, {2, -1}, {2, 1}, {0, 2}, {-2, 1}, {-2, -1}, {1, -2}, {-1, 2}};

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
    rocks_ctx *c = ctx;
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || save[14] || save[15])
        return;          /* taken whole or not at all */
    c->best[0] = get32(save + 2);
    c->best[1] = get32(save + 6);
    c->games = get32(save + 10);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    rocks_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    put32(save + 2, c->best[0]);
    put32(save + 6, c->best[1]);
    put32(save + 10, c->games);
}

static void app_start_cb(void *ctx)
{
    rocks_ctx *c = ctx;
    c->seed = app_seed();
    c->state = TITLE;
    c->full = false;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void new_game(rocks_ctx *c, uint32_t at)
{
    rocks_init(&c->g, c->full, c->seed, c->games++);
    memset(c->b, 0, sizeof c->b);
    c->tune_until = 0;
    c->started = at;
    c->state = PLAY;
}

static int key_of(int key)
{
    switch (key) {
    case APP_KEY_4: return ROCKS_K4;
    case APP_KEY_SOFT1: return ROCKS_KS1;
    case APP_KEY_6: return ROCKS_K6;
    case APP_KEY_SOFT6: return ROCKS_KS6;
    case APP_KEY_7: return ROCKS_K7;
    case APP_KEY_8: return ROCKS_K8;
    case APP_KEY_9: return ROCKS_K9;
    case APP_KEY_SOFT2: return ROCKS_KS2;
    case APP_KEY_GOLD: return ROCKS_KG;
    case APP_KEY_5: return ROCKS_K5;
    case APP_KEY_TIMES: return ROCKS_KX;
    case APP_KEY_SOFT5: return ROCKS_KS5;
    case APP_KEY_ENTER: return ROCKS_KE;
    case APP_KEY_PLUS: return ROCKS_KP;
    default: return -1;
    }
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    rocks_ctx *c = ctx;
    if (c->state == PLAY) {
        rocks_key(&c->g, key_of(key), down);
        return false; /* the picture changes on the step */
    }
    if (!down)
        return false;
    if (c->state == TITLE && (key == APP_KEY_4 || key == APP_KEY_SOFT1 || key == APP_KEY_6 || key == APP_KEY_SOFT6)) {
        c->full = key == APP_KEY_6 || key == APP_KEY_SOFT6;
        return true;
    }
    if (key == APP_KEY_ENTER && (c->state == TITLE || ms - c->end_at >= AGAIN_MS)) {
        new_game(c, ms);
        return true;
    }
    return false;
}

static int px(int32_t q)
{
    return (int)(q >= 0 ? q / ROCKS_Q : -((-q + ROCKS_Q - 1) / ROCKS_Q)); /* floor */
}

/* after each step: the bursts age and start, the step's sounds are noted, the best kept */
static void each_step(const rocks_game *g, void *arg)
{
    rocks_ctx *c = arg;
    for (int i = 0; i < BURSTS; i++)
        if (c->b[i].left)
            c->b[i].left--;
    if (g->ev_breaks)
        for (int i = 0; i < BURSTS; i++)
            if (!c->b[i].left) {
                c->b[i] = (burst){(int16_t)px(g->ev_break_x), (int16_t)(ROCKS_TOP + px(g->ev_break_y)), BURST_STEPS};
                break;
            }
    if (g->score > c->best[g->full])
        c->best[g->full] = g->score;
    c->sound |= (uint16_t)((g->ev_shots ? 1 : 0) | g->ev_sizes | (g->ev_lost ? 16 : 0)
                           | (g->ev_wave ? 32 : 0) | (g->ev_extra ? 64 : 0)
                           | (g->lost == 0 && g->shield == 20 ? 128 : 0)
                           | (g->thrusting && g->step % 4 == 0 ? 256 : 0));
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    rocks_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    uint32_t dt = c->now - c->started < ms ? c->now - c->started : ms; /* not the time before the game began */
    c->sound = 0;
    rocks_tick(&c->g, dt, each_step, c);
    static const uint16_t lost[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
    static const uint16_t wave[] = {523, 100, 659, 100, 784, 200};
    static const uint16_t extra[] = {784, 80, 1047, 120};
    static const uint16_t shield[] = {1568, 30, 0, 30, 1568, 30};
    if (c->sound & 16) {
        app_tune(lost, 5);
        c->tune_until = c->now + 620;
    } else if (c->sound & 32) {
        app_tune(wave, 3);
        c->tune_until = c->now + 400;
    } else if (c->sound & 64) {
        app_tune(extra, 2);
        c->tune_until = c->now + 200;
    } else if (c->sound & 128) {
        app_tune(shield, 3);
        c->tune_until = c->now + 90;
    } else if (c->now >= c->tune_until) {
        if (c->sound & (2 | 4 | 8))
            app_beep(c->sound & 2 ? 196 : c->sound & 4 ? 262 : 392, 30);
        else if (c->sound & 1)
            app_beep(1500, 8);
        else if (c->sound & 256)
            app_beep(110, 15);
    }
    if (c->g.over) {
        c->state = OVER;
        c->end_at = c->now;
        return 100;
    }
    return ROCKS_STEP_MS - c->g.acc;           /* the next step's time */
}

/* drawing */

static char *number(char *out, uint32_t u)
{
    char d[12];
    int n = 0;
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        *out++ = d[--n];
    *out = '\0';
    return out;
}

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

/* a rock's outline at a screen centre */
static void rock_at(ls027_frame *f, int cx, int cy, int r, int o)
{
    for (int k = 0; k < 10; k++) {
        int j = (k + 1) % 10;
        draw_line(f, cx + UNIT[k][0] * FACTOR[o][k] * r / 256, cy + UNIT[k][1] * FACTOR[o][k] * r / 256,
                  cx + UNIT[j][0] * FACTOR[o][j] * r / 256, cy + UNIT[j][1] * FACTOR[o][j] * r / 256, 1);
    }
}

/* the ship's outline turned to a, at a screen centre; with the flame behind it when lit */
static void ship_at(ls027_frame *f, int32_t x, int32_t y, int a, int dy, bool flame)
{
    static const int8_t hull[4][2] = {{0, -8}, {6, 7}, {0, 3}, {-6, 7}};
    static const int8_t fire[3][2] = {{-3, 6}, {0, 12}, {3, 6}};
    int32_t s = rocks_sin[a], co = rocks_cos[a];
    int p[4][2], q[3][2];
    for (int i = 0; i < 4; i++) {
        p[i][0] = px(x + (hull[i][0] * co - hull[i][1] * s) * 4);
        p[i][1] = px(y + (hull[i][0] * s + hull[i][1] * co) * 4) + dy;
    }
    for (int i = 0; i < 4; i++)
        draw_line(f, p[i][0], p[i][1], p[(i + 1) % 4][0], p[(i + 1) % 4][1], 1);
    if (!flame)
        return;
    for (int i = 0; i < 3; i++) {
        q[i][0] = px(x + (fire[i][0] * co - fire[i][1] * s) * 4);
        q[i][1] = px(y + (fire[i][0] * s + fire[i][1] * co) * 4) + dy;
    }
    draw_line(f, q[0][0], q[0][1], q[1][0], q[1][1], 1);
    draw_line(f, q[1][0], q[1][1], q[2][0], q[2][1], 1);
}

static void draw_play(ls027_frame *f, const rocks_ctx *c)
{
    const rocks_game *g = &c->g;
    char t[16];
    /* each thing near an edge is drawn again across it */
    for (int ox = -ROCKS_W; ox <= ROCKS_W; ox += ROCKS_W)
        for (int oy = -ROCKS_H; oy <= ROCKS_H; oy += ROCKS_H) {
            for (int i = 0; i < ROCKS_SLOTS; i++) {
                const rocks_rock *r = &g->r[i];
                if (r->size == ROCKS_NONE)
                    continue;
                int rr = rocks_radius(g, r->size), cx = px(r->x) + ox, cy = px(r->y) + oy;
                if (cx + rr >= 0 && cx - rr < ROCKS_W && cy + rr >= 0 && cy - rr < ROCKS_H)
                    rock_at(f, cx, ROCKS_TOP + cy, rr, r->outline);
            }
            int sx = px(g->x) + ox, sy = px(g->y) + oy;
            if (sx + 14 < 0 || sx - 14 >= ROCKS_W || sy + 14 < 0 || sy - 14 >= ROCKS_H)
                continue;
            if (g->lost == 0 && !g->over) {
                ship_at(f, g->x + ox * ROCKS_Q, g->y + oy * ROCKS_Q, g->a, ROCKS_TOP, g->thrusting && g->step % 2);
                if (g->shield > 20 || (g->shield && g->shield / 2 % 2 == 0))
                    for (int d = 0; d < 16; d++)
                        draw_pixel(f, sx + RING[d][0], ROCKS_TOP + sy + RING[d][1], 1);
            } else {
                int out = g->over ? 30 : (20 - g->lost) * 3 / 2;   /* the burst's dots fly out, and stay out at the end */
                for (int d = 0; d < 8; d++)
                    draw_fill(f, sx + SPARK[d][0] * out / 2 - 1, ROCKS_TOP + sy + SPARK[d][1] * out / 2 - 1, 2, 2,
                              DRAW_DARK);
            }
        }
    for (int i = 0; i < g->nshot; i++)
        draw_fill(f, px(g->shot[i].x), ROCKS_TOP + px(g->shot[i].y), 2, 2, DRAW_DARK);
    for (int i = 0; i < BURSTS; i++)
        if (c->b[i].left) {
            int out = (BURST_STEPS - c->b[i].left + 1) * 3;
            for (int d = 0; d < 6; d++)
                draw_pixel(f, c->b[i].x + SPARK[d][0] * out / 2, c->b[i].y + SPARK[d][1] * out / 2, 1);
        }

    /* the top line, over anything that flew into it */
    draw_fill(f, 0, 0, LS027_W, ROCKS_TOP, DRAW_LIGHT);
    for (int i = 1; i < g->ships; i++)
        draw_blit(f, 4 + 12 * (i - 1), 2, LIFE, 9, 7, DRAW_OR);
    if (g->full) {
        number(t, g->score);
        draw_text(f, LS027_W - 4 - draw_text_width(t, 1), 2, 1, t, 1);
    } else {
        kids_tally(f, LS027_W - 4 - kids_tally_width(g->score), 2, g->score);
    }
    draw_line(f, 0, ROCKS_TOP - 1, LS027_W - 1, ROCKS_TOP - 1, 1);
}

/* a score as its mode shows it: a tally in gentle, digits in full, centred on x */
static void score_at(ls027_frame *f, int x, int y, bool full, uint32_t v)
{
    char t[16];
    if (full) {
        number(t, v);
        draw_text(f, x - draw_text_width(t, 2) / 2, y, 2, t, 1);
    } else {
        kids_tally(f, x - kids_tally_width(v) / 2, y, v);
    }
}

static void draw_title(ls027_frame *f, const rocks_ctx *c)
{
    centred(f, 10, 5, "3720");
    for (int m = 0; m < 2; m++) {
        int x = m ? 214 : 36;
        kids_choice(f, x, 60, 150, 84, c->full == m);
        ship_at(f, (x + 40) * ROCKS_Q, 124 * ROCKS_Q, 3, 0, false);
        if (m) {
            static const uint8_t at[5][3] = {{70, 82, 0}, {110, 76, 1}, {96, 112, 2}, {130, 124, 3}, {60, 128, 1}};
            for (int i = 0; i < 5; i++)
                rock_at(f, x + at[i][0], at[i][1], 7, at[i][2]);
        } else {
            rock_at(f, x + 100, 96, 24, 0);
        }
        if (c->best[m])
            score_at(f, x + 75, 152, m, c->best[m]);
    }
    int x = 52;
    x += kids_keycap(f, x, 172, "4", 1) + 9;
    kids_arrow(f, x, 178, 0);
    x += 13;
    kids_arrow(f, x, 178, 1);
    x += 9;
    x += kids_keycap(f, x, 172, "6", 1) + 18;
    for (int i = 0; i < 3; i++)
        x += kids_keycap(f, x, 172, (const char *[]){"7", "8", "9"}[i], 1) + 2;
    ship_at(f, (x + 10) * ROCKS_Q, 176 * ROCKS_Q, 0, 0, true);
    x += 30;
    for (int i = 0; i < 3; i++)
        x += kids_keycap(f, x, 172, (const char *[]){"GOLD", "5", "\xd7"}[i], 1) + 2;
    for (int d = 0; d < 3; d++)
        draw_fill(f, x + 4 + 6 * d, 177, 2, 2, DRAW_DARK);
    if (c->now / 500 % 2 == 0)
        kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 206, "ENTER", 1);
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    rocks_ctx *c = ctx;
    draw_clear(f);
    if (c->state == TITLE) {
        draw_title(f, c);
        return;
    }
    draw_play(f, c);
    if (c->state == OVER) {
        draw_fill(f, 80, 56, 240, 112, DRAW_LIGHT);
        draw_frame_rect(f, 80, 56, 240, 112, 1);
        draw_blit(f, 196, 66, LIFE, 9, 7, DRAW_OR);
        score_at(f, 200, 84, c->g.full, c->g.score);
        draw_blit(f, 110, 112, KIDS_STAR, 7, 7, DRAW_OR);   /* the best, beside a star */
        score_at(f, 200, 110, c->g.full, c->best[c->g.full]);
        if (c->now - c->end_at >= AGAIN_MS && c->now / 500 % 2 == 0)
            kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 140, "ENTER", 1);
    }
}

const app_def rocks_app = {"3720", sizeof(rocks_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
