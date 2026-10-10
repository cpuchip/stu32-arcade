/* SPDX-License-Identifier: MIT */
/* AIM under APPS (docs/aim.md): four tiles to the round's target in RPN on the calculator's own keys. The rules,
 * the solver and the rounds are aim_logic's; this file turns keys and the clock into them, keeps the score, and
 * draws. */
#include <string.h>

#include "aim_logic.h"
#include "apps.h"
#include "draw.h"

enum { TITLE, PLAY, WON, SHOWN };
#define SAVE_MAGIC 'A'
#define SAVE_VERSION 1
#define SAVE_BYTES 16 /* magic, version, best streak (2), won (4), shown (4), best level, 3 zeros */
#define FLASH_MS 600  /* a won round's target flashes this long */
#define IDLE_MS 250

typedef struct {
    int state;
    uint64_t seed;
    uint32_t round_no; /* this run's rounds, for the generator */
    uint16_t streak, best;
    uint32_t won, shown;
    uint8_t best_level;
    uint32_t now, flash_at;
    int show_n;
    enum aim_key show[AIM_SHOW_MAX];
    aim_round r;
    aim_solver s; /* about 15 KB */
} aim_ctx;

static void put32(uint8_t *b, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        b[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static void aim_load(void *ctx, const uint8_t *save, uint32_t n)
{
    aim_ctx *c = ctx;
    /* taken whole or not at all */
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || save[12] >= AIM_LEVELS)
        return;
    c->best = (uint16_t)(save[2] | save[3] << 8);
    c->won = get32(save + 4);
    c->shown = get32(save + 8);
    c->best_level = save[12];
}

static void aim_save(void *ctx, uint8_t *save, uint32_t cap)
{
    aim_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    save[2] = (uint8_t)c->best;
    save[3] = (uint8_t)(c->best >> 8);
    put32(save + 4, c->won);
    put32(save + 8, c->shown);
    save[12] = c->best_level;
}

static int level(const aim_ctx *c)
{
    int l = c->streak / 5;
    return l < AIM_LEVELS - 1 ? l : AIM_LEVELS - 1;
}

static void new_round(aim_ctx *c)
{
    uint8_t tiles[4];
    int32_t target;
    aim_make_round(&c->s, c->seed, c->round_no++, level(c), tiles, &target);
    aim_round_init(&c->r, tiles, target);
    c->state = PLAY;
    c->show_n = 0;
}

static void aim_start(void *ctx)
{
    aim_ctx *c = ctx;
    /* app_seed is the same each call on a host, so the saved counts make each run's rounds differ */
    c->seed = (uint64_t)app_seed() << 32 ^ (uint64_t)(c->won + c->shown);
    c->state = TITLE;
}

static void aim_stop(void *ctx)
{
    (void)ctx;
}

static void end_streak(aim_ctx *c)
{
    c->streak = 0;
}

static int logic_key(int key)
{
    static const struct {
        int app, aim;
    } map[] = {
        {APP_KEY_0, AIM_K0}, {APP_KEY_1, AIM_K1}, {APP_KEY_2, AIM_K2}, {APP_KEY_3, AIM_K3},
        {APP_KEY_4, AIM_K4}, {APP_KEY_5, AIM_K5}, {APP_KEY_6, AIM_K6}, {APP_KEY_7, AIM_K7},
        {APP_KEY_8, AIM_K8}, {APP_KEY_9, AIM_K9}, {APP_KEY_ENTER, AIM_ENTER}, {APP_KEY_PLUS, AIM_PLUS},
        {APP_KEY_MINUS, AIM_MINUS}, {APP_KEY_TIMES, AIM_TIMES}, {APP_KEY_DIVIDE, AIM_DIVIDE},
        {APP_KEY_SWAP, AIM_SWAP}, {APP_KEY_BACK, AIM_BACK}, {APP_KEY_SOFT1, AIM_BACK},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (map[i].app == key)
            return map[i].aim;
    return -1;
}

static bool aim_key(void *ctx, int key, bool down, uint32_t ms)
{
    aim_ctx *c = ctx;
    (void)ms;
    if (!down)
        return false;
    if (c->state == TITLE) {
        if (key != APP_KEY_ENTER)
            return false;
        new_round(c);
        return true;
    }
    if (c->state != PLAY) {
        if (key != APP_KEY_ENTER && key != APP_KEY_SOFT6)
            return false;
        new_round(c);
        return true;
    }
    if (key == APP_KEY_SOFT2) { /* SKIP: a pass */
        end_streak(c);
        new_round(c);
        return true;
    }
    if (key == APP_KEY_SOFT3) { /* SHOW: give the round up */
        end_streak(c);
        c->shown++;
        c->show_n = aim_show(&c->s, c->r.tiles, c->r.target, c->show);
        c->state = SHOWN;
        return true;
    }
    int k = logic_key(key);
    if (k < 0)
        return false;
    if (aim_round_key(&c->r, (enum aim_key)k)) {
        app_beep(220, 40);
        return true;
    }
    if (c->r.won) {
        c->state = WON;
        c->won++;
        if (++c->streak > c->best)
            c->best = c->streak;
        if (level(c) > c->best_level)
            c->best_level = (uint8_t)level(c);
        c->flash_at = c->now;
        static const uint16_t hit[] = {523, 80, 659, 80, 784, 160}; /* (hz, ms) */
        app_tune(hit, 3);
    }
    return true;
}

static uint32_t aim_tick(void *ctx, uint32_t ms)
{
    aim_ctx *c = ctx;
    c->now += ms;
    if (c->state == WON && c->now - c->flash_at < FLASH_MS)
        return 100;
    return IDLE_MS;
}

/* ---- drawing ---- */

static char *put_num(char *o, int32_t v)
{
    char d[12];
    int n = 0;
    uint32_t u = v < 0 ? (uint32_t)-(int64_t)v : (uint32_t)v;
    if (v < 0)
        *o++ = '-';
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        *o++ = d[--n];
    *o = 0;
    return o;
}

static char *put_str(char *o, const char *s)
{
    while (*s)
        *o++ = *s++;
    *o = 0;
    return o;
}

static void centred(ls027_frame *f, int x, int w, int y, int scale, const char *s, int dark)
{
    draw_text(f, x + (w - draw_text_width(s, scale)) / 2, y, scale, s, dark);
}

static const char *key_label(enum aim_key k)
{
    static const char *const ops[] = {"ENTER", "+", "-", "\xd7", "\xf7"}; /* x and / as the font's 0xd7, 0xf7 */
    static const char *const digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    return k <= AIM_K9 ? digits[k] : ops[k - AIM_ENTER];
}

#define TILE_X 16
#define TILE_Y 92
#define TILE_W 52
#define TILE_GAP 10
#define STACK_X 270

static void draw_play(const aim_ctx *c, ls027_frame *f)
{
    char t[48];
    char *o;
    /* the score line */
    o = put_str(t, "STREAK ");
    put_num(o, c->streak);
    draw_text(f, 6, 4, 2, t, 1);
    o = put_str(t, "BEST ");
    put_num(o, c->best);
    draw_text(f, LS027_W - 6 - draw_text_width(t, 2), 4, 2, t, 1);
    put_num(put_str(t, "ROUND "), (int32_t)c->round_no);
    centred(f, 0, LS027_W, 4, 2, t, 1);
    draw_fill(f, 0, 22, LS027_W, 2, DRAW_DARK);

    /* the target, flashing a moment when it is reached */
    put_num(t, c->r.target);
    bool lit = c->state == WON && c->now - c->flash_at < FLASH_MS && (c->now - c->flash_at) / 100 % 2 == 0;
    if (lit)
        draw_fill(f, 120, 30, 160, 54, DRAW_DARK);
    centred(f, 120, 160, 36, 6, t, !lit);

    /* the tiles, a used one in half shade */
    for (int i = 0; i < 4; i++) {
        int x = TILE_X + i * (TILE_W + TILE_GAP);
        bool used = c->r.used >> i & 1;
        if (used)
            draw_fill(f, x, TILE_Y, TILE_W, TILE_W, DRAW_HALF);
        draw_frame_rect(f, x, TILE_Y, TILE_W, TILE_W, 1);
        char d[2] = {(char)('0' + c->r.tiles[i]), 0};
        if (used)
            draw_fill(f, x + 9, TILE_Y + 7, 34, 38, DRAW_LIGHT);
        centred(f, x, TILE_W, TILE_Y + 9, 5, d, 1);
    }

    /* the stack, X at the bottom as on the calculator, the entry under it */
    draw_fill(f, STACK_X - 10, TILE_Y - 4, 2, 4 * 18 + 16, DRAW_DARK); /* down to the entry line, above the message */
    static const char *const lv[] = {"T", "Z", "Y", "X"};
    for (int i = 0; i < 4; i++) {
        int y = TILE_Y + i * 18, at = c->r.sp - 4 + i;
        draw_text(f, STACK_X, y, 2, lv[i], 1);
        if (at < 0)
            continue;
        aim_frac v = c->r.stack[at];
        o = put_num(t, v.n);
        if (v.d != 1) {
            o = put_str(o, "/");
            put_num(o, v.d);
        }
        draw_text(f, LS027_W - 6 - draw_text_width(t, 2), y, 2, t, 1);
    }
    if (c->r.entry[0]) {
        o = put_str(t, c->r.entry);
        put_str(o, "_");
        draw_text(f, LS027_W - 6 - draw_text_width(t, 2), TILE_Y + 4 * 18, 2, t, 1);
    }

    /* the message line */
    if (c->state == WON) {
        centred(f, 0, LS027_W, 176, 3, "ON TARGET", 1);
    } else if (c->state == SHOWN) {
        o = t;
        for (int i = 0; i < c->show_n; i++) {
            if (i)
                o = put_str(o, " ");
            o = put_str(o, key_label(c->show[i]));
        }
        centred(f, 0, LS027_W, 180, 2, t, 1);
    } else {
        centred(f, 0, LS027_W, 180, 2, "EACH TILE ONCE, + - \xd7 \xf7", 1);
    }

    /* the soft keys' labels */
    static const char *const play_soft[6] = {"UNDO", "SKIP", "SHOW", "", "", ""};
    static const char *const done_soft[6] = {"", "", "", "", "", "NEW"};
    const char *const *soft = c->state == PLAY ? play_soft : done_soft;
    draw_fill(f, 0, 214, LS027_W, 2, DRAW_DARK);
    for (int i = 0; i < 6; i++)
        if (soft[i][0])
            centred(f, i * 66 + 2, 64, 222, 2, soft[i], 1);
}

static void aim_draw(void *ctx, ls027_frame *f)
{
    aim_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 0, LS027_W, 40, 7, "AIM", 1);
        centred(f, 0, LS027_W, 110, 2, "FOUR TILES TO THE TARGET, IN RPN", 1);
        centred(f, 0, LS027_W, 136, 2, "8 ENTER 3 \xd7 MAKES 24", 1);
        centred(f, 0, LS027_W, 176, 2, "ENTER PLAYS", 1);
        if (c->best) {
            put_num(put_str(t, "BEST STREAK "), c->best);
            centred(f, 0, LS027_W, 204, 2, t, 1);
        }
        return;
    }
    draw_play(c, f);
}

const app_def aim_app = {"AIM", sizeof(aim_ctx), aim_load, aim_start, aim_key, aim_tick,
                         aim_draw, aim_save, aim_stop, SAVE_BYTES};
