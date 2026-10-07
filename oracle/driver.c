/* The C side of the oracle: prints what oracle/ref (babalcore) prints, from babal/.
 * Usage: driver pins | level SEED SKILL N | game SCRIPT */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../babal/babal.h"
#include "../babal/internal.h"

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

static const char *b64(double x, char buf[17])
{
    uint64_t u;
    memcpy(&u, &x, 8);
    snprintf(buf, 17, "%016" PRIx64, u);
    return buf;
}
static const char *b32(float x, char buf[9])
{
    uint32_t u;
    memcpy(&u, &x, 4);
    snprintf(buf, 9, "%08" PRIx32, u);
    return buf;
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
        printf("chacha %" PRIu64 " %" PRIu64, seeds[i][0], seeds[i][1]);
        for (int k = 0; k < 70; k++)
            printf(" %s", b64(bb_chacha20_random_f64(&rng), a1));
        printf("\n");
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
        printf("lerp %s %s %s %s %s %s %s -> %s %s %s\n", b64(a.x, a1), b64(a.y, a2), b64(a.z, a3), b64(b.x, a4),
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
            printf("from_secs_f64 %s %" PRIu64 "\n", b64(secs[i], a1), bb_duration_from_secs_f64(secs[i]));
    static const uint64_t fixed_ns[] = {0, 1, 999999999, 1000000000, 160000000};
    for (int k = 0; k < 300; k++) {
        uint64_t ns = k < 5 ? fixed_ns[k] : sm_next() % 100000000000000ULL;
        printf("as_secs_f64 %" PRIu64 " %s\n", ns, b64(bb_duration_as_secs_f64(ns), a1));
    }
    double xs[8 + 300 + 50] = {0.5, -0.5, 1.5, 2.5, -2.5, 0.49999999999999994, 4503599627370495.5, -0.0};
    n = 8;
    for (int i = 0; i < 300; i++)
        xs[n++] = (sm_unit() - 0.5) * 2e4;
    for (int i = 0; i < 50; i++)
        xs[n++] = (double)(sm_next() % 20000) * 0.5 - 5000.0;
    for (int i = 0; i < n; i++) {
        double x = xs[i];
        printf("num %s round=%s fract=%s trunc_i=%" PRId64 " floor=%s f32=%s\n", b64(x, a1), b64(round(x), a2),
               b64(x - trunc(x), a3), bb_f64_as_i64(x), b64(floor(x), a4), b32((float)x, f1));
    }
}

static int cmp_isize(const void *a, const void *b)
{
    intptr_t x = *(const intptr_t *)a, y = *(const intptr_t *)b;
    return (x > y) - (x < y);
}

static void level(uint64_t seed, bb_skill skill, size_t n)
{
    bb_row *storage = malloc(BB_SLIDING_LENGTH_MAX * sizeof *storage);
    bb_level lvl;
    bb_level_init(&lvl, seed, skill, storage, BB_SLIDING_LENGTH_MAX);
    size_t w = bb_level_width(&lvl);
    printf("level seed=%" PRIu64 " skill=%s width=%zu\n", seed, bb_skill_as_str(skill), w);
    while ((size_t)bb_level_last(&lvl) < n) {
        intptr_t last0 = bb_level_last(&lvl);
        bb_level_generate(&lvl, 100);
        for (intptr_t abs = last0; abs < bb_level_last(&lvl); abs++) {
            intptr_t rel = abs - bb_level_first(&lvl);
            char text[BB_WIDTH_MAX + 1];
            for (size_t c = 0; c < w; c++)
                text[c] = bb_slab_kind_char(bb_level_slab_kind(&lvl, (intptr_t)c, rel, false));
            text[w] = '\0';
            printf("r %td %s\n", abs, text);
        }
    }
    intptr_t first = bb_level_first(&lvl), last = bb_level_last(&lvl);
    printf("first=%td last=%td len=%zu\n", first, last, bb_level_len(&lvl));
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
        printf("q %td", r);
        for (intptr_t c = -2; c < BB_WIDTH_MAX + 2; c++)
            printf(" %" PRId64 "%" PRId64 "%" PRId64, bb_level_item(&lvl, c, r, false), bb_level_item(&lvl, c, r, true),
                   bb_slab_kind_as_item(bb_level_slab_kind(&lvl, c, r, true)));
        for (intptr_t c = -2; c < (intptr_t)w + 2; c++) {
            intptr_t c2, r2;
            if (bb_level_find_start_spot(&lvl, c, r, &c2, &r2))
                printf(" (%td,%td)", c2, r2);
            else
                printf(" none");
        }
        printf("\n");
    }
    /* The rows the deque holds, read directly (Level's Display); see the Rust driver. */
    printf("deque first=%td\n", first);
    for (size_t i = 0; i < bb_level_len(&lvl); i++) {
        char text[BB_WIDTH_MAX + 1];
        bb_row_format(bb_level_row_at(&lvl, i), text);
        text[w] = '\0';
        printf("%s%s", i ? "\n" : "", text);
    }
    printf("\n");
    free(storage);
}

