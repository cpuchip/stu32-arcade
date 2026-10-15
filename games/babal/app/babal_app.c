/* SPDX-License-Identifier: MIT */
/* Babal under APPS (babal_app.h). The rules are babalcore's (games/babal/core/), unchanged; this file only
 * feeds them the keys and the clock, keeps rows ahead of the ball, and draws. */
#include <string.h>

#include "babal_app.h"
#include "render.h"

#define SAVE_MAGIC 'B'
#define SAVE_VERSION 2
#define SAVE_BYTES 33 /* magic, version, skill, count, 5 x (points, skill), games */
#define SAVE_V1_BYTES 28

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

/* babalgame's Scores.gd add_score: the list keeps the five highest, highest first (here a new
 * score goes after the equal ones already there). */
static void add_score(babal_ctx *c, int32_t points, bb_skill skill)
{
    int at = c->n_scores;
    while (at > 0 && c->scores[at - 1].points < points)
        at--;
    if (at >= BABAL_SCORES)
        return;
    int last = c->n_scores < BABAL_SCORES ? c->n_scores : BABAL_SCORES - 1;
    for (int k = last; k > at; k--)
        c->scores[k] = c->scores[k - 1];
    c->scores[at].points = points;
    c->scores[at].skill = (uint8_t)skill;
    if (c->n_scores < BABAL_SCORES)
        c->n_scores++;
}

/* babalgame's get_best_score_points: the top of the list, over all skills, 0 for none */
static int32_t best_score(const babal_ctx *c)
{
    return c->n_scores ? c->scores[0].points : 0;
}

static void babal_load(void *ctx, const uint8_t *save, uint32_t n)
{
    babal_ctx *c = ctx;
    c->skill = BB_SKILL_DEFAULT;
    /* taken whole or not at all */
    if (n < SAVE_V1_BYTES || save[0] != SAVE_MAGIC || save[2] > BB_EXPERT)
        return;
    if (save[1] == 1) { /* the first save: a best per skill, kept as the list */
        for (int i = 0; i < 5; i++)
            if ((int32_t)get32(save + 4 + 4 * i) < 0)
                return; /* checked whole before anything is taken (and no ctx on the stack: it is 164 KB) */
        c->n_scores = 0;
        for (int i = 0; i < 5; i++) {
            int32_t b = (int32_t)get32(save + 4 + 4 * i);
            if (b > 0)
                add_score(c, b, (bb_skill)i);
        }
        c->skill = (bb_skill)save[2];
        c->games = get32(save + 24);
        return;
    }
    if (n < SAVE_BYTES || save[1] != SAVE_VERSION || save[3] > BABAL_SCORES)
        return;
    for (int i = 0; i < save[3]; i++) {
        const uint8_t *e = save + 4 + 5 * i;
        if ((int32_t)get32(e) < 0 || e[4] > BB_EXPERT || (i > 0 && (int32_t)get32(e) > (int32_t)get32(e - 5)))
            return;
    }
    c->skill = (bb_skill)save[2];
    c->n_scores = save[3];
    for (int i = 0; i < c->n_scores; i++) {
        c->scores[i].points = (int32_t)get32(save + 4 + 5 * i);
        c->scores[i].skill = save[4 + 5 * i + 4];
    }
    c->games = get32(save + 29);
}

