/* SPDX-License-Identifier: MIT */
/* BALL: the STU-32 games SDK's template (docs/sdk.md). Copy this folder to start a game: one
 * app_def, all state in the ctx, time only from tick and key, drawing with the framework's draw.h.
 * A paddle (4 and 6) keeps a ball in play; each return scores; a miss ends the game; the best is saved. */
#include <string.h>

#include "apps.h"
#include "draw.h"

enum { TITLE, PLAY, OVER };
#define KEY_LEFT 31   /* 4 (APP_KEY_4 in apps.h v1.2) */
#define KEY_RIGHT 33  /* 6 */
#define KEY_START 18  /* ENTER */
#define PADDLE_W 64
#define PADDLE_Y 222
#define BALL 8
#define FIX 256       /* positions in 1/256 px, speeds in 1/256 px a ms */
#define STEP_MAX 50   /* a long tick is taken as 50 ms, so nothing jumps */
#define SAVE_MAGIC 'T'

typedef struct {
    int state;
    int left, right;          /* held */
    int32_t paddle;           /* the paddle's left edge, px */
    int32_t bx, by, vx, vy;   /* the ball, FIX */
    int32_t score, best;
    uint32_t now, over_at;    /* ms on the game's clock */
} ball_ctx;

static void ball_load(void *ctx, const uint8_t *save, uint32_t n)
{
    ball_ctx *c = ctx;
    /* taken whole or not at all */
    if (n >= 5 && save[0] == SAVE_MAGIC) {
        int32_t b = (int32_t)((uint32_t)save[1] | (uint32_t)save[2] << 8 | (uint32_t)save[3] << 16 | (uint32_t)save[4] << 24);
        if (b >= 0)
            c->best = b;
    }
}

static void ball_save(void *ctx, uint8_t *save, uint32_t cap)
{
    ball_ctx *c = ctx;
    if (cap < 5)
        return;
    save[0] = SAVE_MAGIC;
    for (int i = 0; i < 4; i++)
        save[1 + i] = (uint8_t)((uint32_t)c->best >> (8 * i));
}

static void ball_start(void *ctx)
{
    ball_ctx *c = ctx;
    c->state = TITLE;
}

static void ball_stop(void *ctx)
{
    (void)ctx;
}

static void serve(ball_ctx *c)
{
    c->state = PLAY;
    c->score = 0;
    c->paddle = (LS027_W - PADDLE_W) / 2;
    c->bx = (LS027_W / 2) * FIX;
    c->by = 60 * FIX;
    c->vx = 40;     /* 0.16 px a ms */
    c->vy = 50;
}

static bool ball_key(void *ctx, int key, bool down, uint32_t ms)
{
    ball_ctx *c = ctx;
    if (key == KEY_LEFT)
        c->left = down;
    if (key == KEY_RIGHT)
        c->right = down;
    if (down && key == KEY_START && (c->state == TITLE || (c->state == OVER && ms - c->over_at >= 500))) {
        serve(c);
        return true;
    }
    return false; /* the picture changes on the tick */
}

static uint32_t ball_tick(void *ctx, uint32_t ms)
{
    ball_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    int32_t dt = ms > STEP_MAX ? STEP_MAX : (int32_t)ms;
    c->paddle += (c->right - c->left) * dt * 3 / 10; /* 0.3 px a ms */
    if (c->paddle < 0)
        c->paddle = 0;
    if (c->paddle > LS027_W - PADDLE_W)
        c->paddle = LS027_W - PADDLE_W;
    c->bx += c->vx * dt;
    c->by += c->vy * dt;
    if (c->bx < 0 || c->bx > (LS027_W - BALL) * FIX) {
        c->vx = -c->vx;
        c->bx = c->bx < 0 ? 0 : (LS027_W - BALL) * FIX;
        app_beep(440, 15);
    }
    if (c->by < 30 * FIX) {
        c->vy = -c->vy;
        c->by = 30 * FIX;
        app_beep(440, 15);
    }
    int32_t x = c->bx / FIX, y = c->by / FIX;
    if (c->vy > 0 && y + BALL >= PADDLE_Y && y + BALL <= PADDLE_Y + 8 && x + BALL > c->paddle && x < c->paddle + PADDLE_W) {
        c->vy = -c->vy - 4; /* a little faster each return */
        c->by = (PADDLE_Y - BALL) * FIX;
        c->score++;
        app_beep(880, 25);
    }
    if (y > LS027_H) {
        c->state = OVER;
        c->over_at = c->now;
        if (c->score > c->best)
            c->best = c->score;
        app_beep(150, 200);
    }
    return 20;
}

static void number(char *out, const char *label, int32_t v)
{
    char d[12];
    int n = 0;
    uint32_t u = v < 0 ? 0 : (uint32_t)v;
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (*label)
        *out++ = *label++;
    while (n)
        *out++ = d[--n];
    *out = '\0';
}

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

static void ball_draw(void *ctx, ls027_frame *f)
{
    ball_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 60, 5, "BALL");
        centred(f, 150, 2, "4 6 MOVE   ENTER PLAYS");
        if (c->best > 0) {
            number(t, "BEST ", c->best);
            centred(f, 180, 2, t);
        }
        return;
    }
    draw_fill(f, 0, 26, LS027_W, 2, DRAW_DARK);
    number(t, "", c->score);
    draw_text(f, 6, 6, 2, t, 1);
    draw_fill(f, c->paddle, PADDLE_Y, PADDLE_W, 6, DRAW_DARK);
    draw_fill(f, c->bx / FIX, c->by / FIX, BALL, BALL, c->state == PLAY ? DRAW_DARK : DRAW_HALF);
    if (c->state == OVER) {
        draw_fill(f, 80, 90, 240, 60, DRAW_LIGHT);
        draw_frame_rect(f, 80, 90, 240, 60, 1);
        centred(f, 100, 3, "GAME OVER");
        number(t, "BEST ", c->best);
        centred(f, 130, 2, t);
    }
}

const app_def ball_app = {"BALL", sizeof(ball_ctx), ball_load, ball_start, ball_key, ball_tick,
                          ball_draw, ball_save, ball_stop, 0 /* save_size: APP_SAVE_SIZE */};