/* The player's every field in declaration order, as oracle/ref reads it from Debug. */
static void tok_f(double x)
{
    char b[17];
    printf(",%s", b64(x, b));
}
static void tok_v(bb_vec3 v)
{
    tok_f(v.x);
    tok_f(v.y);
    tok_f(v.z);
}
static void print_state(const bb_player *p)
{
    const bb_points_counter *pc = &p->points_counter;
    char b[17];
    printf("s %d,%s", p->initialized ? 1 : 0, b64(p->steer_factor, b));
    tok_v(p->position_vec);
    tok_v(p->speed_vec);
    tok_f(p->time_tracker.now_sec_f64);
    printf(",%d,%d,%td", p->is_jumping ? 1 : 0, p->boost_state ? 1 : 0, p->cur_level_col);
    if (p->has_backing_data) {
        const bb_point_in_time *pts[4] = {&p->backing_data.begin, &p->backing_data.top_fall,
                                          &p->backing_data.bottom_fall, &p->backing_data.end};
        printf(",S");
        for (int i = 0; i < 4; i++) {
            tok_v(pts[i]->position_vec);
            tok_f(pts[i]->time_sec);
        }
    } else {
        printf(",N");
    }
    tok_f(pc->total_points);
    tok_f(pc->last_dist);
    printf(",%td", pc->last_row);
    tok_f(pc->last_best_dist);
    printf(",%td", pc->last_best_row);
    tok_f(pc->time_played_tracker.now_sec_f64);
    tok_f(pc->delta_total);
    printf(",%d,%" PRId32 ",%" PRId32 ",%d,%" PRId32 ",%td,%td\n", pc->behind ? 1 : 0, pc->best_current_score,
           pc->best_previous_score, pc->high_score ? 1 : 0, pc->best_time_pct, p->boost_count, p->overdrive_count);
}
static void print_log(const bb_player *p)
{
    char b[17];
    int first = 1;
    printf("log ");
    for (size_t k = 0; k < p->points_counter.scores_cap; k++)
        if (p->points_counter.scores_log[k].present) {
            printf("%s%zu:%s", first ? "" : " ", k, b64(p->points_counter.scores_log[k].points, b));
            first = 0;
        }
    printf("\n");
}

typedef struct {
    bb_input_steer *steer;
    bb_input_jump *jump;
    bb_instant now;
} adapter;
static bool adapter_pop_jump(void *ctx)
{
    adapter *a = ctx;
    return bb_input_jump_pop_jump(a->jump, a->now);
}
static double adapter_pop_steer(void *ctx)
{
    adapter *a = ctx;
    return bb_input_steer_pop_steer(a->steer, a->now);
}

