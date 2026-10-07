/* from babalcore input_jump.rs, commit fa985cd. Instants are nanoseconds given by the caller. */
#include "babal.h"

void bb_input_jump_init(bb_input_jump *ij)
{
    ij->has_stickiness = false;
    ij->stickiness = 0;
    ij->first_pop = false;
    ij->has_last_request = false;
    ij->last_request = 0;
    bb_input_jump_set_stickiness_secs_f64(ij, true, BB_DEFAULT_STICKINESS_SECS_F64);
}

void bb_input_jump_set_stickiness_secs_f64(bb_input_jump *ij, bool has, double secs)
{
    if (has && secs > 0.0) {
        ij->has_stickiness = true;
        uint64_t ns = bb_duration_from_secs_f64(secs);
        ij->stickiness = ns > (uint64_t)INT64_MAX ? INT64_MAX : (bb_duration)ns; /* past 292 years */
    } else {
        ij->has_stickiness = false;
    }
}

void bb_input_jump_push_jump(bb_input_jump *ij, bb_instant now)
{
    /* The Rust first keeps the later of the two requests, then overwrites it with now. */
    ij->has_last_request = true;
    ij->last_request = now;
    ij->first_pop = true;
}

bool bb_input_jump_pop_jump(bb_input_jump *ij, bb_instant now)
{
    if (!ij->has_last_request)
        return false;
    bool state = false;
    if (ij->has_stickiness) {
        if (now >= ij->last_request)
            state = now - ij->last_request <= ij->stickiness;
        else
            state = false;
    }
    if (!state)
        ij->has_last_request = false;
    if (ij->first_pop) {
        ij->first_pop = false;
        return true;
    }
    return state;
}
