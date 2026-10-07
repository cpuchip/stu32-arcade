/* SPDX-License-Identifier: MIT */
/* from babalcore time_tracker.rs, commit fa985cd */
#include <inttypes.h>
#include <stdio.h>

#include "babal.h"
#include "internal.h"

void bb_time_tracker_add(bb_time_tracker *tt, double delta)
{
    tt->now_sec_f64 += delta;
    if (tt->now_sec_f64 < 0.0 || tt->now_sec_f64 >= BB_TIME_TRACKER_MAX_SEC)
        tt->now_sec_f64 = 0.0;
}

double bb_time_tracker_now_sec(const bb_time_tracker *tt)
{
    return tt->now_sec_f64;
}

int64_t bb_time_tracker_now_msec(const bb_time_tracker *tt)
{
    return bb_f64_as_i64(tt->now_sec_f64 * 1000.0);
}

void bb_time_tracker_format(const bb_time_tracker *tt, char out[32])
{
    int64_t now_msec = bb_time_tracker_now_msec(tt);
    snprintf(out, 32, "%" PRId64 ".%03" PRId64, now_msec / 1000, now_msec % 1000);
}
