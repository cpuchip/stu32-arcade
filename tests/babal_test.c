/* babalcore's own tests (unit and doc), commit fa985cd, ported one to one. Each function
 * names its source. Not portable and so not here: time_helper.rs (Instant::now, the rules
 * take the time from the caller) and type_helper.rs (Rust type names). The runner fails
 * when no check ran. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../babal/babal.h"

static int checks, failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d %s: %s\n", __FILE__, __LINE__, __func__, #cond); \
        }                                                                    \
    } while (0)
#define CHECK_STR(want, got) CHECK(strcmp((want), (got)) == 0)

static bb_row storage[BB_SLIDING_LENGTH_MAX];

static void level_new(bb_level *lvl, uint64_t seed, bb_skill skill)
{
    bb_level_init(lvl, seed, skill, storage, BB_SLIDING_LENGTH_MAX);
}

/* Display for Level: each row cut to the width, joined by newlines. */
static char *level_format(const bb_level *lvl)
{
    static char out[4096];
    size_t n = 0, w = bb_level_width(lvl);
    out[0] = '\0';
    for (size_t i = 0; i < bb_level_len(lvl) && n + w + 2 < sizeof out; i++) {
        char text[BB_WIDTH_MAX + 1];
        bb_row_format(bb_level_row_at(lvl, i), text);
        if (i)
            out[n++] = '\n';
        memcpy(out + n, text, w);
        n += w;
        out[n] = '\0';
    }
    return out;
}

/* maths.rs: test_fade_in, test_fade_out, test_heaviside_step and the doctests */
static void maths_tests(void)
{
    CHECK(0.0 == bb_fade_in(-100.0));
    CHECK(0.0 == bb_fade_in(-0.9));
    CHECK(0.0 == bb_fade_in(-0.1));
    CHECK(0.0 == bb_fade_in(0.0));
    CHECK(0.25 == bb_fade_in(0.25));
    CHECK(0.5 == bb_fade_in(0.5));
    CHECK(0.75 == bb_fade_in(0.75));
    CHECK(1.0 == bb_fade_in(1.0));
    CHECK(1.0 == bb_fade_in(1.1));
    CHECK(1.0 == bb_fade_in(1.9));
    CHECK(1.0 == bb_fade_in(100.0));
    CHECK(0.0 != bb_fade_in(NAN));
    CHECK(0.5 != bb_fade_in(NAN));
    CHECK(1.0 != bb_fade_in(NAN));

    CHECK(1.0 == bb_fade_out(-100.0));
    CHECK(1.0 == bb_fade_out(-0.9));
    CHECK(1.0 == bb_fade_out(-0.1));
    CHECK(1.0 == bb_fade_out(0.0));
    CHECK(0.75 == bb_fade_out(0.25));
    CHECK(0.5 == bb_fade_out(0.5));
    CHECK(0.25 == bb_fade_out(0.75));
    CHECK(0.0 == bb_fade_out(1.0));
    CHECK(0.0 == bb_fade_out(1.1));
    CHECK(0.0 == bb_fade_out(1.9));
    CHECK(0.0 == bb_fade_out(100.0));
    CHECK(0.0 != bb_fade_out(NAN));
    CHECK(0.5 != bb_fade_out(NAN));
    CHECK(1.0 != bb_fade_out(NAN));

    CHECK(0.0 == bb_heaviside_step(-100.0));
    CHECK(0.0 == bb_heaviside_step(-0.9));
    CHECK(0.0 == bb_heaviside_step(-0.1));
    CHECK(0.5 == bb_heaviside_step(0.0));
    CHECK(1.0 == bb_heaviside_step(0.1));
    CHECK(1.0 == bb_heaviside_step(0.5));
    CHECK(1.0 == bb_heaviside_step(0.9));
    CHECK(1.0 == bb_heaviside_step(1.1));
    CHECK(1.0 == bb_heaviside_step(1.9));
    CHECK(1.0 == bb_heaviside_step(100.0));
    CHECK(0.0 != bb_heaviside_step(NAN));
    CHECK(0.5 != bb_heaviside_step(NAN));
    CHECK(1.0 != bb_heaviside_step(NAN));

    /* doctests */
    CHECK(0.0 == bb_fade_in(-0.5));
    CHECK(0.0 == bb_fade_in(0.0));
    CHECK(0.5 == bb_fade_in(0.5));
    CHECK(1.0 == bb_fade_in(1.0));
    CHECK(1.0 == bb_fade_in(1.5));
    CHECK(1.0 == bb_fade_out(-0.5));
    CHECK(1.0 == bb_fade_out(0.0));
    CHECK(0.5 == bb_fade_out(0.5));
    CHECK(0.0 == bb_fade_out(1.0));
    CHECK(0.0 == bb_fade_out(1.5));
    CHECK(-1.0 == bb_square_wave(-0.25));
    CHECK(1.0 == bb_square_wave(0.25));
    CHECK(-1.0 == bb_square_wave(0.75));
    CHECK(1.0 == bb_square_wave(1.25));
    CHECK(-1.0 == bb_triangle_wave(-0.25));
    CHECK(1.0 == bb_triangle_wave(0.25));
    CHECK(-1.0 == bb_triangle_wave(0.75));
    CHECK(1.0 == bb_triangle_wave(1.25));
    CHECK(-0.5 == bb_sawtooth_wave(-0.25));
    CHECK(0.5 == bb_sawtooth_wave(0.25));
    CHECK(-0.5 == bb_sawtooth_wave(0.75));
    CHECK(0.5 == bb_sawtooth_wave(1.25));
    CHECK(-1.0 == bb_sine_wave(-0.25));
    CHECK(0.0 == bb_sine_wave(0.0));
    CHECK(1.0 == bb_sine_wave(0.25));
    CHECK(0.0 == bb_pulse(-0.125, 0.25));
    CHECK(1.0 == bb_pulse(0.125, 0.25));
    CHECK(0.0 == bb_pulse(0.375, 0.25));
    CHECK(1.0 == bb_pulse(1.125, 0.25));
    CHECK(0.0 == bb_boxcar(-0.5));
    CHECK(0.5 == bb_boxcar(0.0));
    CHECK(1.0 == bb_boxcar(0.5));
    CHECK(0.5 == bb_boxcar(1.0));
    CHECK(0.0 == bb_boxcar(1.5));
    CHECK(0.0 == bb_heaviside_step(-0.5));
    CHECK(0.5 == bb_heaviside_step(0.0));
    CHECK(1.0 == bb_heaviside_step(0.5));
}

