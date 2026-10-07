/* from babalcore player.rs, commit fa985cd. Arithmetic is kept term for term in Rust's
 * left-to-right order; with -ffp-contract=off the doubles round as the Rust's do. */
#include <math.h>
#include <string.h>

#include "babal.h"
#include "internal.h"

#define STEER_WIDTH_SCALE 500.0
#define STEER_SLOW_DOWN (STEER_WIDTH_SCALE * 3.0 / 5.0)
#define STEER_MAX_SPEED (STEER_WIDTH_SCALE * 2.0 / 5.0)

#define JUMP_HEIGHT_SCALE 100.0
#define JUMP_VY_INIT (JUMP_HEIGHT_SCALE * 3.0)
#define JUMP_VY_BOOST_INIT (JUMP_HEIGHT_SCALE * 3.4)
#define JUMP_VY_OVERDRIVE_INIT (JUMP_HEIGHT_SCALE * 3.6)
#define JUMP_POSY_INIT (JUMP_HEIGHT_SCALE / 1000.0)
#define JUMP_GRAVITY (JUMP_HEIGHT_SCALE * 6.0)

#define FORWARD_DEPTH_SCALE 100.0
#define FORWARD_JUMP_BOOST (FORWARD_DEPTH_SCALE * 2.0)
#define FORWARD_STEER_BOOST_ON_GROUND (FORWARD_DEPTH_SCALE * 6.0)
#define FORWARD_STEER_BOOST_IN_AIR (FORWARD_DEPTH_SCALE * 2.0)
#define FORWARD_MAX_SPEED (FORWARD_DEPTH_SCALE * 15.0)
#define FORWARD_BOOST_SPEED (FORWARD_DEPTH_SCALE * 6.0)
#define FORWARD_OVERDRIVE_SPEED (FORWARD_DEPTH_SCALE * 12.0)
#define FORWARD_SLOW_DOWN_LINEAR_ON_GROUND (FORWARD_DEPTH_SCALE * 0.2)
#define FORWARD_SLOW_DOWN_SQUARE_ON_GROUND 0.5
#define FORWARD_SLOW_DOWN_LINEAR_IN_AIR (FORWARD_DEPTH_SCALE * 0.1)
#define FORWARD_SLOW_DOWN_SQUARE_IN_AIR 0.4

#define BACKING_REPOS_DELAY 0.05
#define BACKING_FALL_DELAY 0.55
#define BACKING_REWIND_DELAY 0.4
#define BACKING_SIMULATED_DELAY 5.0

#define BACKING_FALL_DEPTH 5.0

#define EXTRA_TIME_ON_BOOST 0.01
#define EXTRA_TIME_ON_OVERDRIVE 0.05

static const bb_vec3 ZERO = {0.0, 0.0, 0.0};

void bb_player_init(bb_player *p, double *log, uint32_t *log_present, size_t log_window)
{
    memset(p, 0, sizeof *p);
    p->initialized = false;
    p->steer_factor = 1.0;
    p->position_vec = ZERO;
    p->speed_vec = ZERO;
    p->time_tracker.now_sec_f64 = 0.0;
    p->is_jumping = false;
    p->boost_state = false;
    p->cur_level_col = 0;
    p->has_backing_data = false;
    bb_points_counter_init(&p->points_counter, log, log_present, log_window);
    p->boost_count = 0;
    p->overdrive_count = 0;
}

static double steer_max_speed(const bb_player *p)
{
    return p->steer_factor > 0.0 ? fabs(STEER_MAX_SPEED * p->steer_factor) : STEER_MAX_SPEED;
}

static double steer_slow_down(const bb_player *p)
{
    return p->steer_factor > 0.0 ? fabs(STEER_SLOW_DOWN * p->steer_factor) : STEER_SLOW_DOWN;
}

static bool is_backing(const bb_player *p)
{
    return p->has_backing_data;
}

static bool is_on_ground(const bb_player *p)
{
    return p->position_vec.y == 0.0;
}

