/* SPDX-License-Identifier: MIT */
/* SALVO under APPS (docs/salvo.md): hold off the swarm. The field, the step and the hits are salvo_logic's; this
 * file turns keys and the clock into its keys and ticks, keeps the best, sounds the events, and draws. */
#include <string.h>

#include "apps.h"
#include "draw.h"
#include "salvo_logic.h"

enum { TITLE, PLAY, OVER };
#define SAVE_MAGIC 'V'
#define SAVE_VERSION 1
#define SAVE_BYTES 16    /* magic, version, best (4), games (4), 6 zeros */
#define AGAIN_MS 500     /* GAME OVER takes ENTER after this */
#define BURSTS 4
#define BURST_STEPS 4
#define STARS 48

typedef struct {
    int16_t x, y;
    uint8_t left;        /* steps it still shows */
} burst;

typedef struct {
    int state;
    uint32_t seed;
    uint32_t best, games; /* kept */
    uint32_t now, started, end_at;
    salvo_game g;
    burst b[BURSTS];
    uint8_t sound;       /* the steps of a tick, folded: 1 shot, 2 kill, 4 lost, 8 wave */
    uint32_t tune_until; /* while a tune plays, no click or beep starts */
} salvo_ctx;

/* flag0: 13 x 10 */
static const uint8_t FLAG0[] = {
    0xf0, 0x01,  /* ....#####.... */
    0xfc, 0x07,  /* ..#########.. */
    0xe6, 0x0c,  /* .##..###..##. */
    0xfe, 0x0f,  /* .###########. */
    0xf5, 0x15,  /* #.#.#####.#.# */
    0x11, 0x11,  /* #...#...#...# */
    0x10, 0x01,  /* ....#...#.... */
    0x18, 0x03,  /* ...##...##... */
    0x00, 0x00,  /* ............. */
    0x00, 0x00,  /* ............. */
};
/* flag1: 13 x 10 */
static const uint8_t FLAG1[] = {
    0xf0, 0x01,  /* ....#####.... */
    0xfc, 0x07,  /* ..#########.. */
    0xe6, 0x0c,  /* .##..###..##. */
    0xfe, 0x0f,  /* .###########. */
    0xf4, 0x05,  /* ..#.#####.#.. */
    0x12, 0x09,  /* .#..#...#..#. */
    0x12, 0x09,  /* .#..#...#..#. */
    0xb0, 0x01,  /* ....##.##.... */
    0x00, 0x00,  /* ............. */
    0x00, 0x00,  /* ............. */
};
/* esc0: 13 x 10 */
static const uint8_t ESC0[] = {
    0x02, 0x08,  /* .#.........#. */
    0x04, 0x04,  /* ..#.......#.. */
    0xfc, 0x07,  /* ..#########.. */
    0xb6, 0x0d,  /* .##.##.##.##. */
    0xff, 0x1f,  /* ############# */
    0xfd, 0x17,  /* #.#########.# */
    0x05, 0x14,  /* #.#.......#.# */
    0x18, 0x03,  /* ...##...##... */
    0x00, 0x00,  /* ............. */
    0x00, 0x00,  /* ............. */
};
/* esc1: 13 x 10 */
static const uint8_t ESC1[] = {
    0x08, 0x02,  /* ...#.....#... */
    0x08, 0x02,  /* ...#.....#... */
    0xfc, 0x07,  /* ..#########.. */
    0xb6, 0x0d,  /* .##.##.##.##. */
    0xff, 0x1f,  /* ############# */
    0xfc, 0x07,  /* ..#########.. */
    0x0a, 0x0a,  /* .#.#.....#.#. */
    0x01, 0x10,  /* #...........# */
    0x00, 0x00,  /* ............. */
    0x00, 0x00,  /* ............. */
};
/* drone0: 13 x 10 */
static const uint8_t DRONE0[] = {
    0xa0, 0x00,  /* .....#.#..... */
    0xe4, 0x04,  /* ..#..###..#.. */
    0xf4, 0x05,  /* ..#.#####.#.. */
    0x5c, 0x07,  /* ..###.#.###.. */
    0xf8, 0x03,  /* ...#######... */
    0xf0, 0x01,  /* ....#####.... */
    0x18, 0x03,  /* ...##...##... */
    0x0c, 0x06,  /* ..##.....##.. */
    0x06, 0x0c,  /* .##.......##. */
    0x00, 0x00,  /* ............. */
};
/* drone1: 13 x 10 */
static const uint8_t DRONE1[] = {
    0xa0, 0x00,  /* .....#.#..... */
    0xe0, 0x00,  /* .....###..... */
    0xf1, 0x11,  /* #...#####...# */
    0x5e, 0x0f,  /* .####.#.####. */
    0xf8, 0x03,  /* ...#######... */
    0xf0, 0x01,  /* ....#####.... */
    0x10, 0x01,  /* ....#...#.... */
    0x10, 0x01,  /* ....#...#.... */
    0x18, 0x03,  /* ...##...##... */
    0x00, 0x00,  /* ............. */
};
/* burst: 13 x 10 */
static const uint8_t BURST[] = {
    0x41, 0x10,  /* #.....#.....# */
    0x42, 0x08,  /* .#....#....#. */
    0x04, 0x04,  /* ..#.......#.. */
    0xe0, 0x00,  /* .....###..... */
    0x17, 0x1d,  /* ###.#...#.### */
    0xe0, 0x00,  /* .....###..... */
    0x04, 0x04,  /* ..#.......#.. */
    0x42, 0x08,  /* .#....#....#. */
    0x41, 0x10,  /* #.....#.....# */
    0x00, 0x00,  /* ............. */
};
/* ship: 15 x 8 */
static const uint8_t SHIP[] = {
    0x80, 0x00,  /* .......#....... */
    0xc0, 0x01,  /* ......###...... */
    0xc0, 0x01,  /* ......###...... */
    0xe4, 0x13,  /* ..#..#####..#.. */
    0xf4, 0x17,  /* ..#.#######.#.. */
    0xfe, 0x3f,  /* .#############. */
    0xff, 0x7f,  /* ############### */
    0x1b, 0x6c,  /* ##.##.....##.## */
};
/* life: 9 x 5 */
static const uint8_t LIFE[] = {
    0x10, 0x00,  /* ....#.... */
    0x38, 0x00,  /* ...###... */
    0xfe, 0x00,  /* .#######. */
    0xff, 0x01,  /* ######### */
    0x83, 0x01,  /* ##.....## */
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
    salvo_ctx *c = ctx;
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION)
        return;
    for (int i = 10; i < SAVE_BYTES; i++)
        if (save[i])
            return;      /* taken whole or not at all */
    c->best = get32(save + 2);
    c->games = get32(save + 6);
}

