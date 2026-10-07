/* SPDX-License-Identifier: MIT */
/* Babal by Christian Mauduit, gitlab.com/liberecofr/babal (public domain, the Unlicense).
 * Its rules in C: a one-to-one port of rust/babalcore at fa985cd49111270edade2c7bebdab7ce204be6a8.
 * Each section names the babalcore file it comes from. Differences that are not a
 * translation are listed in docs/stage1-rules.md. Build with -ffp-contract=off. */
#ifndef BABAL_H
#define BABAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* consts.rs */
#define BB_WIDTH_MAX 32
#define BB_ABSOLUTE_LENGTH_MAX 100000000
#define BB_SLIDING_LENGTH_MAX 10000

/* time: Instant and Duration are int64 nanoseconds; Option<Instant> is a flag beside one. */
typedef int64_t bb_instant;
typedef int64_t bb_duration;
uint64_t bb_duration_from_secs_f64(double secs);
double bb_duration_as_secs_f64(uint64_t nanos);

/* chacha20.c: rand 0.10's ChaCha20Rng as row_generator uses it (seed, f64 draws). */
typedef struct {
    uint32_t key[8];
    uint64_t counter;
    uint32_t buf[16];
    int index;
} bb_chacha20;
void bb_chacha20_from_seed(bb_chacha20 *rng, const uint8_t seed[32]);
uint64_t bb_chacha20_next_u64(bb_chacha20 *rng);
double bb_chacha20_random_f64(bb_chacha20 *rng);

/* euclid's Vector3D<f64>, what player.rs uses of it. */
typedef struct { double x, y, z; } bb_vec3;
typedef struct { float x, y, z; } bb_vec3f;
bb_vec3 bb_vec3_lerp(bb_vec3 a, bb_vec3 b, double t);

/* maths.rs */
double bb_fade_in(double x);
double bb_fade_out(double x);
double bb_square_wave(double x);
double bb_triangle_wave(double x);
double bb_sawtooth_wave(double x);
double bb_sine_wave(double x);
double bb_pulse(double x, double alpha);
double bb_boxcar(double x);
double bb_heaviside_step(double x);

/* slab_def.rs (declaration order is the Ord order) */
typedef enum { BB_DEF_VOID, BB_DEF_FLOOR, BB_DEF_BOOST } bb_slab_def;
bool bb_slab_def_is_special(bb_slab_def def);
char bb_slab_def_char(bb_slab_def def);

/* slab_kind.rs */
typedef enum {
    BB_KIND_VOID = -1,
    BB_KIND_FLOOR,
    BB_KIND_BOOST,
    BB_KIND_OVERDRIVE,
    BB_KIND_HIGHLIGHT,
} bb_slab_kind;
#define BB_MAX_ITEM 4
int64_t bb_slab_kind_as_item(bb_slab_kind kind);
bb_slab_kind bb_slab_kind_from_item(int64_t item);
char bb_slab_kind_char(bb_slab_kind kind);

/* slab.rs */
typedef struct { uint8_t def; } bb_slab;
bb_slab bb_slab_new(bb_slab_def def);
bb_slab_kind bb_slab_kind_of(const bb_slab *slab, int64_t now_msec);
int64_t bb_slab_get_item(const bb_slab *slab, int64_t now_msec);
void bb_slab_set(bb_slab *slab, bb_slab_def def);
bb_slab_def bb_slab_get(const bb_slab *slab);

/* row.rs */
typedef struct { bb_slab slabs[BB_WIDTH_MAX]; } bb_row;
void bb_row_new(bb_row *row, bb_slab_def def);
bb_slab_kind bb_row_slab_kind(const bb_row *row, intptr_t col, int64_t now_msec);
void bb_row_set(bb_row *row, intptr_t col, bb_slab_def def);
bb_slab_def bb_row_get(const bb_row *row, intptr_t col);
void bb_row_format(const bb_row *row, char out[BB_WIDTH_MAX + 1]);