static double forward_slow_down_linear(const bb_player *p)
{
    return is_on_ground(p) ? FORWARD_SLOW_DOWN_LINEAR_ON_GROUND : FORWARD_SLOW_DOWN_LINEAR_IN_AIR;
}

static double forward_slow_down_square(const bb_player *p)
{
    return is_on_ground(p) ? FORWARD_SLOW_DOWN_SQUARE_ON_GROUND : FORWARD_SLOW_DOWN_SQUARE_IN_AIR;
}

static double forward_steer_boost(const bb_player *p)
{
    return is_on_ground(p) ? FORWARD_STEER_BOOST_ON_GROUND : FORWARD_STEER_BOOST_IN_AIR;
}

intptr_t bb_player_level_col(const bb_player *p, size_t width)
{
    return bb_f64_as_isize(round(p->position_vec.x * (double)width / STEER_WIDTH_SCALE)) % (intptr_t)width;
}

double bb_player_dist(const bb_player *p)
{
    return p->position_vec.z / FORWARD_DEPTH_SCALE;
}

intptr_t bb_player_level_row(const bb_player *p)
{
    return bb_f64_as_isize(round(bb_player_dist(p)));
}

bb_vec3 bb_player_from_level_coord(intptr_t col, intptr_t row, size_t width)
{
    intptr_t real_col = col;
    while (real_col < 0)
        real_col += (intptr_t)width;
    real_col = real_col % (intptr_t)width;
    intptr_t real_row = bb_imax(row, 0);
    bb_vec3 v = {(double)real_col * STEER_WIDTH_SCALE / (double)width, 0.0, (double)real_row * FORWARD_DEPTH_SCALE};
    return v;
}

static void replace(bb_player *p, const bb_level *level)
{
    intptr_t c, r;
    if (bb_level_find_start_spot(level, bb_player_level_col(p, bb_level_width(level)), bb_player_level_row(p), &c, &r))
        p->position_vec = bb_player_from_level_coord(c, r, bb_level_width(level));
}

intptr_t bb_player_best_row(const bb_player *p)
{
    return bb_points_counter_best_row(&p->points_counter);
}

static void end_fall(bb_player *p, bb_backing_data backing_data, bb_events *out)
{
    p->position_vec = backing_data.end.position_vec;
    if (bb_player_level_row(p) >= bb_player_best_row(p))
        bb_events_push(out, BB_EVENT_CATCH_UP, false, 0);
    p->speed_vec = ZERO;
    p->has_backing_data = false;
    bb_events_push(out, BB_EVENT_START, false, 0);
}

static void update_fall(bb_player *p, bb_backing_data backing_data, bb_events *out)
{
    double now_sec = bb_time_tracker_now_sec(&p->time_tracker);

    double end_sec = backing_data.end.time_sec;
    if (now_sec >= end_sec) {
        end_fall(p, backing_data, out);
        return;
    }

    double bottom_fall_sec = backing_data.bottom_fall.time_sec;
    if (now_sec >= bottom_fall_sec) {
        double alpha = (now_sec - bottom_fall_sec) / (end_sec - bottom_fall_sec);
        p->position_vec = bb_vec3_lerp(backing_data.top_fall.position_vec, backing_data.end.position_vec, alpha);
        return;
    }

    double top_fall_sec = backing_data.top_fall.time_sec;
    if (now_sec >= top_fall_sec) {
        double alpha = (now_sec - top_fall_sec) / (bottom_fall_sec - top_fall_sec);
        p->position_vec = bb_vec3_lerp(backing_data.top_fall.position_vec, backing_data.bottom_fall.position_vec, alpha);
        return;
    }

    double begin_sec = backing_data.begin.time_sec;
    if (now_sec >= begin_sec) {
        double alpha = (now_sec - begin_sec) / (top_fall_sec - begin_sec);
        /* quirk 5: from begin to begin */
        p->position_vec = bb_vec3_lerp(backing_data.begin.position_vec, backing_data.begin.position_vec, alpha);
        return;
    }
}