static void app_save_cb(void *ctx, uint8_t *save, uint32_t cap)
{
    salvo_ctx *c = ctx;
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
    salvo_ctx *c = ctx;
    c->seed = app_seed();
    c->state = TITLE;
}

static void app_stop_cb(void *ctx)
{
    (void)ctx;
}

static void new_game(salvo_ctx *c, uint32_t at)
{
    salvo_init(&c->g, c->seed, c->games++);
    memset(c->b, 0, sizeof c->b);
    c->tune_until = 0;
    c->started = at;
    c->state = PLAY;
}

static int key_of(int key)
{
    switch (key) {
    case APP_KEY_4: return SALVO_K4;
    case APP_KEY_SOFT1: return SALVO_KS1;
    case APP_KEY_6: return SALVO_K6;
    case APP_KEY_SOFT6: return SALVO_KS6;
    case APP_KEY_8: return SALVO_K8;
    case APP_KEY_ENTER: return SALVO_KE;
    case APP_KEY_9: return SALVO_K9;
    case APP_KEY_TIMES: return SALVO_KX;
    case APP_KEY_PLUS: return SALVO_KP;
    default: return -1;
    }
}

static bool app_key_cb(void *ctx, int key, bool down, uint32_t ms)
{
    salvo_ctx *c = ctx;
    if (c->state == PLAY) {
        salvo_key(&c->g, key_of(key), down);
        return false; /* the picture changes on the step */
    }
    if (down && key == APP_KEY_ENTER && (c->state == TITLE || ms - c->end_at >= AGAIN_MS)) {
        new_game(c, ms);
        return true;
    }
    return false;
}

/* after each step: the bursts age, new ones start where shots killed, and the step's sounds are noted */
static void each_step(const salvo_game *g, void *arg)
{
    salvo_ctx *c = arg;
    for (int i = 0; i < BURSTS; i++)
        if (c->b[i].left)
            c->b[i].left--;
    for (int k = 0; k < g->ev_kills; k++) {
        for (int i = 0; i < BURSTS; i++)
            if (!c->b[i].left) {
                c->b[i] = (burst){g->ev_kill_pt[k].x, g->ev_kill_pt[k].y, BURST_STEPS};
                break;
            }
    }
    if (g->score > c->best)
        c->best = g->score;
    c->sound |= (uint8_t)((g->ev_shots ? 1 : 0) | (g->ev_kills ? 2 : 0) | (g->ev_lost ? 4 : 0) | (g->ev_wave ? 8 : 0));
}