static void babal_save(void *ctx, uint8_t *save, uint32_t cap)
{
    babal_ctx *c = ctx;
    /* leaving a game unfinished keeps its score, as babalgame's back and restart do */
    if (c->state == BABAL_PLAY && !c->score_saved) {
        add_score(c, bb_player_score(&c->player), c->skill);
        c->score_saved = true;
    }
    if (cap < SAVE_BYTES)
        return;
    memset(save, 0, SAVE_BYTES);
    save[0] = SAVE_MAGIC;
    save[1] = SAVE_VERSION;
    save[2] = (uint8_t)c->skill;
    save[3] = (uint8_t)c->n_scores;
    for (int i = 0; i < c->n_scores; i++) {
        put32(save + 4 + 5 * i, (uint32_t)c->scores[i].points);
        save[4 + 5 * i + 4] = c->scores[i].skill;
    }
    put32(save + 29, c->games);
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

/* a side's keys held together steer as one: babalcore sees the side go down with the first and up with the last */
static void steer(babal_ctx *c, uint8_t *held, uint8_t bit, bool down, bool right, bb_instant t)
{
    bool was = *held != 0;
    *held = (uint8_t)(down ? *held | bit : *held & ~bit);
    bool now = *held != 0;
    if (now == was)
        return;
    if (right)
        bb_input_steer_right_key(&c->steer, 2, 1.0, now, t);
    else
        bb_input_steer_left_key(&c->steer, 1, 1.0, now, t);
}

static void start_game(babal_ctx *c, uint32_t ms)
{
    c->games++;
    /* the framework's seed (the device's hardware RNG, apps.h v1.2) folded into the game count and the
       start key's ms: entropy on the device, and still a new level each game where a host fixes it */
    c->seed = mix64(mix64(app_seed()) ^ ((uint64_t)c->games << 32) ^ ms);
    bb_level_init(&c->level, c->seed, c->skill, c->rows, BABAL_ROWS);
    bb_player_init(&c->player, c->log, c->log_present, BABAL_LOG);
    /* babalgame's update_player_best_score at each start: the best over all skills */
    bb_player_set_best_score(&c->player, best_score(c));
    bb_input_steer_init(&c->steer);
    bb_input_jump_init(&c->jump);
    c->left_held = c->right_held = 0;
    c->last_events.n = 0;
    c->new_record = false;
    c->score_saved = false;
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
    for (int i = 0; i < c->last_events.n; i++) {
        const bb_event *e = &c->last_events.v[i];
        if (e->kind == BB_EVENT_HIGH_SCORE)
            c->new_record = true; /* babalgame's _on_Ball_high_score */
        if (e->kind == BB_EVENT_FINISH) { /* babalgame's _on_Ball_finish */
            c->final_score = e->arg;
            add_score(c, e->arg, c->skill);
            c->score_saved = true;
            c->state = BABAL_DONE;
            c->done_ms = c->now_ms;
        }
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
        else if (key >= APP_KEY_SOFT1 && key <= APP_KEY_SOFT5) {
            c->skill = (bb_skill)(BB_NOOB + (key - APP_KEY_SOFT1));
            start_game(c, ms);
        } else if (key == BABAL_KEY_ENTER || key == BABAL_KEY_JUMP)
            start_game(c, ms);
        return true;
    case BABAL_PLAY:
        if (key == BABAL_KEY_LEFT || key == BABAL_KEY_LEFT2)
            steer(c, &c->left_held, key == BABAL_KEY_LEFT ? 1 : 2, down, false, t);
        else if (key == BABAL_KEY_RIGHT || key == BABAL_KEY_RIGHT2)
            steer(c, &c->right_held, key == BABAL_KEY_RIGHT ? 1 : 2, down, true, t);
        else if ((key == BABAL_KEY_JUMP || key == BABAL_KEY_ENTER || key == BABAL_KEY_JUMP2 || key == BABAL_KEY_JUMP3
                  || key == BABAL_KEY_JUMP4) && down)
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

static const char *const SKILL_WORDS[] = {"noob", "easy", "medium", "hard", "expert"};

/* The scores list on the finish screen: rank, skill and points (babalgame's pretty_print, less the
 * date), centred, on a light box below the message. */
static void score_list(ls027_frame *f, const babal_ctx *c)
{
    for (int y = 128; y < 128 + 18 * BABAL_SCORES + 6; y++)
        for (int x = 40; x < LS027_W - 40; x++)
            f->px[y][x >> 3] &= (uint8_t)~(1u << (x & 7));
    for (int i = 0; i < c->n_scores; i++) {
        char line[40], pts[16];
        number(pts, "", c->scores[i].points);
        int n = 0;
        line[n++] = (char)('1' + i);
        line[n++] = '.';
        line[n++] = ' ';
        for (const char *w = SKILL_WORDS[c->scores[i].skill]; *w; w++)
            line[n++] = *w;
        line[n++] = ' ';
        line[n++] = '-';
        line[n++] = ' ';
        for (const char *d = pts; *d; d++)
            line[n++] = *d;
        line[n] = '\0';
        babal_text(f, centred(n, 2), 132 + 18 * i, 2, line);
    }
}

static void babal_draw(void *ctx, ls027_frame *f)
{
    babal_ctx *c = ctx;
    char text[32];
    if (c->state == BABAL_TITLE) {
        static const char *const names[] = {"NOOB", "EASY", "MEDIUM", "HARD", "EXPERT"};
        memset(f, 0, sizeof *f);
        babal_text(f, centred(5, 6), 30, 6, "BABAL");
        /* the original's credit (abacus decision 62): its author here, its home above the help */
        babal_text(f, centred(20, 2), 84, 2, "by Christian Mauduit");
        int len = (int)strlen(names[c->skill]) + 4;
        char sel[16] = "< ";
        strcat(sel, names[c->skill]);
        strcat(sel, " >");
        babal_text(f, centred(len, 3), 110, 3, sel);
        if (best_score(c) > 0) {
            number(text, "BEST ", best_score(c));
            babal_text(f, centred((int)strlen(text), 2), 150, 2, text);
        }
        babal_text(f, centred(27, 2), 172, 2, "gitlab.com/liberecofr/babal");
        static const char *const help[] = {"4 6 SKILL   ENTER PLAYS",
                                           "PLAY: STEER 4 6 OR SOFT 1 6, JUMP 8 9 \xd7 + OR ENTER", "C ENDS"};
        babal_text(f, centred((int)strlen(help[0]), 2), 196, 2, help[0]);
        babal_text(f, centred((int)strlen(help[1]), 1), 218, 1, help[1]);
        babal_text(f, centred((int)strlen(help[2]), 1), 228, 1, help[2]);
        return;
    }
    /* babalgame shows the score and the progress while playing, no best */
    babal_hud hud = {bb_player_score(&c->player), 0, 0, NULL};
    hud.progress = (int32_t)(bb_player_time_progress_done(&c->player, &c->level) * 1000.0 /
                             bb_player_time_progress_total(&c->player));
    if (c->state == BABAL_DONE) {
        /* babalgame's _on_Ball_finish, then its scores list */
        hud.message = c->new_record ? "New record!" : "Game over";
        hud.score = c->final_score;
    }
    babal_render(f, &c->level, &c->player, &hud);
    if (c->state == BABAL_DONE)
        score_list(f, c);
}

const app_def babal_app = {"BABAL", sizeof(babal_ctx), babal_load, babal_start, babal_key, babal_tick,
                           babal_draw, babal_save, babal_stop,
                           0 /* save_size: APP_SAVE_SIZE, 33 of its 64 bytes used */};
