/* SPDX-License-Identifier: MIT */
/* CATCH under APPS (docs/catch.md): catch the falling stars. The basket, the stars and the catches are catch_logic's;
 * this file turns keys and the clock into its keys and ticks, keeps the bests, sounds the events, and draws, with
 * nothing to read in play. */
#include <string.h>

#include "apps.h"
#include "catch_logic.h"
#include "draw.h"
#include "kids_draw.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC 'K'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best gentle (4), best full (4), games (4), 2 zeros */
#define AGAIN_MS 500     /* the end takes ENTER after this */
#define FLASH_STEPS 4

typedef struct {
    int state;
    bool full;           /* the mode picked on the title; it opens on gentle */
    uint32_t seed;
    uint32_t best[2], games; /* kept */
    uint32_t now, started, end_at;
    catch_game g;
    int16_t flash_x;     /* a catch's ring of dots, over the basket */
    uint8_t flash;
    int16_t fade_x;      /* a gentle miss, fading at the bottom */
    uint8_t fade;
    uint8_t sound;       /* the steps of a tick, folded: 1 catch, 2 miss, 4 life, 8 ten, 16 bump, 32 end */
    uint32_t tune_until; /* while a tune plays, no beep starts */
} catch_ctx;

/* star: 11 x 11 */
static const uint8_t STAR[] = {
    0x20, 0x00,  /* .....#..... */
    0x20, 0x00,  /* .....#..... */
    0x70, 0x00,  /* ....###.... */
    0x70, 0x00,  /* ....###.... */
    0xff, 0x07,  /* ########### */
    0xfe, 0x03,  /* .#########. */
    0xfc, 0x01,  /* ..#######.. */
    0xf8, 0x00,  /* ...#####... */
    0xdc, 0x01,  /* ..###.###.. */
    0x8c, 0x01,  /* ..##...##.. */
    0x04, 0x01,  /* ..#.....#.. */
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
    catch_ctx *c = ctx;
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION || save[14] || save[15])
        return;          /* taken whole or not at all */
    c->best[0] = get32(save + 2);
    c->best[1] = get32(save + 6);
    c->games = get32(save + 10);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    catch_ctx *c = ctx;
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
    catch_ctx *c = ctx;
    c->seed = app_seed();
    c->state = TITLE;
    c->full = false;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void new_game(catch_ctx *c, uint32_t at)
{
    catch_init(&c->g, c->full, c->seed, c->games++);
    c->flash = c->fade = 0;
    c->tune_until = 0;
    c->started = at;
    c->state = PLAY;
}