static void begin_fall(bb_player *p, const bb_level *level, bb_events *out)
{
    size_t w = bb_level_width(level);
    double offset = BACKING_SIMULATED_DELAY * p->speed_vec.z / FORWARD_DEPTH_SCALE;
    intptr_t backing_col = bb_player_level_col(p, w);
    intptr_t backing_src_row = bb_player_level_row(p);
    intptr_t backing_dst_row = backing_src_row - bb_f64_as_isize(offset);
    intptr_t sc, sr;
    if (bb_level_find_start_spot(level, backing_col, backing_dst_row, &sc, &sr)) {
        double now_sec = bb_time_tracker_now_sec(&p->time_tracker);
        bb_backing_data bd;
        bd.begin.position_vec = p->position_vec;
        bd.begin.time_sec = now_sec;
        bd.top_fall.position_vec = bb_player_from_level_coord(backing_col, backing_src_row, w);
        bd.top_fall.time_sec = now_sec + BACKING_REPOS_DELAY;
        bb_vec3 src = bb_player_from_level_coord(backing_col, backing_src_row, w);
        bd.bottom_fall.position_vec.x = src.x - 0.0;
        bd.bottom_fall.position_vec.y = src.y - BACKING_FALL_DEPTH * JUMP_HEIGHT_SCALE;
        bd.bottom_fall.position_vec.z = src.z - 0.0;
        bd.bottom_fall.time_sec = now_sec + BACKING_REPOS_DELAY + BACKING_FALL_DELAY;
        bd.end.position_vec = bb_player_from_level_coord(sc, sr, w);
        bd.end.time_sec = now_sec + BACKING_REPOS_DELAY + BACKING_FALL_DELAY + BACKING_REWIND_DELAY;
        p->backing_data = bd;
        p->has_backing_data = true;
    }
    p->speed_vec = ZERO;
    bb_events_push(out, BB_EVENT_FALL, false, 0);
}

static bool check_valid_position(const bb_player *p, const bb_level *level)
{
    bb_slab_kind kind = bb_slab_kind_from_item(bb_level_item(
        level, bb_player_level_col(p, bb_level_width(level)), bb_player_level_row(p), p->boost_state));
    return !(kind == BB_KIND_VOID && p->position_vec.y <= 0.0);
}

static void handle_fall(bb_player *p, const bb_level *level, bb_events *out)
{
    if (p->has_backing_data)
        update_fall(p, p->backing_data, out);
    else if (!check_valid_position(p, level))
        begin_fall(p, level, out);
}

static void handle_steer(bb_player *p, double delta, bb_input_query *input, size_t width, bb_events *out)
{
    if (p->has_backing_data)
        return;

    double steer = input->pop_steer(input->ctx);
    p->speed_vec.x += steer;

    double max_speed = steer_max_speed(p);
    if (p->speed_vec.x > max_speed)
        p->speed_vec.x = max_speed;
    if (p->speed_vec.x < -max_speed)
        p->speed_vec.x = -max_speed;

    double slow_down = steer_slow_down(p);
    if (p->speed_vec.x > 0.0) {
        p->speed_vec.x -= delta * slow_down;
        if (p->speed_vec.x < 0.0)
            p->speed_vec.x = 0.0;
        p->speed_vec.z += delta * (p->speed_vec.x / max_speed) * forward_steer_boost(p);
    }
    if (p->speed_vec.x < 0.0) {
        p->speed_vec.x += delta * slow_down;
        if (p->speed_vec.x > 0.0)
            p->speed_vec.x = 0.0;
        p->speed_vec.z -= delta * (p->speed_vec.x / max_speed) * forward_steer_boost(p);
    }

    p->position_vec.x += p->speed_vec.x * delta;

    while (p->position_vec.x >= STEER_WIDTH_SCALE)
        p->position_vec.x -= STEER_WIDTH_SCALE;
    while (p->position_vec.x < 0.0)
        p->position_vec.x += STEER_WIDTH_SCALE;
    if (p->cur_level_col != bb_player_level_col(p, width)) {
        p->cur_level_col = bb_player_level_col(p, width);
        bb_events_push(out, BB_EVENT_CHANGE_COLUMN, false, 0);
    }
}