static uint32_t app_tick_cb(void *ctx, uint32_t ms)
{
    salvo_ctx *c = ctx;
    c->now += ms;
    if (c->state != PLAY)
        return 100;
    uint32_t dt = c->now - c->started < ms ? c->now - c->started : ms; /* not the time before the game began */
    c->sound = 0;
    salvo_tick(&c->g, dt, each_step, c);
    static const uint16_t fall[] = {392, 120, 0, 40, 330, 120, 0, 40, 262, 300}; /* (hz, ms); 0: a rest */
    static const uint16_t rise[] = {523, 100, 659, 100, 784, 200};
    if (c->sound & 4) {
        app_tune(fall, 5);
        c->tune_until = c->now + 620;
    } else if (c->sound & 8) {
        app_tune(rise, 3);
        c->tune_until = c->now + 400;
    } else if (c->now >= c->tune_until) {
        if (c->sound & 2)
            app_beep(880, 20);
        else if (c->sound & 1)
            app_beep(1200, 10);
    }
    if (c->g.over) {
        c->state = OVER;
        c->end_at = c->now;
        return 100;
    }
    return SALVO_STEP_MS - c->g.acc;            /* the next step's time */
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

static void stars(ls027_frame *f)
{
    uint32_t z = 0x5a17u;                       /* the same 48 every game (a fixed LCG; any fixed set would do) */
    for (int i = 0; i < STARS; i++) {
        z = z * 1103515245u + 12345u;
        int x = (int)(z >> 8) % LS027_W;
        z = z * 1103515245u + 12345u;
        int y = SALVO_TOP + (int)(z >> 8) % (SALVO_SHIP_Y - SALVO_TOP);
        draw_pixel(f, x, y, 1);
    }
}

static const uint8_t *alien_sprite(int slot, int frame)
{
    int r = slot / SALVO_COLS;
    if (r == 0)
        return frame ? FLAG1 : FLAG0;
    if (r == 1)
        return frame ? ESC1 : ESC0;
    return frame ? DRONE1 : DRONE0;
}

static void draw_play(ls027_frame *f, const salvo_ctx *c)
{
    const salvo_game *g = &c->g;
    char t[24];
    stars(f);
    int frame = g->sway / 4 % 2;
    for (int i = 0; i < SALVO_ALIENS; i++) {
        const salvo_alien *a = &g->a[i];
        if (a->state == SALVO_DEAD)
            continue;
        int x = a->state == SALVO_SLOT ? salvo_slot_x(g, i) : a->x;
        int y = a->state == SALVO_SLOT ? salvo_slot_y(g, i) : a->y;
        draw_blit(f, x, y, alien_sprite(i, frame), SALVO_ALIEN_W, SALVO_ALIEN_H, DRAW_OR);
    }
    for (int i = 0; i < BURSTS; i++)
        if (c->b[i].left)
            draw_blit(f, c->b[i].x, c->b[i].y, BURST, SALVO_ALIEN_W, SALVO_ALIEN_H, DRAW_OR);
    for (int s = 0; s < g->nshot; s++)
        draw_fill(f, g->shot[s].x, g->shot[s].y, 1, 6, DRAW_DARK);
    for (int s = 0; s < g->neshot; s++)
        draw_fill(f, g->eshot[s].x, g->eshot[s].y, 2, 4, DRAW_DARK);
    if (salvo_ship_alive(g))
        draw_blit(f, g->sx, SALVO_SHIP_Y, SHIP, SALVO_SHIP_W, SALVO_SHIP_H, DRAW_OR);
    else if (g->ship_t > 0 && g->ship_t % 4 < 2)            /* the burst blinks through the 40 steps */
        draw_blit(f, g->sx + 1, SALVO_SHIP_Y - 2, BURST, SALVO_ALIEN_W, SALVO_ALIEN_H, DRAW_OR);

    /* the top line, over anything that flew into it */
    draw_fill(f, 0, 0, LS027_W, SALVO_TOP, DRAW_LIGHT);
    label(t, "SCORE ", g->score);
    draw_text(f, 4, 2, 1, t, 1);
    label(t, "WAVE ", g->wave);
    centred(f, 2, 1, t);
    for (int i = 1; i < g->lives; i++)
        draw_blit(f, LS027_W - 4 - 11 * i, 3, LIFE, 9, 5, DRAW_OR);
    draw_line(f, 0, SALVO_TOP - 1, LS027_W - 1, SALVO_TOP - 1, 1);
}

static void app_draw_cb(void *ctx, ls027_frame *f)
{
    salvo_ctx *c = ctx;
    char t[24];
    draw_clear(f);
    if (c->state == TITLE) {
        centred(f, 30, 5, "SALVO");
        centred(f, 96, 2, "4 6 OR SOFT 1 6 MOVE");
        centred(f, 122, 2, "8 9 \xd7 + ENTER FIRE");
        centred(f, 156, 2, "ENTER PLAYS   C ENDS");
        if (c->best > 0) {
            label(t, "BEST ", c->best);
            centred(f, 196, 2, t);
        }
        return;
    }
    draw_play(f, c);
    if (c->state == OVER) {
        draw_fill(f, 80, 56, 240, 104, DRAW_LIGHT);
        draw_frame_rect(f, 80, 56, 240, 104, 1);
        centred(f, 66, 3, "GAME OVER");
        label(t, "SCORE ", c->g.score);
        centred(f, 96, 2, t);
        label(t, "BEST ", c->best);
        centred(f, 116, 2, t);
        if (c->now - c->end_at >= AGAIN_MS)
            centred(f, 142, 1, "ENTER PLAYS");
    }
}

const app_def salvo_app = {"SALVO", sizeof(salvo_ctx), app_load_cb, app_start_cb, app_key_cb, app_tick_cb,
                           app_draw_cb, app_save_cb, app_stop_cb, SAVE_BYTES};
