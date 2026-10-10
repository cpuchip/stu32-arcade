/* SPDX-License-Identifier: MIT */
/* MINES under APPS (docs/mines.md): clear the field, with no guess ever needed. The field, opening, the chord,
 * flags, dealing and the solver are mines_logic's; this file is the cursor, the clock, the settings, the records,
 * the screens and the sound. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "mines_logic.h"

enum { TITLE, SETTINGS, DEALING, PLAY, OVER };
#define SAVE_MAGIC 'M'
#define SAVE_VERSION 2
#define SAVE_BYTES 80    /* magic, version, settings, six records of best ms, wins, games (u32 each), 5 zeros */
#define RECORDS 6        /* each board's no-guess and classic */
#define SET_CLASSIC 4    /* the settings byte: bits 0-1 the board, then these */
#define SET_QUIET 8
#define REPEAT_FIRST 300 /* a held cursor key moves again after this, */
#define REPEAT_NEXT 80   /* then every this */
#define AGAIN_MS 500     /* the end takes ENTER after this */
#define DEAL_STEP 4      /* attempts a tick while dealing */
#define FY 22

typedef struct {
    uint32_t best, wins, games; /* best in ms, 0 for none */
} record;

typedef struct {
    int state;
    uint64_t seed;
    int board;                  /* the settings, kept */
    bool classic, quiet;
    record rec[RECORDS];        /* kept */
    uint32_t now, t0, t_end;    /* t0: the deal's tick; t_end: the end */
    int cx, cy;
    int held;                   /* the held cursor key, or 0 */
    uint32_t repeat_at;
    int row;                    /* the settings page's row */
    bool new_best, loaded;      /* loaded: a save was taken */
    mines_game g;
} mines_ctx;

/* each board's cell, field origin and number scale (docs/mines.md, the boards) */
static const struct {
    uint8_t cell, fx, fy, scale;
    const char *name, *size;
} LOOK[MINES_BOARDS] = {
    {22, 101, 30, 2, "EASY", "9 X 9, 10 MINES"},
    {13, 96, FY, 1, "MEDIUM", "16 X 16, 40 MINES"},
    {13, 5, FY, 1, "HARD", "30 X 16, 99 MINES"},
};

static record *rec(mines_ctx *c)
{
    return &c->rec[c->board * 2 + c->classic];
}

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
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || (save[2] & 3) == 3 || (save[2] & 0xf0))
        return;
    for (int i = 0; i < RECORDS; i++)
        if (get32(save + 3 + 12 * i + 4) > get32(save + 3 + 12 * i + 8))
            return;
    c->loaded = true;
    c->board = save[2] & 3;
    c->classic = save[2] & SET_CLASSIC;
    c->quiet = save[2] & SET_QUIET;
    for (int i = 0; i < RECORDS; i++) {
        const uint8_t *r = save + 3 + 12 * i;
        c->rec[i] = (record){get32(r), get32(r + 4), get32(r + 8)};
    }
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    mines_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    save[2] = (uint8_t)(c->board | (c->classic ? SET_CLASSIC : 0) | (c->quiet ? SET_QUIET : 0));
    for (int i = 0; i < RECORDS; i++) {
        uint8_t *r = save + 3 + 12 * i;
        put32(r, c->rec[i].best);
        put32(r + 4, c->rec[i].wins);
        put32(r + 8, c->rec[i].games);
    }
}

