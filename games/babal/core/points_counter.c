/* SPDX-License-Identifier: MIT */
/* from babalcore points_counter.rs, commit fa985cd. The scores log (HashMap<isize, f64>) is a
 * window of the newest rows in caller storage; see babal.h. */
#include <math.h>
#include <string.h>

#include "babal.h"
#include "internal.h"

#define INSTANT_SPEED_WEIGHT 3.0
#define AVERAGE_SPEED_WEIGHT 1.0

void bb_points_counter_init(bb_points_counter *pc, double *log, uint32_t *log_present, size_t log_window)
{
    memset(pc, 0, sizeof *pc);
    memset(log_present, 0, BB_LOG_PRESENT_WORDS(log_window) * sizeof *log_present);
    pc->log = log;
    pc->log_present = log_present;
    pc->log_window = log_window;
    pc->log_max_key = -1;
}

static void set_present(bb_points_counter *pc, size_t slot, bool on)
{
    uint32_t bit = 1u << (slot % 32);
    if (on)
        pc->log_present[slot / 32] |= bit;
    else
        pc->log_present[slot / 32] &= ~bit;
}

static void score_to_log(bb_points_counter *pc, double dist)
{
    intptr_t key = bb_f64_as_isize(round(dist));
    /* Keys rise (they are the best distance, rounded); one that does not, or a negative one,
     * could be lost to the window, so it is counted. */
    if (key < 0 || (pc->log_max_key >= 0 && key < pc->log_max_key)) {
        pc->log_out_of_window++;
        return;
    }
    if (key > pc->log_max_key) {
        /* the rows skipped on the way to key were never logged: clear what their slots held */
        intptr_t from = pc->log_max_key + 1, w = (intptr_t)pc->log_window;
        if (from < key - w + 1)
            from = key - w + 1;
        for (intptr_t j = from; j < key; j++)
            set_present(pc, (size_t)j % pc->log_window, false);
        pc->log_max_key = key;
    }
    size_t slot = (size_t)key % pc->log_window;
    pc->log[slot] = pc->total_points;
    set_present(pc, slot, true);
}

static void score_from_log(bb_points_counter *pc, double dist)
{
    intptr_t key = bb_f64_as_isize(round(dist));
    pc->total_points = 0.0; /* unwrap_or(&0.0) */
    if (key < 0 || key > pc->log_max_key)
        return; /* never logged */
    size_t depth = (size_t)(pc->log_max_key - key);
    if (depth > pc->log_max_depth)
        pc->log_max_depth = depth;
    if (depth >= pc->log_window) {
        pc->log_out_of_window++; /* it may have been logged and given way */
        return;
    }
    size_t slot = (size_t)key % pc->log_window;
    if (pc->log_present[slot / 32] & (1u << (slot % 32)))
        pc->total_points = pc->log[slot];
}

void bb_points_counter_handle_dist(bb_points_counter *pc, double delta, double dist, double time_to_complete, bb_events *out)
{
    if (delta <= 0.0)
        return;
    double delta_best_dist = dist - pc->last_best_dist;
    if (delta_best_dist > 0.0 && !pc->behind)
        bb_time_tracker_add(&pc->time_played_tracker, delta);
    pc->delta_total += delta;
    if (pc->delta_total <= 0.0)
        return;
    if (delta_best_dist > 0.0) {
        double average_speed = dist / pc->delta_total;
        pc->total_points += AVERAGE_SPEED_WEIGHT * delta * average_speed;
        double instant_speed = delta_best_dist / delta;
        pc->total_points += INSTANT_SPEED_WEIGHT * delta_best_dist * instant_speed;
        pc->last_best_dist = dist;
        score_to_log(pc, dist);
    } else {
        score_from_log(pc, dist);
    }
    intptr_t row = bb_f64_as_isize(round(dist));
    if (row >= pc->last_best_row) {
        /* "catch-up" checked on integer rows: the ball may move a little forward when falling. */
        if (pc->behind) {
            bb_events_push(out, BB_EVENT_CATCH_UP, false, 0);
            pc->behind = false;
        }
        pc->last_best_row = row;
    } else {
        pc->behind = true;
    }
    pc->last_dist = dist;
    pc->last_row = row;
    int32_t score = bb_f64_as_i32(floor(pc->total_points));
    if (pc->best_current_score < score) {
        if (score / 1000 > pc->best_current_score / 1000)
            bb_events_push(out, BB_EVENT_THOUSAND_SCORE, true, (score / 1000) * 1000);
        pc->best_current_score = score;
    }
    if (pc->best_previous_score > 0 && pc->best_previous_score < score && !pc->high_score) {
        pc->high_score = true;
        bb_events_push(out, BB_EVENT_HIGH_SCORE, true, score);
    }
    int32_t time_pct = bb_f64_as_i32(floor(bb_points_counter_time_pct(pc, time_to_complete)));
    if (pc->best_time_pct < time_pct) {
        if (time_pct >= 100) {
            bb_events_push(out, BB_EVENT_FINISH, true, score);
        } else {
            int32_t step = time_to_complete < 50.0 ? 20 : time_to_complete > 200.0 ? 5 : 10;
            if (time_pct / step > pc->best_time_pct / step || time_pct == 50 || time_pct == 90 ||
                time_pct == 95 || time_pct == 97 || time_pct == 98 || time_pct == 99)
                bb_events_push(out, BB_EVENT_PERCENT_TIME, true, time_pct);
        }
        pc->best_time_pct = time_pct;
    }
}

int32_t bb_points_counter_score(const bb_points_counter *pc)
{
    return bb_f64_as_i32(pc->total_points);
}

void bb_points_counter_set_best_score(bb_points_counter *pc, int32_t best_score)
{
    pc->best_previous_score = best_score;
}

intptr_t bb_points_counter_best_row(const bb_points_counter *pc)
{
    return bb_f64_as_isize(round(pc->last_best_dist));
}

double bb_points_counter_best_dist(const bb_points_counter *pc)
{
    return pc->last_best_dist;
}

double bb_points_counter_time_pct(const bb_points_counter *pc, double time_to_complete)
{
    if (time_to_complete <= 0.0)
        return 0.0;
    double pct = 100.0 * bb_time_tracker_now_sec(&pc->time_played_tracker) / time_to_complete;
    return pct < 100.0 ? pct : 100.0;
}