/* skill.rs */
typedef enum { BB_NOOB, BB_EASY, BB_MEDIUM, BB_HARD, BB_EXPERT } bb_skill;
#define BB_SKILL_DEFAULT BB_MEDIUM
typedef struct { bb_slab_def def; double density; } bb_def_density;
typedef struct { bb_def_density v[3]; int n; } bb_def_densities; /* sorted by def */
bb_skill bb_skill_from_str(const char *name);
const char *bb_skill_as_str(bb_skill skill);
size_t bb_skill_width(bb_skill skill);
bb_def_densities bb_skill_def_density(bb_skill skill);
bb_def_densities bb_skill_def_density0(bb_skill skill);
double bb_skill_mutability(bb_skill skill);
double bb_skill_time_to_complete(bb_skill skill);
double bb_skill_begin_speed_factor(bb_skill skill);
double bb_skill_end_speed_factor(bb_skill skill);

/* row_generator.rs */
void bb_generate_first_row(bb_row *out, uint64_t seed, size_t width, bb_skill skill);
void bb_generate_next_row(bb_row *out, uint64_t seed, size_t j, const bb_row *previous_row, bb_skill skill);

/* level.rs, level_query.rs. The level is babalcore's: up to BB_SLIDING_LENGTH_MAX rows kept,
 * row0 counting those slid off the front. Rows are held packed, all 32 slab defs at 2 bits each
 * (8 bytes a row), the newest `window` of them, row r at rows[r % window] in storage the caller
 * owns; window = BB_SLIDING_LENGTH_MAX holds the whole level, which a game needs (a struggling
 * player drifts back without limit: docs/stage2.md). A read of an older row returns a void row and
 * adds to out_of_window, which must stay 0. max_read_depth is how far behind the newest row any
 * read went. bb_level_row_at unpacks into the level's scratch row: valid until the next call. */
typedef uint64_t bb_packed_row; /* slab j's def in bits 2j and 2j+1 */
typedef struct {
    uint64_t seed;
    bb_packed_row *rows;
    size_t window;
    size_t len;
    intptr_t row0;
    size_t w;
    bb_skill skl;
    int64_t now_msec;
    bb_row scratch;          /* C only: bb_level_row_at's */
    uint32_t out_of_window;  /* C only */
    size_t max_read_depth;   /* C only */
} bb_level;
void bb_level_init(bb_level *lvl, uint64_t seed, bb_skill skill, bb_packed_row *storage, size_t window);
void bb_level_push(bb_level *lvl, const bb_row *row);
void bb_level_generate(bb_level *lvl, size_t n);
bb_slab_kind bb_level_slab_kind(const bb_level *lvl, intptr_t col, intptr_t row, bool boost);
void bb_level_set_now_msec(bb_level *lvl, int64_t now_msec);
void bb_level_set_now_sec(bb_level *lvl, double now_sec);
const bb_row *bb_level_row_at(const bb_level *lvl, size_t index); /* index from the front */
size_t bb_level_width(const bb_level *lvl);
size_t bb_level_len(const bb_level *lvl);
intptr_t bb_level_first(const bb_level *lvl);
intptr_t bb_level_last(const bb_level *lvl);
int64_t bb_level_item(const bb_level *lvl, intptr_t col, intptr_t row, bool boost);
bool bb_level_find_start_spot(const bb_level *lvl, intptr_t col, intptr_t row, intptr_t *out_col, intptr_t *out_row);
bb_skill bb_level_skill(const bb_level *lvl);

/* time_tracker.rs */
#define BB_TIME_TRACKER_MAX_SEC 1e12
typedef struct { double now_sec_f64; } bb_time_tracker;
void bb_time_tracker_add(bb_time_tracker *tt, double delta);
double bb_time_tracker_now_sec(const bb_time_tracker *tt);
int64_t bb_time_tracker_now_msec(const bb_time_tracker *tt);
void bb_time_tracker_format(const bb_time_tracker *tt, char out[32]); /* Display: "2.050" */