/* slab_kind.rs, slab_def.rs, slab.rs: test_fmt */
static void slab_tests(void)
{
    CHECK(' ' == bb_slab_kind_char(BB_KIND_VOID));
    CHECK('#' == bb_slab_kind_char(BB_KIND_FLOOR));
    CHECK('^' == bb_slab_kind_char(BB_KIND_BOOST));
    CHECK('O' == bb_slab_kind_char(BB_KIND_OVERDRIVE));
    CHECK('*' == bb_slab_kind_char(BB_KIND_HIGHLIGHT));
    CHECK(' ' == bb_slab_def_char(BB_DEF_VOID));
    CHECK('#' == bb_slab_def_char(BB_DEF_FLOOR));
    CHECK('^' == bb_slab_def_char(BB_DEF_BOOST));
    bb_slab v = bb_slab_new(BB_DEF_VOID), f = bb_slab_new(BB_DEF_FLOOR), b = bb_slab_new(BB_DEF_BOOST);
    CHECK(' ' == bb_slab_kind_char(bb_slab_kind_of(&v, 0)));
    CHECK('#' == bb_slab_kind_char(bb_slab_kind_of(&f, 0)));
    CHECK('^' == bb_slab_kind_char(bb_slab_kind_of(&b, 0)));
}

/* row.rs: test_fmt and the doctests */
static void row_tests(void)
{
    bb_row row;
    char text[BB_WIDTH_MAX + 1];
    bb_row_new(&row, BB_DEF_VOID);
    bb_row_format(&row, text);
    CHECK_STR("                                ", text);
    bb_row_new(&row, BB_DEF_FLOOR);
    bb_row_format(&row, text);
    CHECK_STR("################################", text);

    bb_row_new(&row, BB_DEF_FLOOR);
    CHECK(BB_KIND_FLOOR == bb_row_slab_kind(&row, 5, 123));
    bb_row_new(&row, BB_DEF_VOID); /* Row::default() */
    CHECK(BB_KIND_VOID == bb_row_slab_kind(&row, 3, 0));
    bb_row_set(&row, 3, BB_DEF_FLOOR);
    CHECK(BB_KIND_FLOOR == bb_row_slab_kind(&row, 3, 0));
    CHECK(BB_KIND_VOID == bb_row_slab_kind(&row, 2, 0));
}

