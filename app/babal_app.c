/* Babal under APPS (babal_app.h). The rules are babalcore's (babal/), unchanged; this file only
 * feeds them the keys and the clock, keeps rows ahead of the ball, and draws. */
#include <string.h>

#include "babal_app.h"
#include "render.h"

#define SAVE_MAGIC 'B'
#define SAVE_VERSION 1
#define SAVE_BYTES 28

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
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

static void babal_load(void *ctx, const uint8_t *save, uint32_t n)
{
    babal_ctx *c = ctx;
    c->skill = BB_SKILL_DEFAULT;
    if (n < SAVE_BYTES || save[0] != SAVE_MAGIC || save[1] != SAVE_VERSION)
        return;
    if (save[2] <= BB_EXPERT)
        c->skill = (bb_skill)save[2];
    for (int i = 0; i < 5; i++)
        c->best[i] = (int32_t)get32(save + 4 + 4 * i);
    c->games = get32(save + 24);
}

static void babal_save(void *ctx, uint8_t *save, uint32_t cap)
{
    babal_ctx *c = ctx;
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    save[2] = (uint8_t)c->skill;
    for (int i = 0; i < 5; i++)
        put32(save + 4 + 4 * i, (uint32_t)c->best[i]);
    put32(save + 24, c->games);
}

static void babal_start(void *ctx)
{
    babal_ctx *c = ctx;
    c->state = BABAL_TITLE;
}

static void babal_stop(void *ctx)
{
    (void)ctx;
}

/* babalgame keeps rows 100 to 200 ahead of the ball, made 100 at a time; a row depends only on
 * the seed, its index and the row before, so making them a few at a time gives the same rows
 * (the host harness checks it against babalcore) without a long tick. */
static void generate_ahead(babal_ctx *c)
{
    intptr_t row = bb_player_level_row(&c->player);
    while (bb_level_last(&c->level) < row + 100)
        bb_level_generate(&c->level, 1);
    for (int i = 0; i < 4 && bb_level_last(&c->level) < row + 200; i++)
        bb_level_generate(&c->level, 1);
}

static void start_game(babal_ctx *c, uint32_t ms)
{
    c->games++;
    c->seed = mix64(((uint64_t)c->games << 32) ^ ms);
    bb_level_init(&c->level, c->seed, c->skill, c->rows, BABAL_ROWS);
    bb_player_init(&c->player, c->log, BABAL_LOG);
    bb_input_steer_init(&c->steer);
    bb_input_jump_init(&c->jump);
    c->last_events.n = 0;
    c->new_best = false;
    bb_level_generate(&c->level, 200);
    c->state = BABAL_PLAY;
}

typedef struct {
    babal_ctx *c;
    bb_instant now;
} adapter;
static bool pop_jump(void *a)
{
    return bb_input_jump_pop_jump(&((adapter *)a)->c->jump, ((adapter *)a)->now);
}
static double pop_steer(void *a)
{
    return bb_input_steer_pop_steer(&((adapter *)a)->c->steer, ((adapter *)a)->now);
}

static void sound(const bb_events *ev)
{
    /* one tone a tick, the most telling one */
    static const struct { bb_event_kind kind; uint16_t hz, ms; } tones[] = {
        {BB_EVENT_FINISH, 1320, 250}, {BB_EVENT_FALL, 150, 150},       {BB_EVENT_OVERDRIVE, 990, 40},
        {BB_EVENT_BOOST, 660, 40},    {BB_EVENT_HIGH_SCORE, 1100, 80}, {BB_EVENT_THOUSAND_SCORE, 880, 30},
        {BB_EVENT_LAND, 220, 20},
    };
    for (size_t t = 0; t < sizeof tones / sizeof tones[0]; t++)
        for (int i = 0; i < ev->n; i++)
            if (ev->v[i].kind == tones[t].kind) {
                app_beep(tones[t].hz, tones[t].ms);
                return;
            }
}