static int key_of(int key)
{
    switch (key) {
    case APP_KEY_4: return CATCH_K4;
    case APP_KEY_SOFT1: return CATCH_KS1;
    case APP_KEY_6: return CATCH_K6;
    case APP_KEY_SOFT6: return CATCH_KS6;
    default: return -1;
    }
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    catch_ctx *c = ctx;
    if (c->state == PLAY) {
        catch_key(&c->g, key_of(key), down);
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

/* after each step: the flashes age and start, the step's sounds are noted, the best kept */
static void each_step(const catch_game *g, void *arg)
{
    catch_ctx *c = arg;
    if (c->flash)
        c->flash--;
    if (c->fade)
        c->fade--;
    if (g->ev_catches) {
        c->flash = FLASH_STEPS;
        c->flash_x = g->bx;
    }
    if (g->ev_misses && !g->full) {
        c->fade = FLASH_STEPS;
        c->fade_x = g->ev_miss_x;
    }
    if (g->score > c->best[g->full])
        c->best[g->full] = g->score;
    c->sound |= (uint8_t)((g->ev_catches ? 1 : 0) | (g->ev_misses ? 2 : 0) | (g->ev_lost ? 4 : 0) | (g->ev_ten ? 8 : 0)
                          | (g->bump ? 16 : 0) | (g->over ? 32 : 0));
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    catch_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    uint32_t dt = c->now - c->started < ms ? c->now - c->started : ms; /* not the time before the game began */
    c->sound = 0;
    catch_tick(&c->g, dt, each_step, c);
    static const uint16_t end[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
    static const uint16_t lost[] = {330, 100, 262, 160};
    static const uint16_t ten[] = {523, 100, 659, 100, 784, 200};
    if (c->sound & 32) {
        app_tune(end, 5);
        c->tune_until = c->now + 620;
    } else if (c->sound & 4) {
        app_tune(lost, 2);
        c->tune_until = c->now + 260;
    } else if (c->sound & 8) {
        app_tune(ten, 3);
        c->tune_until = c->now + 400;
    } else if (c->now >= c->tune_until) {
        if (c->sound & 1)
            app_beep(1047, 30);
        else if (c->sound & 16)
            app_beep(100, 10);
        else if (c->sound & 2)
            app_beep(196, 30);
    }
    if (c->g.over) {
        c->state = OVER;
        c->end_at = c->now;
        return 100;
    }
    return CATCH_STEP_MS - c->g.acc;           /* the next step's time */
}

/* drawing */

static void centred(ls027_frame *f, int y, int scale, const char *s)
{
    draw_text(f, (LS027_W - draw_text_width(s, scale)) / 2, y, scale, s, 1);
}

/* the basket, its top-left at (x, y): wider at the mouth, with a weave of two lines */
static void basket(ls027_frame *f, int x, int y, int w, int h)
{
    int in = w / 12;
    draw_line(f, x, y, x + w - 1, y, 1);
    draw_line(f, x, y, x + in, y + h - 1, 1);
    draw_line(f, x + w - 1, y, x + w - 1 - in, y + h - 1, 1);
    draw_line(f, x + in, y + h - 1, x + w - 1 - in, y + h - 1, 1);
    draw_line(f, x + in / 2 + 1, y + h / 3, x + w - 2 - in / 2, y + h / 3, 1);
    draw_line(f, x + in, y + 2 * h / 3, x + w - 1 - in, y + 2 * h / 3, 1);
}

static void ring(ls027_frame *f, int cx, int cy)
{
    static const int8_t d[8][2] = {{0, -9}, {6, -6}, {9, 0}, {6, 6}, {0, 9}, {-6, 6}, {-9, 0}, {-6, -6}};
    for (int i = 0; i < 8; i++)
        draw_fill(f, cx + d[i][0] - 1, cy + d[i][1] - 1, 2, 2, DRAW_DARK);
}

static void draw_play(ls027_frame *f, const catch_ctx *c)
{
    const catch_game *g = &c->g;
    for (int i = 0; i < g->n; i++) {
        int y = g->star[i].y >= 0 ? g->star[i].y / CATCH_SUB : -((-g->star[i].y + CATCH_SUB - 1) / CATCH_SUB);
        draw_blit(f, g->star[i].x, CATCH_TOP + y, STAR, CATCH_STAR, CATCH_STAR, DRAW_OR);
    }
    basket(f, g->bx + 4, CATCH_TOP + CATCH_BASKET_Y, CATCH_BASKET_W - 8, CATCH_BASKET_H);   /* drawn as wide as it catches */
    if (c->flash)
        ring(f, c->flash_x + CATCH_BASKET_W / 2, CATCH_TOP + CATCH_BASKET_Y - 4);
    if (c->fade & 1)                                    /* a gentle miss blinks at the bottom as it goes */
        draw_blit(f, c->fade_x, LS027_H - CATCH_STAR, STAR, CATCH_STAR, CATCH_STAR, DRAW_OR);

    /* the top line, over anything that fell into it */
    draw_fill(f, 0, 0, LS027_W, CATCH_TOP, DRAW_LIGHT);
    for (int i = 0; i < g->lives; i++)
        basket(f, 4 + 16 * i, 2, 13, 7);
    kids_tally(f, LS027_W - 4 - kids_tally_width(g->score), 2, g->score);
    draw_line(f, 0, CATCH_TOP - 1, LS027_W - 1, CATCH_TOP - 1, 1);
}

static void draw_title(ls027_frame *f, const catch_ctx *c)
{
    centred(f, 14, 5, "CATCH");
    for (int m = 0; m < 2; m++) {
        int x = m ? 214 : 36;
        kids_choice(f, x, 66, 150, 80, c->full == m);
        basket(f, x + 51, 124, 48, 14);
        if (m) {
            static const int8_t at[4][2] = {{20, 18}, {62, 40}, {100, 10}, {120, 60}};
            for (int i = 0; i < 4; i++)
                draw_blit(f, x + at[i][0], 66 + at[i][1], STAR, CATCH_STAR, CATCH_STAR, DRAW_OR);
            for (int i = 0; i < 3; i++)
                basket(f, x + 6 + 16 * i, 70, 13, 7);
        } else {
            draw_blit(f, x + 70, 90, STAR, CATCH_STAR, CATCH_STAR, DRAW_OR);
        }
        if (c->best[m])
            kids_tally(f, x + 75 - kids_tally_width(c->best[m]) / 2, 152, c->best[m]);
    }
    int x = 150;
    x += kids_keycap(f, x, 176, "4", 1) + 10;
    kids_arrow(f, x, 182, 0);
    x += 14;
    kids_arrow(f, x, 182, 1);
    x += 10;
    kids_keycap(f, x, 176, "6", 1);
    if (c->now / 500 % 2 == 0)
        kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 206, "ENTER", 1);
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    catch_ctx *c = ctx;
    draw_clear(f);
    if (c->state == TITLE) {
        draw_title(f, c);
        return;
    }
    draw_play(f, c);
    if (c->state == OVER) {
        uint32_t best = c->best[c->g.full];
        draw_fill(f, 80, 56, 240, 112, DRAW_LIGHT);
        draw_frame_rect(f, 80, 56, 240, 112, 1);
        basket(f, 176, 66, 48, 14);
        kids_tally(f, 200 - kids_tally_width(c->g.score) / 2, 92, c->g.score);
        draw_blit(f, 120, 112, KIDS_STAR, 7, 7, DRAW_OR);   /* the best, beside a star */
        kids_tally(f, 200 - kids_tally_width(best) / 2, 112, best);
        if (c->now - c->end_at >= AGAIN_MS && c->now / 500 % 2 == 0)
            kids_keycap(f, (LS027_W - kids_keycap(f, 0, 0, "ENTER", 0)) / 2, 140, "ENTER", 1);
    }
}

const app_def catch_app = {"CATCH", sizeof(catch_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
