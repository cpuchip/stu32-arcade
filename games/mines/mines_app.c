/* SPDX-License-Identifier: MIT */
/* MINES under APPS (docs/mines.md): clear the field, with no guess ever needed. The field, opening, the chord,
 * flags, dealing and the solver are mines_logic's; this file is the cursor, the clock, the best time, the screens
 * and the sound. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "mines_logic.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC 'M'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best ms (4), wins (4), games (4), 2 zeros */
#define REPEAT_FIRST 300 /* a held cursor key moves again after this, */
#define REPEAT_NEXT 80   /* then every this */
#define AGAIN_MS 500     /* the end takes ENTER after this */
#define HUD_H 20
#define FX 8
#define FY 22
#define CELL 16

typedef struct {
    int state;
    uint64_t seed;
    uint32_t best, wins, games; /* kept; best in ms, 0 for none */
    uint32_t now, t0, t_end;    /* t0: the first open; t_end: the end */
    int cx, cy;
    int held;                   /* the held cursor key, or 0 */
    uint32_t repeat_at;
    bool new_best;
    mines_game g;
} mines_ctx;

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
    mines_ctx *c = ctx;
    /* taken whole or not at all */
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || get32(save + 6) > get32(save + 10))
        return;
    c->best = get32(save + 2);
    c->wins = get32(save + 6);
    c->games = get32(save + 10);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    mines_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    put32(save + 2, c->best);
    put32(save + 6, c->wins);
    put32(save + 10, c->games);
}