static void handle_jump(bb_player *p, double delta, bb_input_query *input, bb_events *out)
{
    if (p->has_backing_data)
        return;

    bool jump = input->pop_jump(input->ctx);
    if (p->is_jumping) {
        p->speed_vec.y -= delta * JUMP_GRAVITY;
        p->position_vec.y += delta * p->speed_vec.y;

        if (p->position_vec.y < 0.0) {
            p->speed_vec.y = 0.0;
            p->position_vec.y = 0.0;
            p->is_jumping = false;
            bb_events_push(out, BB_EVENT_LAND, false, 0);
            return;
        }
        if (p->speed_vec.y > 0.0)
            p->speed_vec.z += delta * FORWARD_JUMP_BOOST;
    } else if (jump) {
        p->is_jumping = true;
        p->speed_vec.y = JUMP_VY_INIT;
        p->position_vec.y = JUMP_POSY_INIT;
        bb_events_push(out, BB_EVENT_JUMP, false, 0);
    }
}

static void handle_boost(bb_player *p, const bb_level *level, bb_events *out)
{
    if (is_on_ground(p) && !is_backing(p)) {
        bb_slab_kind kind = bb_slab_kind_from_item(bb_level_item(
            level, bb_player_level_col(p, bb_level_width(level)), bb_player_level_row(p), p->boost_state));
        if (kind == BB_KIND_BOOST || kind == BB_KIND_OVERDRIVE) {
            double target_speed = p->boost_state ? FORWARD_OVERDRIVE_SPEED : FORWARD_BOOST_SPEED;
            if (p->speed_vec.z < target_speed)
                p->speed_vec.z = target_speed;
            if (p->boost_state) {
                if (bb_player_level_row(p) >= bb_player_best_row(p))
                    p->overdrive_count += 1;
                bb_events_push(out, BB_EVENT_OVERDRIVE, false, 0);
            } else {
                if (bb_player_level_row(p) >= bb_player_best_row(p))
                    p->boost_count += 1;
                bb_events_push(out, BB_EVENT_BOOST, false, 0);
            }
            p->speed_vec.y = p->boost_state ? JUMP_VY_OVERDRIVE_INIT : JUMP_VY_BOOST_INIT;
            p->position_vec.y = JUMP_POSY_INIT;
            p->is_jumping = true;
            p->boost_state = true;
            return;
        }
        p->boost_state = false;
    }
}

double bb_player_time_progress_total(const bb_player *p)
{
    double extra_time_from_boost = EXTRA_TIME_ON_BOOST * (double)p->boost_count;
    double extra_time_from_overdrive = EXTRA_TIME_ON_OVERDRIVE * (double)p->overdrive_count;
    return 100.0 * (1.0 + extra_time_from_boost + extra_time_from_overdrive);
}

static double time_to_complete(const bb_player *p, const bb_level *level)
{
    double base = bb_skill_time_to_complete(bb_level_skill(level));
    return base * bb_player_time_progress_total(p) / 100.0;
}

static double time_pct(const bb_player *p, const bb_level *level)
{
    return bb_points_counter_time_pct(&p->points_counter, time_to_complete(p, level));
}

double bb_player_speed_factor(const bb_player *p, const bb_level *level)
{
    double tp = time_pct(p, level);
    bb_skill skill = bb_level_skill(level);
    return ((100.0 - tp) * bb_skill_begin_speed_factor(skill) + tp * bb_skill_end_speed_factor(skill)) / 100.0;
}

