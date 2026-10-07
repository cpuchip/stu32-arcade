/* from babalcore fake_input.rs, commit fa985cd */
#include "babal.h"

void bb_fake_input_init(bb_fake_input *fi)
{
    fi->jumps = 0;
    fi->head = 0;
    fi->count = 0;
}

void bb_fake_input_push_jump(bb_fake_input *fi)
{
    fi->jumps += 1;
}

void bb_fake_input_push_steer(bb_fake_input *fi, double steer)
{
    if (fi->count >= BB_FAKE_STEERS_MAX)
        return;
    fi->steers[(fi->head + fi->count) % BB_FAKE_STEERS_MAX] = steer;
    fi->count++;
}

bool bb_fake_input_pop_jump(void *ctx)
{
    bb_fake_input *fi = ctx;
    if (fi->jumps > 0) {
        fi->jumps -= 1;
        return true;
    }
    return false;
}

double bb_fake_input_pop_steer(void *ctx)
{
    bb_fake_input *fi = ctx;
    if (fi->count == 0)
        return 0.0;
    double f = fi->steers[fi->head];
    fi->head = (fi->head + 1) % BB_FAKE_STEERS_MAX;
    fi->count--;
    return f;
}

bb_input_query bb_fake_input_query(bb_fake_input *fi)
{
    bb_input_query q = {bb_fake_input_pop_jump, bb_fake_input_pop_steer, fi};
    return q;
}