static void app_start_cb(void *ctx)
{
    mines_ctx *c = ctx;
    c->seed = (uint64_t)app_seed() << 32;
    c->state = TITLE;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void new_game(mines_ctx *c)
{
    mines_init(&c->g, c->seed, c->games++);
    c->cx = 12;
    c->cy = 6;
    c->held = 0;
    c->new_best = false;
    c->state = PLAY;
}

static void move(mines_ctx *c, int key)
{
    int dx = key == APP_KEY_4 ? -1 : key == APP_KEY_6 ? 1 : 0, dy = key == APP_KEY_8 ? -1 : key == APP_KEY_2 ? 1 : 0;
    int x = c->cx + dx, y = c->cy + dy;
    if (x >= 0 && x < MINES_W)
        c->cx = x;
    if (y >= 0 && y < MINES_H)
        c->cy = y;
}

static void open_here(mines_ctx *c, uint32_t ms)
{
    bool dealt = c->g.dealt;
    uint16_t opened = c->g.opened;
    mines_open(&c->g, c->cy * MINES_W + c->cx);
    if (!dealt && c->g.dealt)
        c->t0 = ms; /* the time starts at the first open */
    if (c->g.state == MINES_PLAY) {
        if (c->g.opened != opened)
            app_beep(440, 10);
        return;
    }
    c->t_end = ms;
    c->state = OVER;
    c->held = 0;
    if (c->g.state == MINES_LOST) {
        static const uint16_t fall[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
        app_tune(fall, 5);
        return;
    }
    static const uint16_t rise[] = {523, 100, 659, 100, 784, 100, 1047, 300};
    app_tune(rise, 4);
    c->wins++;
    uint32_t t = ms - c->t0;
    if (c->best == 0 || t < c->best) {
        c->best = t;
        c->new_best = true;
    }
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    mines_ctx *c = ctx;
    bool cursor = key == APP_KEY_8 || key == APP_KEY_2 || key == APP_KEY_4 || key == APP_KEY_6;
    if (!down) {
        if (cursor && key == c->held)
            c->held = 0;
        return false;
    }
    if (c->state == PLAY) {
        if (cursor) {
            move(c, key);
            c->held = key;
            c->repeat_at = ms + REPEAT_FIRST;
        } else if (key == APP_KEY_5 || key == APP_KEY_ENTER) {
            open_here(c, ms);
        } else if (key == APP_KEY_0) {
            mines_flag(&c->g, c->cy * MINES_W + c->cx);
        } else {
            return false;
        }
        return true;
    }
    if (key == APP_KEY_ENTER && (c->state == TITLE || ms - c->t_end >= AGAIN_MS)) {
        new_game(c);
        return true;
    }
    return false;
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    mines_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    while (c->held && (int32_t)(c->now - c->repeat_at) >= 0) {
        move(c, c->held);
        c->repeat_at += REPEAT_NEXT;
    }
    return 20;
}

/* drawing */

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

/* the clock is the ticks', the first open's time the key's, so a frame drawn on that key is before t0 */
static uint32_t seconds(const mines_ctx *c)
{
    if (!c->g.dealt)
        return 0;
    int32_t t = (int32_t)((c->state == PLAY ? c->now : c->t_end) - c->t0);
    return t > 0 ? (uint32_t)t / 1000 : 0;
}

static void pennant(ls027_frame *f, int x, int y)
{
    draw_line(f, x + 5, y + 3, x + 5, y + 12, 1);
    for (int i = 0; i < 4; i++)
        draw_line(f, x + 6, y + 3 + i, x + 11 - i, y + 3 + i, 1);
    draw_line(f, x + 3, y + 12, x + 8, y + 12, 1);
}

static void draw_cell(ls027_frame *f, const mines_ctx *c, int cell)
{
    const mines_game *g = &c->g;
    int x = FX + cell % MINES_W * CELL, y = FY + cell / MINES_W * CELL;
    uint8_t s = g->cell[cell];
    bool over = c->state == OVER;
    draw_frame_rect(f, x, y, CELL + 1, CELL + 1, 1);
    if (s & MINES_OPEN) {
        if (g->num[cell]) {
            char t[2] = {(char)('0' + g->num[cell]), 0};
            draw_text(f, x + 4, y + 2, 2, t, 1);
        }
        return;
    }
    if (over && g->state == MINES_LOST && (s & MINES_MINE)) {
        bool hit = cell == g->hit;
        if (hit)
            draw_fill(f, x + 1, y + 1, CELL - 1, CELL - 1, DRAW_DARK);
        draw_fill(f, x + 5, y + 5, 7, 7, hit ? DRAW_LIGHT : DRAW_DARK);
        return;
    }
    if (s & MINES_FLAG) {
        pennant(f, x, y);
        if (over && g->state == MINES_LOST) { /* a wrong flag */
            draw_line(f, x + 2, y + 2, x + 14, y + 14, 1);
            draw_line(f, x + 14, y + 2, x + 2, y + 14, 1);
        }
        return;
    }
    draw_fill(f, x + 2, y + 2, CELL - 3, CELL - 3, DRAW_HALF);
}

static void draw_play(ls027_frame *f, const mines_ctx *c)
{
    char t[24];
    bool over = c->state == OVER;
    if (over)
        draw_text(f, 4, 3, 2, c->g.state == MINES_WON ? "CLEAR" : "BOOM", 1);
    else {
        label(t, "MINES ", (int32_t)MINES_N - c->g.flags);
        draw_text(f, 4, 3, 2, t, 1);
    }
    label(t, "TIME ", (int32_t)seconds(c));
    centred(f, 3, 2, t);
    if (c->best) {
        label(t, over && c->new_best ? "NEW BEST " : "BEST ", (int32_t)(c->best / 1000));
        draw_text(f, LS027_W - 4 - draw_text_width(t, 2), 3, 2, t, 1);
    }
    for (int cell = 0; cell < MINES_CELLS; cell++)
        draw_cell(f, c, cell);
    if (c->state == PLAY) {
        int x = FX + c->cx * CELL, y = FY + c->cy * CELL;
        draw_frame_rect(f, x - 1, y - 1, CELL + 3, CELL + 3, 1);
        draw_frame_rect(f, x - 2, y - 2, CELL + 5, CELL + 5, 1);
    }
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    mines_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 40, 5, "MINES");
        centred(f, 106, 2, "8 2 4 6 MOVE  5 OPEN  0 FLAG");
        centred(f, 132, 1, "CLEAR THE FIELD: EVERY BOARD NEEDS NO GUESS");
        centred(f, 164, 2, "ENTER PLAYS");
        if (c->best) {
            label(t, "BEST ", (int32_t)(c->best / 1000));
            centred(f, 196, 2, t);
        }
        return;
    }
    draw_play(f, c); /* the end is told on the top line, so the whole field shows */
    if (c->state == OVER && (int32_t)(c->now - c->t_end) >= AGAIN_MS)
        centred(f, FY + MINES_H * CELL + 2, 1, "ENTER PLAYS");
}

const app_def mines_app = {"MINES", sizeof(mines_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