static void handle_forward(bb_player *p, double delta, const bb_level *level)
{
    if (is_backing(p))
        return;

    p->speed_vec.z -= delta * forward_slow_down_linear(p);
    p->speed_vec.z -= delta * forward_slow_down_square(p) * p->speed_vec.z;

    if (p->speed_vec.z > FORWARD_MAX_SPEED)
        p->speed_vec.z = FORWARD_MAX_SPEED;

    if (p->speed_vec.z < 0.0)
        p->speed_vec.z = 0.0;

    p->position_vec.z += delta * p->speed_vec.z * bb_player_speed_factor(p, level);
    if (p->position_vec.z < 0.0)
        p->position_vec.z = 0.0;
}

void bb_player_tick(bb_player *p, double delta, const bb_level *level, bb_input_query *input, bb_events *out)
{
    out->n = 0;
    if (!p->initialized) {
        replace(p, level);
        p->initialized = true;
        bb_events_push(out, BB_EVENT_START, false, 0);
    }
    bb_time_tracker_add(&p->time_tracker, delta);
    handle_steer(p, delta, input, bb_level_width(level), out);
    handle_jump(p, delta, input, out);
    handle_boost(p, level, out);
    handle_forward(p, delta, level);
    handle_fall(p, level, out);
    double dist = bb_player_dist(p);
    double ttc = time_to_complete(p, level);
    bb_points_counter_handle_dist(&p->points_counter, delta, dist, ttc, out);
}

static bb_vec3f export_position3(bb_vec3 v)
{
    bb_vec3f r;
    double z = v.z / FORWARD_DEPTH_SCALE + 0.5;
    r.x = (float)(-v.x * 2.0 * BB_PI / STEER_WIDTH_SCALE);
    r.y = (float)(v.y / JUMP_HEIGHT_SCALE);
    r.z = (float)((z - trunc(z)) - 0.5); /* f64::fract is x - x.trunc() */
    return r;
}

bb_vec3f bb_player_position(const bb_player *p)
{
    return export_position3(p->position_vec);
}

static bb_vec3f export_speed3(bb_vec3 v)
{
    bb_vec3f r;
    r.x = (float)(-v.x * 2.0 * BB_PI / STEER_WIDTH_SCALE);
    r.y = (float)(v.y / JUMP_HEIGHT_SCALE);
    r.z = (float)(v.z / FORWARD_DEPTH_SCALE);
    return r;
}

bb_vec3f bb_player_speed(const bb_player *p)
{
    return export_speed3(p->speed_vec);
}

void bb_player_set_steer_factor(bb_player *p, double sensibility)
{
    p->steer_factor = sensibility;
}

double bb_player_backing_camera_blend(const bb_player *p)
{
    if (!p->has_backing_data)
        return 0.0;
    double now_sec = bb_time_tracker_now_sec(&p->time_tracker);
    double begin_sec = p->backing_data.begin.time_sec;
    double end_sec = p->backing_data.end.time_sec;
    if (now_sec <= begin_sec || now_sec >= end_sec)
        return 0.0;
    double middle_sec = (begin_sec + end_sec) / 2.0;
    double half_range = middle_sec - begin_sec;
    if (half_range > 0.0)
        return now_sec <= middle_sec ? (now_sec - begin_sec) / half_range : (end_sec - now_sec) / half_range;
    return 0.0;
}

int32_t bb_player_score(const bb_player *p)
{
    return bb_points_counter_score(&p->points_counter);
}

void bb_player_set_best_score(bb_player *p, int32_t best_score)
{
    bb_points_counter_set_best_score(&p->points_counter, best_score);
}

double bb_player_best_dist(const bb_player *p)
{
    return bb_points_counter_best_dist(&p->points_counter);
}

bool bb_player_is_boosting(const bb_player *p)
{
    return p->boost_state;
}

double bb_player_time_progress_done(const bb_player *p, const bb_level *level)
{
    double pct = time_pct(p, level);
    if (pct >= 100.0)
        return bb_player_time_progress_total(p);
    return pct * bb_player_time_progress_total(p) / 100.0;
}