static void app_start_cb(void *ctx)
{
    mines_ctx *c = ctx;
    c->seed = (uint64_t)app_seed() << 32;
    c->state = TITLE;
    if (!c->loaded)
        c->board = MINES_MEDIUM; /* no save taken: the defaults */
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void beep(const mines_ctx *c, uint16_t hz, uint16_t ms)
{
    if (!c->quiet)
        app_beep(hz, ms);
}

static void tune(const mines_ctx *c, const uint16_t *notes, uint8_t n)
{
    if (!c->quiet)
        app_tune(notes, n);
}

static void new_game(mines_ctx *c)
{
    uint32_t game = 0;
    for (int i = 0; i < RECORDS; i++)
        game += c->rec[i].games; /* the games ever started, on any board */
    mines_init(&c->g, c->seed, game, c->board, c->classic);
    rec(c)->games++;
    c->cx = c->g.w / 2;
    c->cy = c->g.h / 2;
    c->held = 0;
    c->new_best = false;
    c->state = PLAY;
}

static void move(mines_ctx *c, int key)
{
    int dx = key == APP_KEY_4 ? -1 : key == APP_KEY_6 ? 1 : 0, dy = key == APP_KEY_8 ? -1 : key == APP_KEY_2 ? 1 : 0;
    int x = c->cx + dx, y = c->cy + dy;
    if (x >= 0 && x < c->g.w)
        c->cx = x;
    if (y >= 0 && y < c->g.h)
        c->cy = y;
}

/* after an open at time ms: the tick for it, or the end */
static void opened(mines_ctx *c, uint16_t before, uint32_t ms)
{
    if (c->g.state == MINES_PLAY) {
        if (c->g.opened != before)
            beep(c, 440, 10);
        return;
    }
    c->t_end = ms;
    c->state = OVER;
    c->held = 0;
    if (c->g.state == MINES_LOST) {
        static const uint16_t fall[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
        tune(c, fall, 5);
        return;
    }
    static const uint16_t rise[] = {523, 100, 659, 100, 784, 100, 1047, 300};
    tune(c, rise, 4);
    record *r = rec(c);
    r->wins++;
    uint32_t t = ms - c->t0;
    if (t == 0)
        t = 1; /* a first open that clears the field: 0 is kept for no best */
    if (r->best == 0 || t < r->best) {
        r->best = t;
        c->new_best = true;
    }
}

static void open_here(mines_ctx *c, uint32_t ms)
{
    int cell = c->cy * c->g.w + c->cx;
    if (!c->g.dealt) {
        if (mines_deal_start(&c->g, cell)) { /* the deal runs on the ticks, so no key waits on it */
            c->state = DEALING;
            c->held = 0;
        }
        return;
    }
    uint16_t before = c->g.opened;
    mines_open(&c->g, cell);
    opened(c, before, ms);
}

static void setting(mines_ctx *c, int key)
{
    if (c->row == 0) {
        if (key == APP_KEY_4 && c->board > 0)
            c->board--;
        else if (key == APP_KEY_6 && c->board < MINES_BOARDS - 1)
            c->board++;
        else if (key == APP_KEY_5)
            c->board = (c->board + 1) % MINES_BOARDS;
    } else if (c->row == 1) {
        c->classic = !c->classic;
    } else {
        c->quiet = !c->quiet;
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
    switch (c->state) {
    case PLAY:
        if (cursor) {
            move(c, key);
            c->held = key;
            c->repeat_at = ms + REPEAT_FIRST;
        } else if (key == APP_KEY_5 || key == APP_KEY_ENTER) {
            open_here(c, ms);
        } else if (key == APP_KEY_0) {
            mines_flag(&c->g, c->cy * c->g.w + c->cx);
        } else {
            return false;
        }
        return true;
    case SETTINGS:
        if (key == APP_KEY_8 && c->row > 0)
            c->row--;
        else if (key == APP_KEY_2 && c->row < 2)
            c->row++;
        else if (key == APP_KEY_4 || key == APP_KEY_6 || key == APP_KEY_5)
            setting(c, key);
        else if (key == APP_KEY_ENTER)
            c->state = TITLE;
        else
            return false;
        return true;
    case DEALING:
        return false;
    default: /* TITLE, OVER */
        if (c->state == OVER && ms - c->t_end < AGAIN_MS)
            return false;
        if (key == APP_KEY_ENTER) {
            new_game(c);
            return true;
        }
        if (key == APP_KEY_0) {
            c->state = SETTINGS;
            c->row = 0;
            return true;
        }
        return false;
    }
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    mines_ctx *c = ctx;
    c->now += ms;
    if (c->state == DEALING) {
        int f = c->g.deal_f;
        if (!mines_deal_step(&c->g, DEAL_STEP))
            return 1;
        c->t0 = c->now; /* the time starts as the board is dealt, so the deal is never on the clock */
        c->state = PLAY;
        mines_open(&c->g, f);
        opened(c, 0, c->now);
        return 20;
    }
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

static void join(char *out, const char *a, const char *b, const char *c)
{
    out[0] = '\0';
    strcat(strcat(strcat(out, a), b), c);
}

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

/* whole seconds on the clock: the ticks' while playing, the end's after; 0 before the deal. t0 is a tick's time and
 * the end a later key's, so neither is ever before it */
static uint32_t seconds(const mines_ctx *c)
{
    if (!c->g.dealt || c->state == DEALING)
        return 0;
    return ((c->state == PLAY ? c->now : c->t_end) - c->t0) / 1000;
}

static int px(int v, int s) /* revision 1's 16 px drawing, scaled to a cell of s */
{
    return v * s / 16;
}

static void pennant(ls027_frame *f, int x, int y, int s)
{
    draw_line(f, x + px(5, s), y + px(3, s), x + px(5, s), y + px(12, s), 1);
    for (int i = 0; i < px(4, s); i++)
        draw_line(f, x + px(6, s), y + px(3, s) + i, x + px(11, s) - i, y + px(3, s) + i, 1);
    draw_line(f, x + px(3, s), y + px(12, s), x + px(8, s), y + px(12, s), 1);
}

static void draw_cell(ls027_frame *f, const mines_ctx *c, int cell)
{
    const mines_game *g = &c->g;
    int s = LOOK[c->board].cell, sc = LOOK[c->board].scale;
    int x = LOOK[c->board].fx + cell % g->w * s, y = LOOK[c->board].fy + cell / g->w * s;
    uint8_t st = g->cell[cell];
    bool over = c->state == OVER;
    draw_frame_rect(f, x, y, s + 1, s + 1, 1);
    if (st & MINES_OPEN) {
        if (g->num[cell]) {
            char t[2] = {(char)('0' + g->num[cell]), 0};
            draw_text(f, x + (s + 1 - 5 * sc) / 2, y + (s + 1 - 7 * sc) / 2, sc, t, 1);
        }
        return;
    }
    if (over && g->state == MINES_LOST && (st & MINES_MINE)) {
        bool hit = cell == g->hit;
        int d = px(7, s), o = (s + 1 - d) / 2;
        if (hit)
            draw_fill(f, x + 1, y + 1, s - 1, s - 1, DRAW_DARK);
        draw_fill(f, x + o, y + o, d, d, hit ? DRAW_LIGHT : DRAW_DARK);
        return;
    }
    if (st & MINES_FLAG) {
        pennant(f, x, y, s);
        if (over && g->state == MINES_LOST) { /* a wrong flag */
            draw_line(f, x + 2, y + 2, x + s - 2, y + s - 2, 1);
            draw_line(f, x + s - 2, y + 2, x + 2, y + s - 2, 1);
        }
        return;
    }
    draw_fill(f, x + 2, y + 2, s - 3, s - 3, DRAW_HALF);
}

static void draw_play(ls027_frame *f, const mines_ctx *c)
{
    char t[24];
    bool over = c->state == OVER;
    if (c->state == DEALING)
        draw_text(f, 4, 3, 2, "DEALING", 1);
    else if (over)
        draw_text(f, 4, 3, 2, c->g.state == MINES_WON ? "CLEAR" : "BOOM", 1);
    else {
        label(t, "MINES ", (int32_t)c->g.n - c->g.flags);
        draw_text(f, 4, 3, 2, t, 1);
    }
    label(t, "TIME ", (int32_t)seconds(c));
    centred(f, 3, 2, t);
    const record *r = &c->rec[c->board * 2 + c->classic];
    if (r->best) {
        label(t, over && c->new_best ? "NEW BEST " : "BEST ", (int32_t)(r->best / 1000));
        draw_text(f, LS027_W - 4 - draw_text_width(t, 2), 3, 2, t, 1);
    }
    for (int cell = 0; cell < c->g.cells; cell++)
        draw_cell(f, c, cell);
    if (c->state != OVER) {
        int s = LOOK[c->board].cell;
        int x = LOOK[c->board].fx + c->cx * s, y = LOOK[c->board].fy + c->cy * s;
        draw_frame_rect(f, x - 1, y - 1, s + 3, s + 3, 1);
        draw_frame_rect(f, x - 2, y - 2, s + 5, s + 5, 1);
    }
}

static void box(ls027_frame *f, int x, int y, bool on)
{
    draw_frame_rect(f, x, y, 15, 15, 1);
    if (on) {
        draw_line(f, x + 3, y + 3, x + 11, y + 11, 1);
        draw_line(f, x + 11, y + 3, x + 3, y + 11, 1);
    }
}

static void draw_settings(ls027_frame *f, const mines_ctx *c)
{
    static const char *const rows[] = {"BOARD", "CLASSIC RULES", "SOUND EFFECTS"};
    centred(f, 16, 3, "SETTINGS");
    for (int i = 0; i < 3; i++) {
        int y = 70 + 46 * i;
        if (i == c->row)
            draw_text(f, 16, y, 2, ">", 1);
        draw_text(f, 40, y, 2, rows[i], 1);
    }
    draw_text(f, 230, 70, 2, LOOK[c->board].name, 1);
    draw_text(f, 230, 92, 1, LOOK[c->board].size, 1);
    box(f, 230, 116, c->classic);
    box(f, 230, 162, !c->quiet);
    centred(f, 222, 1, "8 2 ROW   4 6 5 CHANGE   ENTER BACK");
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    mines_ctx *c = ctx;
    char t[48];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 30, 5, "MINES");
        centred(f, 96, 2, "8 2 4 6 MOVE  5 OPEN  0 FLAG");
        join(t, LOOK[c->board].name, " ", LOOK[c->board].size);
        centred(f, 126, 1, t);
        centred(f, 140, 1, c->classic ? "CLASSIC RULES: A BOARD MAY NEED A GUESS" : "EVERY BOARD NEEDS NO GUESS");
        centred(f, 164, 2, "ENTER PLAYS   0 SETTINGS");
        const record *r = &c->rec[c->board * 2 + c->classic];
        if (r->best) {
            label(t, "BEST ", (int32_t)(r->best / 1000));
            centred(f, 196, 2, t);
        }
        return;
    }
    if (c->state == SETTINGS) {
        draw_settings(f, c);
        return;
    }
    draw_play(f, c); /* the end is told on the top line, so the whole field shows */
    if (c->state == OVER && (int32_t)(c->now - c->t_end) >= AGAIN_MS)
        centred(f, LOOK[c->board].fy + c->g.h * LOOK[c->board].cell + 2, 1, "ENTER PLAYS   0 SETTINGS");
}

const app_def mines_app = {"MINES", sizeof(mines_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