static uint32_t babal_tick(void *ctx, uint32_t ms)
{
    babal_ctx *c = ctx;
    c->now_ms += ms;
    if (c->state != BABAL_PLAY)
        return 200;
    uint32_t step = ms > BABAL_STEP_MAX_MS ? BABAL_STEP_MAX_MS : ms;
    c->last_delta = (double)step / 1000.0;
    generate_ahead(c);
    adapter a = {c, (bb_instant)c->now_ms * 1000000};
    bb_input_query q = {pop_jump, pop_steer, &a};
    bb_player_tick(&c->player, c->last_delta, &c->level, &q, &c->last_events);
    sound(&c->last_events);
    for (int i = 0; i < c->last_events.n; i++)
        if (c->last_events.v[i].kind == BB_EVENT_FINISH) {
            c->final_score = bb_player_score(&c->player);
            if (c->final_score > c->best[c->skill]) {
                c->best[c->skill] = c->final_score;
                c->new_best = true;
            }
            c->state = BABAL_DONE;
            c->done_ms = c->now_ms;
        }
    return BABAL_TICK_MS;
}

static bool babal_key(void *ctx, int key, bool down, uint32_t ms)
{
    babal_ctx *c = ctx;
    bb_instant t = (bb_instant)ms * 1000000;
    switch (c->state) {
    case BABAL_TITLE:
        if (!down)
            return false;
        if (key == BABAL_KEY_LEFT)
            c->skill = (bb_skill)((c->skill + 4) % 5);
        else if (key == BABAL_KEY_RIGHT)
            c->skill = (bb_skill)((c->skill + 1) % 5);
        else if (key >= 0 && key <= 4) {
            c->skill = (bb_skill)key;
            start_game(c, ms);
        } else if (key == BABAL_KEY_ENTER || key == BABAL_KEY_JUMP)
            start_game(c, ms);
        return true;
    case BABAL_PLAY:
        if (key == BABAL_KEY_LEFT)
            bb_input_steer_left_key(&c->steer, 1, 1.0, down, t);
        else if (key == BABAL_KEY_RIGHT)
            bb_input_steer_right_key(&c->steer, 2, 1.0, down, t);
        else if ((key == BABAL_KEY_JUMP || key == BABAL_KEY_ENTER) && down)
            bb_input_jump_push_jump(&c->jump, t);
        return false; /* the picture changes on the tick */
    default:
        if (down && ms - c->done_ms >= 600) {
            c->state = BABAL_TITLE;
            return true;
        }
        return false;
    }
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

static int centred(int len, int scale)
{
    return (LS027_W - len * 6 * scale) / 2;
}

static void babal_draw(void *ctx, ls027_frame *f)
{
    babal_ctx *c = ctx;
    char text[32];
    if (c->state == BABAL_TITLE) {
        static const char *const names[] = {"NOOB", "EASY", "MEDIUM", "HARD", "EXPERT"};
        memset(f, 0, sizeof *f);
        babal_text(f, centred(5, 6), 30, 6, "BABAL");
        int len = (int)strlen(names[c->skill]) + 4;
        char sel[16] = "< ";
        strcat(sel, names[c->skill]);
        strcat(sel, " >");
        babal_text(f, centred(len, 3), 110, 3, sel);
        if (c->best[c->skill] > 0) {
            number(text, "BEST ", c->best[c->skill]);
            babal_text(f, centred((int)strlen(text), 2), 150, 2, text);
        }
        babal_text(f, centred(28, 2), 196, 2, "4 6 SKILL   8 ENTER JUMP");
        babal_text(f, centred(20, 2), 218, 2, "ENTER PLAYS   C ENDS");
        return;
    }
    babal_hud hud = {bb_player_score(&c->player), c->best[c->skill], 0, NULL};
    hud.progress = (int32_t)(bb_player_time_progress_done(&c->player, &c->level) * 1000.0 /
                             bb_player_time_progress_total(&c->player));
    if (c->state == BABAL_DONE) {
        hud.message = c->new_best ? "NEW BEST" : "FINISH";
        hud.score = c->final_score;
    }
    babal_render(f, &c->level, &c->player, &hud);
}

const app_def babal_app = {"BABAL", sizeof(babal_ctx), babal_load, babal_start, babal_key, babal_tick,
                           babal_draw, babal_save, babal_stop};