/* player_event_kind.rs, player_event.rs */
typedef enum {
    BB_EVENT_START,
    BB_EVENT_JUMP,
    BB_EVENT_LAND,
    BB_EVENT_FALL,
    BB_EVENT_CATCH_UP,
    BB_EVENT_CHANGE_COLUMN,
    BB_EVENT_BOOST,
    BB_EVENT_OVERDRIVE,
    BB_EVENT_THOUSAND_SCORE,
    BB_EVENT_PERCENT_TIME,
    BB_EVENT_HIGH_SCORE,
    BB_EVENT_FINISH,
} bb_event_kind;
const char *bb_event_kind_as_str(bb_event_kind kind);
typedef struct { bb_event_kind kind; bool has_arg; int32_t arg; } bb_event;
#define BB_EVENTS_MAX 16
typedef struct { bb_event v[BB_EVENTS_MAX]; int n; } bb_events;
void bb_events_push(bb_events *events, bb_event_kind kind, bool has_arg, int32_t arg);
void bb_event_format(const bb_event *e, char out[32]); /* Display: "jump", "finish: 42" */

/* points_counter.rs. The scores log (HashMap<isize, f64>) holds the newest `log_window` rows in
 * the caller's storage: row r's score in log[r % log_window], and a presence bit in log_present.
 * Rows are logged in rising order (the key is the best distance so far, rounded); logging row k
 * clears the bits of the rows skipped on the way to it, so a set bit is always its own row's. A
 * lookup of a row more than log_window behind the newest adds to log_out_of_window, which must
 * stay 0; a window above the deepest row logged holds babalcore's whole log. */
#define BB_LOG_PRESENT_WORDS(window) (((window) + 31) / 32)
typedef struct {
    double total_points;
    double last_dist;
    intptr_t last_row;
    double last_best_dist;
    intptr_t last_best_row;
    bb_time_tracker time_played_tracker;
    double delta_total;
    double *log;
    uint32_t *log_present;
    size_t log_window;
    intptr_t log_max_key;      /* the newest row logged, -1 for none */
    bool behind;
    int32_t best_current_score;
    int32_t best_previous_score;
    bool high_score;
    int32_t best_time_pct;
    uint32_t log_out_of_window; /* C only: must stay 0 */
    size_t log_max_depth;       /* C only: the deepest lookup behind the newest row logged */
} bb_points_counter;
void bb_points_counter_init(bb_points_counter *pc, double *log, uint32_t *log_present, size_t log_window);
void bb_points_counter_handle_dist(bb_points_counter *pc, double delta, double dist, double time_to_complete, bb_events *out);
int32_t bb_points_counter_score(const bb_points_counter *pc);
void bb_points_counter_set_best_score(bb_points_counter *pc, int32_t best_score);
intptr_t bb_points_counter_best_row(const bb_points_counter *pc);
double bb_points_counter_best_dist(const bb_points_counter *pc);
double bb_points_counter_time_pct(const bb_points_counter *pc, double time_to_complete);

/* input_query.rs */
typedef struct {
    bool (*pop_jump)(void *ctx);
    double (*pop_steer)(void *ctx);
    void *ctx;
} bb_input_query;

/* fake_input.rs */
#define BB_FAKE_STEERS_MAX 256
typedef struct {
    intptr_t jumps;
    double steers[BB_FAKE_STEERS_MAX];
    int head, count;
    uint32_t dropped; /* C only: steers pushed past BB_FAKE_STEERS_MAX queued (must stay 0) */
} bb_fake_input;
void bb_fake_input_init(bb_fake_input *fi);
void bb_fake_input_push_jump(bb_fake_input *fi);
void bb_fake_input_push_steer(bb_fake_input *fi, double steer);
bool bb_fake_input_pop_jump(void *fi);
double bb_fake_input_pop_steer(void *fi);
bb_input_query bb_fake_input_query(bb_fake_input *fi);