/* time_tracker.rs: test_fmt and the doctests */
static void time_tracker_tests(void)
{
    char text[32];
    bb_time_tracker tt = {0.0};
    bb_time_tracker_add(&tt, 2.0);
    bb_time_tracker_add(&tt, 0.05);
    bb_time_tracker_format(&tt, text);
    CHECK_STR("2.050", text);

    tt.now_sec_f64 = 0.0;
    bb_time_tracker_add(&tt, 0.01);
    CHECK(0.01 == bb_time_tracker_now_sec(&tt));
    tt.now_sec_f64 = 0.0;
    CHECK(0.0 == bb_time_tracker_now_sec(&tt));
    bb_time_tracker_add(&tt, 0.003);
    CHECK(0.003 == bb_time_tracker_now_sec(&tt));
    tt.now_sec_f64 = 0.0;
    CHECK(0 == bb_time_tracker_now_msec(&tt));
    bb_time_tracker_add(&tt, 0.003);
    CHECK(3 == bb_time_tracker_now_msec(&tt));
}

/* player_event_kind.rs and player_event.rs: test_fmt */
static void player_event_tests(void)
{
    static const char *const want[] = {"start", "jump", "land", "fall", "catch_up", "change_column",
                                       "boost", "overdrive", "thousand_score", "percent_time", "high_score", "finish"};
    for (int k = BB_EVENT_START; k <= BB_EVENT_FINISH; k++)
        CHECK_STR(want[k], bb_event_kind_as_str((bb_event_kind)k));
    char text[32];
    for (int k = BB_EVENT_START; k <= BB_EVENT_OVERDRIVE; k++) {
        bb_event e = {(bb_event_kind)k, false, 0};
        bb_event_format(&e, text);
        CHECK_STR(want[k], text);
    }
    bb_event e = {BB_EVENT_THOUSAND_SCORE, true, 42};
    bb_event_format(&e, text);
    CHECK_STR("thousand_score: 42", text);
    e.kind = BB_EVENT_PERCENT_TIME;
    bb_event_format(&e, text);
    CHECK_STR("percent_time: 42", text);
    e.kind = BB_EVENT_HIGH_SCORE;
    bb_event_format(&e, text);
    CHECK_STR("high_score: 42", text);
    e.kind = BB_EVENT_FINISH;
    bb_event_format(&e, text);
    CHECK_STR("finish: 42", text);
}

/* input_jump.rs: test_default, test_set_stickiness_secs_f64, test_push_pop_without_stickiness,
 * test_push_pop_with_stickiness, test_push_pop_rearm */
static void input_jump_tests(void)
{
    const bb_instant now = 1000000000000; /* any instant */
    bb_input_jump ij;

    bb_input_jump_init(&ij);
    CHECK(ij.has_stickiness && ij.stickiness == (bb_duration)bb_duration_from_secs_f64(BB_DEFAULT_STICKINESS_SECS_F64));

    bb_input_jump_init(&ij);
    CHECK(ij.has_stickiness && ij.stickiness == (bb_duration)bb_duration_from_secs_f64(BB_DEFAULT_STICKINESS_SECS_F64));
    bb_input_jump_set_stickiness_secs_f64(&ij, false, 0.0);
    CHECK(!ij.has_stickiness);
    bb_input_jump_set_stickiness_secs_f64(&ij, true, 0.5);
    CHECK(ij.has_stickiness && ij.stickiness == (bb_duration)bb_duration_from_secs_f64(0.5));

    bb_input_jump_init(&ij);
    bb_input_jump_set_stickiness_secs_f64(&ij, false, 0.0);
    CHECK(!bb_input_jump_pop_jump(&ij, now));
    bb_input_jump_push_jump(&ij, now);
    CHECK(bb_input_jump_pop_jump(&ij, now));
    CHECK(!bb_input_jump_pop_jump(&ij, now));
    bb_input_jump_push_jump(&ij, now);
    bb_input_jump_push_jump(&ij, now);
    bb_input_jump_push_jump(&ij, now);
    CHECK(bb_input_jump_pop_jump(&ij, now));
    CHECK(!bb_input_jump_pop_jump(&ij, now));

    bb_input_jump_init(&ij);
    bb_duration short_enough = (bb_duration)bb_duration_from_secs_f64(BB_DEFAULT_STICKINESS_SECS_F64 - 0.001);
    bb_duration too_long = (bb_duration)bb_duration_from_secs_f64(BB_DEFAULT_STICKINESS_SECS_F64 + 0.001);
    CHECK(!bb_input_jump_pop_jump(&ij, now));
    bb_input_jump_push_jump(&ij, now);
    CHECK(bb_input_jump_pop_jump(&ij, now));
    CHECK(bb_input_jump_pop_jump(&ij, now));
    CHECK(bb_input_jump_pop_jump(&ij, now + short_enough));
    CHECK(bb_input_jump_pop_jump(&ij, now + short_enough));
    CHECK(!bb_input_jump_pop_jump(&ij, now + too_long));
    CHECK(!bb_input_jump_pop_jump(&ij, now + short_enough));
    CHECK(!bb_input_jump_pop_jump(&ij, now));

    bb_input_jump_init(&ij);
    bb_duration extra_delay = (bb_duration)bb_duration_from_secs_f64(BB_DEFAULT_STICKINESS_SECS_F64 * 3.0);
    CHECK(!bb_input_jump_pop_jump(&ij, now));
    bb_input_jump_push_jump(&ij, now);
    bb_input_jump_push_jump(&ij, now + extra_delay);
    bb_input_jump_push_jump(&ij, now);
    CHECK(bb_input_jump_pop_jump(&ij, now + extra_delay));
}

