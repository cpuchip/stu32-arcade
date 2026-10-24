/* SPDX-License-Identifier: MIT */
/* MOLES under APPS (docs/moles.md): bonk the mole. The holes, the moles and the bonks are moles_logic's; this file
 * turns keys and the clock into bonks and ticks, keeps the bests and the digits option, sounds the events, and draws,
 * with nothing to read in play. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "kids_draw.h"
#include "moles_logic.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC 'M'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best gentle (4), best full (4), games (4), the digits option, 1 zero */
#define AGAIN_MS 500     /* the end takes ENTER after this */
#define PUFF_STEPS 4
#define CELL_W 133
#define CELL_H 76
#define TOP 12
#define MOLE_W 44
#define MOLE_H 40

typedef struct {
    int state;
    bool full;           /* the mode picked on the title; it opens on gentle */
    bool digits;         /* kept: the digit on a mole that is up */
    uint32_t seed;
    uint32_t best[2], games; /* kept */
    uint32_t now, started, end_at;
    moles_game g;
    uint8_t puff[MOLES_HOLES];  /* a wrong key's dust, steps left */
    uint8_t sound;       /* the steps of a tick, folded: 1 rise, 2 bonk, 4 wrong, 8 ten, 16 gone, 32 end */
    uint32_t tune_until; /* while a tune plays, no beep starts */
} moles_ctx;

/* mole: 44 x 40 */
static const uint8_t MOLE[] = {
    0x00, 0x80, 0xff, 0x1f, 0x00, 0x00,  /* ...............##############............... */
    0x00, 0xe0, 0x03, 0x7c, 0x00, 0x00,  /* .............#####........#####............. */
    0x00, 0x78, 0x00, 0xe0, 0x01, 0x00,  /* ...........####..............####........... */
    0x00, 0x1c, 0x00, 0x80, 0x03, 0x00,  /* ..........###..................###.......... */
    0x00, 0x0e, 0x00, 0x00, 0x07, 0x00,  /* .........###....................###......... */
    0x00, 0x07, 0x00, 0x00, 0x0e, 0x00,  /* ........###......................###........ */
    0x80, 0x03, 0x00, 0x00, 0x1c, 0x00,  /* .......###........................###....... */
    0xc0, 0x01, 0x00, 0x00, 0x38, 0x00,  /* ......###..........................###...... */
    0xc0, 0x00, 0x00, 0x00, 0x30, 0x00,  /* ......##............................##...... */
    0x60, 0xc0, 0x01, 0x38, 0x60, 0x00,  /* .....##.......###..........###.......##..... */
    0x60, 0xc0, 0x01, 0x38, 0x60, 0x00,  /* .....##.......###..........###.......##..... */
    0x30, 0xc0, 0x01, 0x38, 0xc0, 0x00,  /* ....##........###..........###........##.... */
    0x30, 0x00, 0x00, 0x00, 0xc0, 0x00,  /* ....##................................##.... */
    0x18, 0x00, 0xf0, 0x00, 0x80, 0x01,  /* ...##...............####...............##... */
    0x18, 0x00, 0xf8, 0x01, 0x80, 0x01,  /* ...##..............######..............##... */
    0x08, 0x00, 0xfc, 0x03, 0x00, 0x01,  /* ...#..............########..............#... */
    0x0c, 0x00, 0xfc, 0x03, 0x00, 0x03,  /* ..##..............########..............##.. */
    0x0c, 0x00, 0xf8, 0x01, 0x00, 0x03,  /* ..##...............######...............##.. */
    0x0c, 0x00, 0xf0, 0x00, 0x00, 0x03,  /* ..##................####................##.. */
    0x0c, 0x00, 0x94, 0x02, 0x00, 0x03,  /* ..##..............#.#..#.#..............##.. */
    0x04, 0x00, 0x94, 0x02, 0x00, 0x02,  /* ..#...............#.#..#.#...............#.. */
    0x04, 0x00, 0x94, 0x02, 0x00, 0x02,  /* ..#...............#.#..#.#...............#.. */
    0x04, 0x00, 0x9c, 0x03, 0x00, 0x02,  /* ..#...............###..###...............#.. */
    0x04, 0x00, 0x00, 0x00, 0x00, 0x02,  /* ..#......................................#.. */
    0x04, 0x00, 0xfe, 0x07, 0x00, 0x02,  /* ..#..............##########..............#.. */
    0xac, 0x80, 0x0f, 0x1f, 0x50, 0x03,  /* ..##.#.#.......#####....#####.......#.#.##.. */
    0x5c, 0xc0, 0x01, 0x38, 0xa0, 0x03,  /* ..###.#.......###..........###.......#.###.. */
    0x0c, 0xe0, 0x00, 0x70, 0x00, 0x03,  /* ..##.........###............###.........##.. */
    0x0c, 0x30, 0x00, 0xc0, 0x00, 0x03,  /* ..##........##................##........##.. */
    0x0c, 0x38, 0x00, 0xc0, 0x01, 0x03,  /* ..##.......###................###.......##.. */
    0x08, 0x18, 0x00, 0x80, 0x01, 0x01,  /* ...#.......##..................##.......#... */
    0x18, 0x18, 0x00, 0x80, 0x81, 0x01,  /* ...##......##..................##......##... */
    0x18, 0x18, 0x00, 0x80, 0x81, 0x01,  /* ...##......##..................##......##... */
    0x30, 0x18, 0x00, 0x80, 0xc1, 0x00,  /* ....##.....##..................##.....##.... */
    0x30, 0x18, 0x00, 0x80, 0xc1, 0x00,  /* ....##.....##..................##.....##.... */
    0x60, 0x18, 0x00, 0x80, 0x61, 0x00,  /* .....##....##..................##....##..... */
    0x60, 0x38, 0x00, 0xc0, 0x61, 0x00,  /* .....##....###................###....##..... */
    0xc0, 0x30, 0x00, 0xc0, 0x30, 0x00,  /* ......##....##................##....##...... */
    0xc0, 0xe1, 0x00, 0x70, 0x38, 0x00,  /* ......###....###............###....###...... */
    0x80, 0xc3, 0x01, 0x38, 0x1c, 0x00,  /* .......###....###..........###....###....... */
};
/* life: 9 x 7 */
static const uint8_t LIFE[] = {
    0x7c, 0x00,  /* ..#####.. */
    0xaa, 0x00,  /* .#.#.#.#. */
    0x11, 0x01,  /* #...#...# */
    0x39, 0x01,  /* #..###..# */
    0x01, 0x01,  /* #.......# */
    0xaa, 0x00,  /* .#.#.#.#. */
    0x7c, 0x00,  /* ..#####.. */
};

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
    moles_ctx *c = ctx;
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || save[14] > 1 || save[15])
        return;          /* taken whole or not at all */
    c->best[0] = get32(save + 2);
    c->best[1] = get32(save + 6);
    c->games = get32(save + 10);
    c->digits = save[14];
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    moles_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    put32(save + 2, c->best[0]);
    put32(save + 6, c->best[1]);
    put32(save + 10, c->games);
    save[14] = c->digits;
}

