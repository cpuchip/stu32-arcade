/* SPDX-License-Identifier: MIT */
/* The C side of the oracle: prints what oracle/ref (babalcore) prints, from games/babal/core/.
 * Usage: driver pins | level SEED SKILL N | game SCRIPT | points SEED | inputs SEED */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../games/babal/core/babal.h"
#include "../games/babal/core/internal.h"
#include "oracle.h"

static uint64_t sm_state;
static uint64_t sm_next(void)
{
    sm_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = sm_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static double sm_unit(void)
{
    return (double)(sm_next() >> 11) * (1.0 / 9007199254740992.0);
}

static void pins(void)
{
    char a1[17], a2[17], a3[17], a4[17], a5[17], a6[17], a7[17], a8[17], a9[17], a10[17];
    char f1[9];
    sm_state = 0x5eed;
    uint64_t seeds[64][2] = {{0, 0}, {0, 1}, {123, 0}, {UINT64_MAX, UINT64_MAX}};
    for (int i = 4; i < 64; i++) {
        seeds[i][0] = sm_next();
        seeds[i][1] = sm_next() % 20000;
    }
    for (int i = 0; i < 64; i++) {
        uint8_t buf[32] = {0};
        for (int k = 0; k < 8; k++) {
            buf[k] = (uint8_t)(seeds[i][0] >> (8 * k));
            buf[8 + k] = (uint8_t)(seeds[i][1] >> (8 * k));
        }
        bb_chacha20 rng;
        bb_chacha20_from_seed(&rng, buf);
        outf("chacha %" PRIu64 " %" PRIu64, seeds[i][0], seeds[i][1]);
        for (int k = 0; k < 70; k++)
            outf(" %s", b64(bb_chacha20_random_f64(&rng), a1));
        outf("\n");
    }
    static const double ts[] = {0.0, 1.0, 0.5, 0.25, 1.0 / 3.0};
    for (int k = 0; k < 300; k++) {
        bb_vec3 a, b;
        a.x = sm_unit() * 2000.0 - 1000.0;
        a.y = sm_unit() * 800.0 - 400.0;
        a.z = sm_unit() * 1e6;
        b.x = sm_unit() * 2000.0 - 1000.0;
        b.y = sm_unit() * 800.0 - 400.0;
        b.z = sm_unit() * 1e6;
        double t = k < 5 ? ts[k] : sm_unit();
        bb_vec3 r = bb_vec3_lerp(a, b, t);
        outf("lerp %s %s %s %s %s %s %s -> %s %s %s\n", b64(a.x, a1), b64(a.y, a2), b64(a.z, a3), b64(b.x, a4),
               b64(b.y, a5), b64(b.z, a6), b64(t, a7), b64(r.x, a8), b64(r.y, a9), b64(r.z, a10));
    }
    double secs[17 + 300 + 100] = {0.16, 0.0, 1e-9, 0.5e-9, 1.5e-9, 2.5e-9, 0.999999999, 1.0, 0.16 * 0.5, 0.16 * 1.5, 0.16 * 3.0,
                                   1.0 / 1024.0, 3.0 / 1024.0, 5.0 / 1024.0, 7.0 / 1024.0, 1e-10, 3.0 * 1e-10};
    int n = 17;
    for (int i = 0; i < 300; i++)
        secs[n++] = sm_unit() * 3.0;
    for (int i = 0; i < 100; i++) {
        double whole = (double)(sm_next() % 3000000000ULL) / 1e9;
        secs[n++] = whole + (sm_unit() - 0.5) * 1e-9;
    }
    for (int i = 0; i < n; i++)
        if (secs[i] >= 0.0)
            outf("from_secs_f64 %s %" PRIu64 "\n", b64(secs[i], a1), bb_duration_from_secs_f64(secs[i]));
    static const uint64_t fixed_ns[] = {0, 1, 999999999, 1000000000, 160000000};
    for (int k = 0; k < 300; k++) {
        uint64_t ns = k < 5 ? fixed_ns[k] : sm_next() % 100000000000000ULL;
        outf("as_secs_f64 %" PRIu64 " %s\n", ns, b64(bb_duration_as_secs_f64(ns), a1));
    }
    double xs[8 + 300 + 50] = {0.5, -0.5, 1.5, 2.5, -2.5, 0.49999999999999994, 4503599627370495.5, -0.0};
    n = 8;
    for (int i = 0; i < 300; i++)
        xs[n++] = (sm_unit() - 0.5) * 2e4;
    for (int i = 0; i < 50; i++)
        xs[n++] = (double)(sm_next() % 20000) * 0.5 - 5000.0;
    for (int i = 0; i < n; i++) {
        double x = xs[i];
        outf("num %s round=%s fract=%s trunc_i=%" PRId64 " floor=%s f32=%s\n", b64(x, a1), b64(round(x), a2),
               b64(x - trunc(x), a3), bb_f64_as_i64(x), b64(floor(x), a4), b32((float)x, f1));
    }
    static const double big[] = {9.3e9, 1e10, 1.8e10, 1.8446744073709550e10, 1.8446744073709552e10,
                                 1.8446744073709553e10, 1e11, 1e15};
    for (int i = 0; i < 8; i++)
        outf("from_secs_f64 %s %" PRIu64 "\n", b64(big[i], a1), bb_duration_from_secs_f64(big[i]));
    double mx[18 + 200] = {-1.5, -1.0, -0.75, -0.5, -0.25, -0.0, 0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5,
                           NAN, INFINITY, -INFINITY, 1e-300, 0.9999999999999999};
    n = 18;
    for (int i = 0; i < 200; i++)
        mx[n++] = (sm_unit() - 0.5) * 6.0;
    static const double alphas[] = {0.0, 0.25, 0.5, 1.0, 1.5, -0.5};
    for (int i = 0; i < n; i++) {
        double x = mx[i];
        outf("maths %s %s %s %s %s %s %s %s %s", b64(x, a1), b64(bb_fade_in(x), a2), b64(bb_fade_out(x), a3),
               b64(bb_square_wave(x), a4), b64(bb_triangle_wave(x), a5), b64(bb_sawtooth_wave(x), a6),
               b64(bb_sine_wave(x), a7), b64(bb_boxcar(x), a8), b64(bb_heaviside_step(x), a9));
        for (int k = 0; k < 6; k++)
            outf(" %s", b64(bb_pulse(x, alphas[k]), a1));
        outf("\n");
    }
    static const double adds[] = {0.5, 1e12, 0.25, 1e12 - 0.25, 0.25, -1.0, 0.001, 0.0005, 2.0005, 1e11, 9e11, 0.123456, -0.2};
    bb_time_tracker tt = {0.0};
    for (int i = 0; i < 13; i++) {
        char text[32];
        bb_time_tracker_add(&tt, adds[i]);
        bb_time_tracker_format(&tt, text);
        outf("tt %s %s %" PRId64 " %s\n", b64(adds[i], a1), b64(bb_time_tracker_now_sec(&tt), a2),
               bb_time_tracker_now_msec(&tt), text);
    }
    static const int32_t args[] = {42, -7};
    for (int i = 0; i < 2; i++)
        for (int k = BB_EVENT_START; k <= BB_EVENT_FINISH; k++) {
            char text[32];
            bb_event e = {(bb_event_kind)k, k >= BB_EVENT_THOUSAND_SCORE, k >= BB_EVENT_THOUSAND_SCORE ? args[i] : 0};
            bb_event_format(&e, text);
            outf("ev %s\n", text);
        }
}

static int cmp_isize(const void *a, const void *b)
{
    intptr_t x = *(const intptr_t *)a, y = *(const intptr_t *)b;
    return (x > y) - (x < y);
}

static void level(uint64_t seed, bb_skill skill, size_t n)
{
    bb_packed_row *storage = malloc(BB_SLIDING_LENGTH_MAX * sizeof *storage);
    bb_level lvl;
    bb_level_init(&lvl, seed, skill, storage, BB_SLIDING_LENGTH_MAX);
    size_t w = bb_level_width(&lvl);
    outf("level seed=%" PRIu64 " skill=%s width=%zu\n", seed, bb_skill_as_str(skill), w);
    while ((size_t)bb_level_last(&lvl) < n) {
        intptr_t last0 = bb_level_last(&lvl);
        bb_level_generate(&lvl, 100);
        for (intptr_t abs = last0; abs < bb_level_last(&lvl); abs++) {
            intptr_t rel = abs - bb_level_first(&lvl);
            char text[BB_WIDTH_MAX + 1];
            for (size_t c = 0; c < w; c++)
                text[c] = bb_slab_kind_char(bb_level_slab_kind(&lvl, (intptr_t)c, rel, false));
            text[w] = '\0';
            outf("r %td %s\n", abs, text);
        }
    }
    intptr_t first = bb_level_first(&lvl), last = bb_level_last(&lvl);
    outf("first=%td last=%td len=%zu\n", first, last, bb_level_len(&lvl));
    intptr_t rows[8 + 18] = {-3, -1, 0, 1, 2, 3, 50, last / 2};
    int nr = 8;
    for (intptr_t d = -3; d < 3; d++) {
        rows[nr++] = last + d;
        rows[nr++] = first + d;
        rows[nr++] = last - first + d;
    }
    qsort(rows, nr, sizeof rows[0], cmp_isize);
    for (int i = 0; i < nr; i++) {
        if (i > 0 && rows[i] == rows[i - 1])
            continue;
        intptr_t r = rows[i];
        outf("q %td", r);
        for (intptr_t c = -2; c < BB_WIDTH_MAX + 2; c++)
            outf(" %" PRId64 "%" PRId64 "%" PRId64, bb_level_item(&lvl, c, r, false), bb_level_item(&lvl, c, r, true),
                   bb_slab_kind_as_item(bb_level_slab_kind(&lvl, c, r, true)));
        for (intptr_t c = -2; c < (intptr_t)w + 2; c++) {
            intptr_t c2, r2;
            if (bb_level_find_start_spot(&lvl, c, r, &c2, &r2))
                outf(" (%td,%td)", c2, r2);
            else
                outf(" none");
        }
        outf("\n");
    }
    /* The rows the deque holds, read directly (Level's Display); see the Rust driver. */
    outf("deque first=%td\n", first);
    for (size_t i = 0; i < bb_level_len(&lvl); i++) {
        char text[BB_WIDTH_MAX + 1];
        bb_row_format(bb_level_row_at(&lvl, i), text);
        text[w] = '\0';
        outf("%s%s", i ? "\n" : "", text);
    }
    outf("\n");
    free(storage);
}

/* Window sizes: BB_WINDOW (level rows) and BB_LOG_WINDOW (scores log slots) from the
 * environment; unset, they hold everything (babalcore's 10,000 rows; more log slots than any
 * job reaches). With either set the run is in window mode: the final log and rows dumps, which
 * show storage rather than behaviour, are left out (oracle/run.sh compares without them), and
 * the deepest reads go to stderr. */
#define FULL_LOG_WINDOW 32768
static size_t level_window = BB_SLIDING_LENGTH_MAX, log_window = FULL_LOG_WINDOW;
static bool window_mode;
static double log_storage[FULL_LOG_WINDOW];
static uint32_t log_present[BB_LOG_PRESENT_WORDS(FULL_LOG_WINDOW)];

static void read_windows(void)
{
    const char *w = getenv("BB_WINDOW"), *l = getenv("BB_LOG_WINDOW");
    if (w && *w) {
        level_window = strtoul(w, NULL, 10);
        window_mode = true;
    }
    if (l && *l) {
        log_window = strtoul(l, NULL, 10);
        window_mode = true;
    }
    if (level_window < 1 || level_window > BB_SLIDING_LENGTH_MAX || log_window < 1 || log_window > FULL_LOG_WINDOW) {
        fprintf(stderr, "window out of range\n");
        exit(2);
    }
}

/* The C-only counters must stay 0, or the run says so and fails. */
static void check_counters(const bb_points_counter *pc, const bb_input_steer *is)
{
    uint32_t log_out = pc ? pc->log_out_of_window : 0, dropped = is ? is->dropped_keys : 0;
    if (window_mode)
        fprintf(stderr, "depth level=0 log=%zu\n", pc ? pc->log_max_depth : 0);
    if (log_out || dropped) {
        fprintf(stderr, "C-only limit reached: level out_of_window=0 log out_of_window=%u dropped_keys=%u\n",
                log_out, dropped);
        exit(3);
    }
}

static void points(uint64_t seed)
{
    char b1[17], b2[17], b3[17];
    sm_state = seed;
    bb_points_counter pc;
    bb_points_counter_init(&pc, log_storage, log_present, log_window);
    if (sm_next() % 2 == 0)
        bb_points_counter_set_best_score(&pc, (int32_t)(sm_next() % 5000));
    static const double ttcs[] = {30.0, 49.9, 50.0, 50.1, 120.0, 199.9, 200.0, 200.1, 300.0, 0.0, -1.0};
    double ttc = ttcs[seed % 11]; /* by seed, so every band edge is used */
    uint64_t steps = 4000 + (seed % 3) * 6000;
    outf("points seed=%" PRIu64 " ttc=%s steps=%" PRIu64 "\n", seed, b64(ttc, b1), steps);
    double dist = 0.0;
    for (uint64_t i = 0; i < steps; i++) {
        double r = sm_unit();
        double delta = r < 0.02 ? 0.0 : r < 0.03 ? -0.01 : sm_unit() * 0.05;
        double m = sm_unit();
        if (m < 0.85) {
            dist += sm_unit() * 0.4;
        } else if (m < 0.95) {
            dist -= sm_unit() * 5.0;
            if (dist < 0.0)
                dist = 0.0;
        } else {
            dist += sm_unit() * 40.0;
        }
        bb_events ev = {.n = 0};
        bb_points_counter_handle_dist(&pc, delta, dist, ttc, &ev);
        outf("p %" PRIu64 " ev=", i);
        print_events(&ev);
        outf(" score=%" PRId32 " best_row=%td best_dist=%s pct=%s st=", bb_points_counter_score(&pc),
               bb_points_counter_best_row(&pc), b64(bb_points_counter_best_dist(&pc), b2),
               b64(bb_points_counter_time_pct(&pc, ttc), b3));
        print_pc_tokens(&pc, false);
        outf("\n");
    }
    if (!window_mode)
        print_log(&pc);
    check_counters(&pc, NULL);
}

static void inputs(uint64_t seed)
{
    char b[17];
    sm_state = seed;
    bb_input_jump ij;
    bb_input_steer st;
    bb_input_jump_init(&ij);
    bb_input_steer_init(&st);
    int64_t t = 1000000000000;
    static const double ints[] = {1.0, 0.5, 0.25, 1.5, 0.0, -0.3};
    static const double sticks[] = {-1.0, 0.0, 0.05, 0.16, 1.0, 1e10};
    static const double mice[] = {3.0, -7.5, 0.125, -40.0, NAN};
    outf("inputs seed=%" PRIu64 "\n", seed);
    for (int i = 0; i < 3000; i++) {
        uint64_t r = sm_next() % 100;
        int64_t dt = (int64_t)(sm_next() % 50000000);
        bool back = sm_next() % 10 == 0;
        if (back)
            t -= dt;
        else if (sm_next() % 5 != 0)
            t += dt;
        if (r < 15) {
            bb_input_jump_push_jump(&ij, t);
        } else if (r < 30) {
            outf("j %d %d\n", i, bb_input_jump_pop_jump(&ij, t) ? 1 : 0);
        } else if (r < 60) {
            int64_t id = 1 + (int64_t)(sm_next() % 2);
            double intensity = ints[sm_next() % 6];
            bool state = sm_next() % 3 != 0;
            if (r < 45)
                bb_input_steer_left_key(&st, id, intensity, state, t);
            else
                bb_input_steer_right_key(&st, id, intensity, state, t);
        } else if (r < 65) {
            bb_input_steer_mouse_move(&st, mice[sm_next() % 5]);
        } else if (r < 70) {
            double s = sticks[sm_next() % 6];
            bb_input_jump_set_stickiness_secs_f64(&ij, s >= 0.0, s);
        } else {
            outf("s %d %s\n", i, b64(bb_input_steer_pop_steer(&st, t), b));
        }
    }
    check_counters(NULL, &st);
}

static void game(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
        perror(path);
        exit(1);
    }
    fclose(f);
    bb_packed_row *storage = malloc(level_window * sizeof *storage);
    game_result res = {0};
    int rc = game_run(text, (size_t)size, storage, level_window, log_storage, log_present, log_window, window_mode, &res);
    if (window_mode)
        fprintf(stderr, "depth level=%zu log=%zu\n", res.depth_level, res.depth_log);
    if (rc == 3)
        fprintf(stderr, "C-only limit reached: level out_of_window=%u log out_of_window=%u dropped_keys=%u\n",
                res.level_out, res.log_out, res.dropped);
    free(storage);
    free(text);
    if (rc)
        exit(rc);
}

int main(int argc, char **argv)
{
    read_windows();
    if (argc >= 2 && !strcmp(argv[1], "pins"))
        pins();
    else if (argc >= 5 && !strcmp(argv[1], "level"))
        level(strtoull(argv[2], NULL, 10), bb_skill_from_str(argv[3]), strtoull(argv[4], NULL, 10));
    else if (argc >= 3 && !strcmp(argv[1], "game"))
        game(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "points"))
        points(strtoull(argv[2], NULL, 10));
    else if (argc >= 3 && !strcmp(argv[1], "inputs"))
        inputs(strtoull(argv[2], NULL, 10));
    else {
        fprintf(stderr, "usage: driver pins | level SEED SKILL N | game SCRIPT | points SEED | inputs SEED\n");
        return 2;
    }
    return 0;
}