/* level.rs: test_fmt, test_sliding_length_max, test_generate and the doctests */
static void level_tests(void)
{
    bb_level lvl;
    level_new(&lvl, 0, BB_EASY);
    CHECK_STR("", level_format(&lvl));
    level_new(&lvl, 0, BB_NOOB);
    bb_level_generate(&lvl, 3);
    CHECK_STR("########\n########\n########", level_format(&lvl));

    level_new(&lvl, 0, BB_NOOB);
    CHECK(0 == bb_level_len(&lvl));
    bb_row row;
    bb_row_new(&row, BB_DEF_FLOOR);
    bb_level_generate(&lvl, BB_SLIDING_LENGTH_MAX - 1);
    CHECK(BB_SLIDING_LENGTH_MAX - 1 == bb_level_len(&lvl));
    bb_level_generate(&lvl, 2);
    CHECK(BB_SLIDING_LENGTH_MAX == bb_level_len(&lvl));
    CHECK(1 == bb_level_first(&lvl));
    CHECK(BB_SLIDING_LENGTH_MAX + 1 == bb_level_last(&lvl));
    bb_level_push(&lvl, &row);
    CHECK(BB_SLIDING_LENGTH_MAX == bb_level_len(&lvl));
    CHECK(2 == bb_level_first(&lvl));
    CHECK(BB_SLIDING_LENGTH_MAX + 2 == bb_level_last(&lvl));

    static const struct { uint64_t seed; const char *want; } expected[] = {
        {0, "### ######\n####### ##\n#^##### ##"},
        {123, "#  ## # ##\n^  ## #^##\n###   # ##"},
    };
    for (int i = 0; i < 2; i++) {
        level_new(&lvl, expected[i].seed, BB_MEDIUM);
        bb_level_generate(&lvl, 3);
        CHECK(3 == bb_level_len(&lvl));
        CHECK_STR(expected[i].want, level_format(&lvl));
    }

    /* doctests */
    level_new(&lvl, 0, BB_EASY);
    bb_level_push(&lvl, &row);
    CHECK(1 == bb_level_len(&lvl));
    level_new(&lvl, 0, BB_EASY);
    bb_level_generate(&lvl, 2);
    CHECK(2 == bb_level_len(&lvl));
    bb_level_generate(&lvl, 3);
    CHECK(5 == bb_level_len(&lvl));
    level_new(&lvl, 0, BB_EASY);
    bb_level_generate(&lvl, 2);
    CHECK(BB_KIND_FLOOR == bb_level_slab_kind(&lvl, 5, 1, false));
    level_new(&lvl, 0, BB_EASY);
    CHECK(9 == bb_level_width(&lvl));
    CHECK(0 == bb_level_len(&lvl));
    CHECK(0 == bb_level_first(&lvl));
    CHECK(0 == bb_level_last(&lvl));
    bb_level_generate(&lvl, 2);
    CHECK(0 == bb_level_item(&lvl, 5, 1, false));
    level_new(&lvl, 42, BB_EASY);
    intptr_t c, r;
    CHECK(!bb_level_find_start_spot(&lvl, 2, 1, &c, &r));
    bb_level_generate(&lvl, 100);
    CHECK(bb_level_find_start_spot(&lvl, 2, 1, &c, &r) && c == 2 && r == 1);
    level_new(&lvl, 42, BB_EASY);
    CHECK(BB_EASY == bb_level_skill(&lvl));
}

int main(void)
{
    maths_tests();
    slab_tests();
    row_tests();
    time_tracker_tests();
    player_event_tests();
    input_jump_tests();
    level_tests();
    if (checks == 0) {
        printf("no checks ran\n");
        return 1;
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