/* input_jump.rs */
#define BB_DEFAULT_STICKINESS_SECS_F64 0.16
typedef struct {
    bool has_stickiness;
    bb_duration stickiness;
    bool first_pop;
    bool has_last_request;
    bb_instant last_request;
} bb_input_jump;
void bb_input_jump_init(bb_input_jump *ij);
void bb_input_jump_set_stickiness_secs_f64(bb_input_jump *ij, bool has, double secs);
void bb_input_jump_push_jump(bb_input_jump *ij, bb_instant now);
bool bb_input_jump_pop_jump(bb_input_jump *ij, bb_instant now);

/* input_steer.rs. The key maps are small arrays (see docs/stage1-rules.md, quirk 3). */
#define BB_DEFAULT_MOUSE_SENSIBILITY 1.0
#define BB_DEFAULT_KEYBOARD_SENSIBILITY 1000.0
#define BB_INTENSITY_MIN 0.0
#define BB_INTENSITY_MAX 1.0
#define BB_BASE_KEYBOARD_ACC_VALUE 0.01
#define BB_STEER_KEYS_MAX 8 /* ids ever used on one side; more are counted in dropped_keys */
typedef struct {
    int64_t id;
    double acc;
    double intensity;
    bool has_last_press;
    bb_instant last_press;
} bb_key_state;
typedef struct { bb_key_state v[BB_STEER_KEYS_MAX]; int n; } bb_key_map;
typedef struct {
    double mouse_sensibility;
    double keyboard_sensibility;
    double mouse_acc;
    bb_key_map right_keys;
    bb_key_map left_keys;
    uint32_t dropped_keys; /* C only: presses of ids past BB_STEER_KEYS_MAX (must stay 0) */
} bb_input_steer;
void bb_input_steer_init(bb_input_steer *is);
void bb_input_steer_mouse_move(bb_input_steer *is, double delta_px);
void bb_input_steer_right_key(bb_input_steer *is, int64_t id, double intensity, bool state, bb_instant now);
void bb_input_steer_left_key(bb_input_steer *is, int64_t id, double intensity, bool state, bb_instant now);
double bb_input_steer_pop_steer(bb_input_steer *is, bb_instant now);

/* player.rs */
typedef struct { bb_vec3 position_vec; double time_sec; } bb_point_in_time;
typedef struct { bb_point_in_time begin, top_fall, bottom_fall, end; } bb_backing_data;
typedef struct {
    bool initialized;
    double steer_factor;
    bb_vec3 position_vec;
    bb_vec3 speed_vec;
    bb_time_tracker time_tracker;
    bool is_jumping;
    bool boost_state;
    intptr_t cur_level_col;
    bool has_backing_data;
    bb_backing_data backing_data;
    bb_points_counter points_counter;
    intptr_t boost_count;
    intptr_t overdrive_count;
} bb_player;
void bb_player_init(bb_player *p, double *log, uint32_t *log_present, size_t log_window);
void bb_player_tick(bb_player *p, double delta, const bb_level *level, bb_input_query *input, bb_events *out);
bb_vec3f bb_player_position(const bb_player *p);
bb_vec3f bb_player_speed(const bb_player *p);
intptr_t bb_player_level_col(const bb_player *p, size_t width);
intptr_t bb_player_level_row(const bb_player *p);
double bb_player_dist(const bb_player *p);
bb_vec3 bb_player_from_level_coord(intptr_t col, intptr_t row, size_t width);
void bb_player_set_steer_factor(bb_player *p, double sensibility);
double bb_player_backing_camera_blend(const bb_player *p);
double bb_player_speed_factor(const bb_player *p, const bb_level *level);
int32_t bb_player_score(const bb_player *p);
void bb_player_set_best_score(bb_player *p, int32_t best_score);
intptr_t bb_player_best_row(const bb_player *p);
double bb_player_best_dist(const bb_player *p);
bool bb_player_is_boosting(const bb_player *p);
double bb_player_time_progress_done(const bb_player *p, const bb_level *level);
double bb_player_time_progress_total(const bb_player *p);

#endif