static void game(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(1);
    }
    char line[512], b[8][17], fb[6][9];
    int64_t clock = 0;
    uint64_t seed = 0;
    bb_skill skill = BB_SKILL_DEFAULT;
    bb_row *storage = NULL;
    bb_level lvl;
    bb_player player;
    bb_input_steer steer;
    bb_input_jump jump;
    bb_player_init(&player);
    bb_input_steer_init(&steer);
    bb_input_jump_init(&jump);
    size_t ticks = 0;
    while (fgets(line, sizeof line, f)) {
        char *w[4] = {0};
        int nw = 0;
        for (char *tok = strtok(line, " \t\r\n"); tok && nw < 4; tok = strtok(NULL, " \t\r\n"))
            w[nw++] = tok;
        if (nw == 0 || w[0][0] == '#')
            continue;
        bb_instant now = clock;
        if (!strcmp(w[0], "seed"))
            seed = strtoull(w[1], NULL, 10);
        else if (!strcmp(w[0], "skill"))
            skill = bb_skill_from_str(w[1]);
        else if (!strcmp(w[0], "steer_factor"))
            bb_player_set_steer_factor(&player, strtod(w[1], NULL));
        else if (!strcmp(w[0], "best"))
            bb_player_set_best_score(&player, (int32_t)strtol(w[1], NULL, 10));
        else if (!strcmp(w[0], "stick")) {
            if (!strcmp(w[1], "none"))
                bb_input_jump_set_stickiness_secs_f64(&jump, false, 0.0);
            else
                bb_input_jump_set_stickiness_secs_f64(&jump, true, strtod(w[1], NULL));
        } else if (!strcmp(w[0], "sens")) {
            steer.mouse_sensibility = strtod(w[1], NULL);
            steer.keyboard_sensibility = strtod(w[2], NULL);
        } else if (!strcmp(w[0], "wait"))
            clock += (int64_t)strtoull(w[1], NULL, 10);
        else if (!strcmp(w[0], "L") || !strcmp(w[0], "R")) {
            bool state = !strcmp(w[1], "1");
            double intensity = nw > 2 ? strtod(w[2], NULL) : 1.0;
            if (w[0][0] == 'L')
                bb_input_steer_left_key(&steer, 1, intensity, state, now);
            else
                bb_input_steer_right_key(&steer, 2, intensity, state, now);
        } else if (!strcmp(w[0], "J"))
            bb_input_jump_push_jump(&jump, now);
        else if (!strcmp(w[0], "M"))
            bb_input_steer_mouse_move(&steer, strtod(w[1], NULL));
        else if (!strcmp(w[0], "tick") || !strcmp(w[0], "tickd")) {
            if (!storage) {
                storage = malloc(BB_SLIDING_LENGTH_MAX * sizeof *storage);
                bb_level_init(&lvl, seed, skill, storage, BB_SLIDING_LENGTH_MAX);
            }
            double delta;
            if (!strcmp(w[0], "tick")) {
                uint64_t ns = strtoull(w[1], NULL, 10);
                clock += (int64_t)ns;
                delta = (double)ns / 1e9;
            } else {
                delta = strtod(w[1], NULL);
            }
            /* babalgame's generate_missing: keep 100 rows ahead of the player. */
            while (bb_level_last(&lvl) < bb_player_level_row(&player) + 100)
                bb_level_generate(&lvl, 100);
            adapter a = {&steer, &jump, clock};
            bb_input_query q = {adapter_pop_jump, adapter_pop_steer, &a};
            bb_events ev;
            bb_player_tick(&player, delta, &lvl, &q, &ev);
            printf("t %zu ev=", ticks);
            for (int i = 0; i < ev.n; i++) {
                if (i)
                    printf(",");
                if (ev.v[i].has_arg)
                    printf("%s:%" PRId32, bb_event_kind_as_str(ev.v[i].kind), ev.v[i].arg);
                else
                    printf("%s", bb_event_kind_as_str(ev.v[i].kind));
            }
            bb_vec3f p = bb_player_position(&player), s = bb_player_speed(&player);
            printf(" p=%s,%s,%s s=%s,%s,%s col=%td row=%td dist=%s score=%" PRId32 " best_row=%td best_dist=%s boost=%d"
                   " blend=%s sf=%s tpd=%s tpt=%s first=%td last=%td len=%zu\n",
                   b32(p.x, fb[0]), b32(p.y, fb[1]), b32(p.z, fb[2]), b32(s.x, fb[3]), b32(s.y, fb[4]), b32(s.z, fb[5]),
                   bb_player_level_col(&player, bb_level_width(&lvl)), bb_player_level_row(&player),
                   b64(bb_player_dist(&player), b[0]), bb_player_score(&player), bb_player_best_row(&player),
                   b64(bb_player_best_dist(&player), b[1]), bb_player_is_boosting(&player) ? 1 : 0,
                   b64(bb_player_backing_camera_blend(&player), b[2]), b64(bb_player_speed_factor(&player, &lvl), b[3]),
                   b64(bb_player_time_progress_done(&player, &lvl), b[4]), b64(bb_player_time_progress_total(&player), b[5]),
                   bb_level_first(&lvl), bb_level_last(&lvl), bb_level_len(&lvl));
            print_state(&player);
            ticks++;
        } else {
            fprintf(stderr, "unknown script word %s\n", w[0]);
            exit(1);
        }
    }
    fclose(f);
    print_log(&player);
    if (storage) {
        printf("rows first=%td last=%td\n", bb_level_first(&lvl), bb_level_last(&lvl));
        size_t w = bb_level_width(&lvl);
        for (size_t i = 0; i < bb_level_len(&lvl); i++) {
            char text[BB_WIDTH_MAX + 1];
            bb_row_format(bb_level_row_at(&lvl, i), text);
            text[w] = '\0';
            printf("%s%s", i ? "\n" : "", text);
        }
        printf("\n");
        free(storage);
    }
    bb_player_free(&player);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "pins"))
        pins();
    else if (argc >= 5 && !strcmp(argv[1], "level"))
        level(strtoull(argv[2], NULL, 10), bb_skill_from_str(argv[3]), strtoull(argv[4], NULL, 10));
    else if (argc >= 3 && !strcmp(argv[1], "game"))
        game(argv[2]);
    else {
        fprintf(stderr, "usage: driver pins | level SEED SKILL N | game SCRIPT\n");
        return 2;
    }
    return 0;
}
