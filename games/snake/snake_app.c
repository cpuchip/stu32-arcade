/* SPDX-License-Identifier: MIT */
/* SNAKE under APPS (docs/snake.md): eat, grow, and keep off the walls and yourself. The grid, the turns, the step
 * and the food are snake_logic's; this file turns keys and the clock into turns and steps, keeps the best, and
 * draws. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "snake_logic.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC 'S'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best (4), games (4), 6 zeros */
#define TICK_MAX_MS 100  /* a long tick is taken as this, so nothing jumps */
#define STEPS_MAX 4      /* a tick's most steps */
#define AGAIN_MS 500     /* GAME OVER takes ENTER after this */
#define HUD_H 20
#define CELL 10

typedef struct {
    int state;
    uint64_t seed;
    uint32_t best, games; /* kept */
    uint32_t now, started, end_at, acc;
    snake_game g;
} snake_ctx;

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
    snake_ctx *c = ctx;
    /* taken whole or not at all */
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION)
        return;
    c->best = get32(save + 2);
    c->games = get32(save + 6);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    snake_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    put32(save + 2, c->best);
    put32(save + 6, c->games);
}

static void app_start_cb(void *ctx)
{
    snake_ctx *c = ctx;
    c->seed = (uint64_t)app_seed() << 32;
    c->state = TITLE;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

/* a game from `at` ms: its first tick counts only the time since then */
static void new_game(snake_ctx *c, uint32_t at)
{
    snake_init(&c->g, c->seed, c->games++);
    c->acc = 0;
    c->started = at;
    c->state = PLAY;
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    snake_ctx *c = ctx;
    if (!down)
        return false;
    if (c->state == PLAY) {
        int dir = key == APP_KEY_8 ? SNAKE_UP : key == APP_KEY_6 ? SNAKE_RIGHT : key == APP_KEY_2 ? SNAKE_DOWN
                : key == APP_KEY_4 ? SNAKE_LEFT : -1;
        if (dir >= 0)
            snake_offer(&c->g, dir);
        return false; /* the picture changes on the step */
    }
    if (key == APP_KEY_ENTER && (c->state == TITLE || c->now - c->end_at >= AGAIN_MS)) {
        new_game(c, ms);
        return true;
    }
    return false;
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    snake_ctx *c = ctx;
    c->now += ms;
    if (c->state == TITLE)
        return 100;
    if (c->state != PLAY)
        return 20;
    uint32_t dt = c->now - c->started < ms ? c->now - c->started : ms; /* not the time before the game began */
    c->acc += dt > TICK_MAX_MS ? TICK_MAX_MS : dt;
    for (int n = 0; n < STEPS_MAX && c->state == PLAY; n++) {
        uint32_t step = snake_interval(c->g.eaten);
        if (c->acc < step)
            break;
        c->acc -= step;
        uint16_t eaten = c->g.eaten;
        enum snake_end e = snake_step(&c->g);
        if (c->g.eaten != eaten) {
            if (c->g.eaten > c->best)
                c->best = c->g.eaten;
            app_beep(660, 30);
        }
        if (e != SNAKE_ON) {
            static const uint16_t fall[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
            static const uint16_t rise[] = {523, 100, 659, 100, 784, 100, 1047, 300};
            if (e == SNAKE_DEAD)
                app_tune(fall, 5);
            else
                app_tune(rise, 4);
            c->state = OVER;
            c->end_at = c->now;
        }
    }
    return 20;
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

static void label(char *out, const char *s, uint32_t v)
{
    while (*s)
        *out++ = *s++;
    number(out, v);
}

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

static void draw_play(ls027_frame *f, const snake_ctx *c)
{
    char t[24];
    label(t, "SCORE ", c->g.eaten);
    draw_text(f, 4, 3, 2, t, 1);
    label(t, "LENGTH ", c->g.len);
    centred(f, 3, 2, t);
    label(t, "BEST ", c->best);
    draw_text(f, LS027_W - 4 - draw_text_width(t, 2), 3, 2, t, 1);
    draw_frame_rect(f, 0, HUD_H, LS027_W, LS027_H - HUD_H, 1);

    const snake_game *g = &c->g;
    for (int i = 0; i + 1 < g->len; i++) {
        int cell = snake_at(g, i);
        draw_fill(f, cell % SNAKE_W * CELL + 1, HUD_H + cell / SNAKE_W * CELL + 1, 8, 8, DRAW_DARK);
    }
    int h = snake_head(g), hx = h % SNAKE_W * CELL + 1, hy = HUD_H + h / SNAKE_W * CELL + 1;
    draw_frame_rect(f, hx, hy, 8, 8, 1);
    draw_fill(f, hx + 3, hy + 3, 2, 2, DRAW_DARK);
    if (g->food >= 0) {
        int fx = g->food % SNAKE_W * CELL + 5, fy = HUD_H + g->food / SNAKE_W * CELL + 5;
        draw_line(f, fx, fy - 4, fx + 4, fy, 1);
        draw_line(f, fx + 4, fy, fx, fy + 4, 1);
        draw_line(f, fx, fy + 4, fx - 4, fy, 1);
        draw_line(f, fx - 4, fy, fx, fy - 4, 1);
    }
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    snake_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 40, 5, "SNAKE");
        centred(f, 110, 2, "8 2 4 6 TURN");
        centred(f, 136, 1, "EAT, GROW, AND KEEP OFF THE WALLS AND YOURSELF");
        centred(f, 168, 2, "ENTER PLAYS");
        if (c->best > 0) {
            label(t, "BEST ", c->best);
            centred(f, 200, 2, t);
        }
        return;
    }
    draw_play(f, c);
    if (c->state == OVER) {
        draw_fill(f, 80, 56, 240, 104, DRAW_LIGHT);
        draw_frame_rect(f, 80, 56, 240, 104, 1);
        centred(f, 66, 3, c->g.end == SNAKE_WON ? "FULL" : "GAME OVER");
        label(t, "SCORE ", c->g.eaten);
        centred(f, 96, 2, t);
        label(t, "BEST ", c->best);
        centred(f, 116, 2, t);
        if (c->now - c->end_at >= AGAIN_MS)
            centred(f, 142, 1, "ENTER PLAYS");
    }
}

const app_def snake_app = {"SNAKE", sizeof(snake_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