static void app_start_cb(void *ctx)
{
    moles_ctx *c = ctx;
    c->seed = app_seed();
    c->state = TITLE;
    c->full = false;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void new_game(moles_ctx *c, uint32_t at)
{
    moles_init(&c->g, c->full, c->seed, c->games++);
    memset(c->puff, 0, sizeof c->puff);
    c->tune_until = 0;
    c->started = at;
    c->state = PLAY;
}

static int digit_of(int key)
{
    switch (key) {
    case APP_KEY_1: return 1;
    case APP_KEY_2: return 2;
    case APP_KEY_3: return 3;
    case APP_KEY_4: return 4;
    case APP_KEY_5: return 5;
    case APP_KEY_6: return 6;
    case APP_KEY_7: return 7;
    case APP_KEY_8: return 8;
    case APP_KEY_9: return 9;
    default: return 0;
    }
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    moles_ctx *c = ctx;
    if (c->state == PLAY) {
        moles_key(&c->g, moles_hole_of(digit_of(key)), down);
        return false; /* the picture changes on the step */
    }
    if (!down)
        return false;
    if (c->state == TITLE && (key == APP_KEY_4 || key == APP_KEY_SOFT1 || key == APP_KEY_6 || key == APP_KEY_SOFT6)) {
        c->full = key == APP_KEY_6 || key == APP_KEY_SOFT6;
        return true;
    }
    if (c->state == TITLE && key == APP_KEY_0) {
        c->digits = !c->digits;
        return true;
    }
    if (key == APP_KEY_ENTER && (c->state == TITLE || ms - c->end_at >= AGAIN_MS)) {
        new_game(c, ms);
        return true;
    }
    return false;
}

/* after each step: the dust ages and starts, the step's sounds are noted, the best kept */
static void each_step(const moles_game *g, void *arg)
{
    moles_ctx *c = arg;
    for (int i = 0; i < MOLES_HOLES; i++) {
        if (c->puff[i])
            c->puff[i]--;
        if (g->ev_wrong_at & 1u << i)
            c->puff[i] = PUFF_STEPS;
    }
    if (g->score > c->best[g->full])
        c->best[g->full] = g->score;
    c->sound |= (uint8_t)((g->ev_rises ? 1 : 0) | (g->ev_bonks ? 2 : 0) | (g->ev_wrong ? 4 : 0) | (g->ev_ten ? 8 : 0)
                          | (g->ev_gone ? 16 : 0) | (g->over ? 32 : 0));
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    moles_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    uint32_t dt = c->now - c->started < ms ? c->now - c->started : ms; /* not the time before the game began */
    c->sound = 0;
    moles_tick(&c->g, dt, each_step, c);
    static const uint16_t end[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
    static const uint16_t gone[] = {330, 100, 262, 160};
    static const uint16_t ten[] = {523, 100, 659, 100, 784, 200};
    static const uint16_t bonk[] = {220, 30, 1320, 15};
    static const uint16_t pop[] = {660, 30, 880, 30};
    if (c->sound & 32) {
        app_tune(end, 5);
        c->tune_until = c->now + 620;
    } else if (c->sound & 16) {
        app_tune(gone, 2);
        c->tune_until = c->now + 260;
    } else if (c->sound & 8) {
        app_tune(ten, 3);
        c->tune_until = c->now + 400;
    } else if (c->sound & 2) {
        app_tune(bonk, 2);
        c->tune_until = c->now + 45;
    } else if (c->sound & 1) {
        app_tune(pop, 2);
        c->tune_until = c->now + 60;
    } else if (c->sound & 4 && c->now >= c->tune_until) {
        app_beep(150, 20);
    }
    if (c->g.over) {
        c->state = OVER;
        c->end_at = c->now;
        return 100;
    }
    return MOLES_STEP_MS - c->g.acc;           /* the next step's time */
}

/* drawing */

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

/* a hole: a flat oval, its front lip drawn again over a mole */
static void hole(ls027_frame *f, int cx, int cy, int front_only)
{
    static const int8_t p[13][2] = {{28, 0}, {24, 3}, {14, 5}, {0, 5}, {-14, 5}, {-24, 3}, {-28, 0},
                                    {-24, -3}, {-14, -5}, {0, -5}, {14, -5}, {24, -3}, {28, 0}};
    for (int i = 0; i < 12; i++)
        if (!front_only || i < 6)
            draw_line(f, cx + p[i][0], cy + p[i][1], cx + p[i + 1][0], cy + p[i + 1][1], 1);
}

/* the top `rows` of a mole standing in the hole at (cx, cy) */
static void mole(ls027_frame *f, int cx, int cy, int rows)
{
    if (rows <= 0)
        return;
    draw_fill(f, cx - MOLE_W / 2, cy - rows, MOLE_W, rows, DRAW_LIGHT);
    draw_blit(f, cx - MOLE_W / 2, cy - rows, MOLE, MOLE_W, rows, DRAW_OR);
}

static void draw_holes(ls027_frame *f, const moles_ctx *c)
{
    const moles_game *g = &c->g;
    static const int8_t digit[MOLES_HOLES] = {7, 8, 9, 4, 5, 6, 1, 2, 3};
    for (int i = 0; i < MOLES_HOLES; i++) {
        int cx = (i % 3) * CELL_W + CELL_W / 2, cy = TOP + (i / 3) * CELL_H + 68;
        const moles_hole *h = &g->h[i];
        int rows = h->state == MOLES_RISE   ? 10 * ((int)h->n + 1)
                   : h->state == MOLES_SINK ? MOLE_H - 10 * ((int)h->n + 1)
                   : h->state == MOLES_EMPTY ? 0
                                             : MOLE_H;
        hole(f, cx, cy, 0);
        mole(f, cx, cy, rows);
        if (rows)
            hole(f, cx, cy, 1);
        if (h->state == MOLES_UP && c->digits) {
            char d[2] = {(char)('0' + digit[i]), 0};
            draw_fill(f, cx - 7, cy - 17, 14, 17, DRAW_LIGHT);
            draw_text(f, cx - draw_text_width(d, 2) / 2, cy - 15, 2, d, 1);
        }
        if (h->state == MOLES_BONK)                 /* dizzy: three stars around the head */
            for (int s = 0; s < 3; s++) {
                static const int8_t at[6][2] = {{-14, -44}, {0, -48}, {14, -44}, {10, -50}, {-10, -50}, {0, -42}};
                int k = (s * 2 + (int)h->n) % 6;
                draw_blit(f, cx + at[k][0] - 3, cy + at[k][1], KIDS_STAR, 7, 7, DRAW_OR);
            }
        if (c->puff[i])
            for (int d = 0; d < 5; d++)
                draw_fill(f, cx - 16 + 8 * d, cy - 8 - (PUFF_STEPS - c->puff[i]) * 3 - (d & 1) * 3, 2, 2, DRAW_DARK);
    }
}

static void top_line(ls027_frame *f, const moles_game *g)
{
    draw_fill(f, 0, 0, LS027_W, TOP, DRAW_LIGHT);
    for (int i = 0; i < g->lives; i++)
        draw_blit(f, 4 + 12 * i, 2, LIFE, 9, 7, DRAW_OR);
    kids_tally(f, LS027_W - 4 - kids_tally_width(g->score), 2, g->score);
    draw_line(f, 0, TOP - 1, LS027_W - 1, TOP - 1, 1);
}

static void draw_title(ls027_frame *f, const moles_ctx *c)
{
    centred(f, 10, 5, "MOLES");
    for (int m = 0; m < 2; m++) {
        int x = m ? 214 : 36;
        kids_choice(f, x, 60, 150, 84, c->full == m);
        if (m) {
            hole(f, x + 40, 132, 0);
            mole(f, x + 40, 132, MOLE_H);
            hole(f, x + 40, 132, 1);
            hole(f, x + 110, 132, 0);
            mole(f, x + 110, 132, MOLE_H / 2);
            hole(f, x + 110, 132, 1);
            for (int i = 0; i < 3; i++)
                draw_blit(f, x + 6 + 12 * i, 64, LIFE, 9, 7, DRAW_OR);
        } else {
            hole(f, x + 75, 132, 0);
            mole(f, x + 75, 132, MOLE_H);
            hole(f, x + 75, 132, 1);
        }
        if (c->best[m])
            kids_tally(f, x + 75 - kids_tally_width(c->best[m]) / 2, 152, c->best[m]);
    }
    int x = 36 + kids_keycap(f, 36, 176, "0", 1) + 8;     /* the digits option */
    draw_blit(f, x, 179, LIFE, 9, 7, DRAW_OR);
    if (c->digits)
        draw_text(f, x + 13, 177, 1, "7", 1);
    if (c->now / 500 % 2 == 0)
        kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 206, "ENTER", 1);
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    moles_ctx *c = ctx;
    draw_clear(f);
    if (c->state == TITLE) {
        draw_title(f, c);
        return;
    }
    draw_holes(f, c);
    top_line(f, &c->g);
    if (c->state == OVER) {
        uint32_t best = c->best[c->g.full];
        draw_fill(f, 80, 56, 240, 112, DRAW_LIGHT);
        draw_frame_rect(f, 80, 56, 240, 112, 1);
        draw_blit(f, 196, 68, LIFE, 9, 7, DRAW_OR);
        kids_tally(f, 200 - kids_tally_width(c->g.score) / 2, 88, c->g.score);
        draw_blit(f, 120, 112, KIDS_STAR, 7, 7, DRAW_OR);   /* the best, beside a star */
        kids_tally(f, 200 - kids_tally_width(best) / 2, 112, best);
        if (c->now - c->end_at >= AGAIN_MS && c->now / 500 % 2 == 0)
            kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 140, "ENTER", 1);
    }
}

const app_def moles_app = {"MOLES", sizeof(moles_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
