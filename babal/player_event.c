/* from babalcore player_event_kind.rs and player_event.rs, commit fa985cd */
#include <inttypes.h>
#include <stdio.h>

#include "babal.h"

static const char *const KIND_NAMES[] = {
    "start", "jump", "land", "fall", "catch_up", "change_column",
    "boost", "overdrive", "thousand_score", "percent_time", "high_score", "finish",
};

const char *bb_event_kind_as_str(bb_event_kind kind)
{
    return KIND_NAMES[kind];
}

void bb_events_push(bb_events *events, bb_event_kind kind, bool has_arg, int32_t arg)
{
    if (events->n >= BB_EVENTS_MAX)
        return; /* cannot happen: a tick makes at most 10 */
    bb_event *e = &events->v[events->n++];
    e->kind = kind;
    e->has_arg = has_arg;
    e->arg = has_arg ? arg : 0;
}

void bb_event_format(const bb_event *e, char out[32])
{
    if (e->has_arg)
        snprintf(out, 32, "%s: %" PRId32, bb_event_kind_as_str(e->kind), e->arg);
    else
        snprintf(out, 32, "%s", bb_event_kind_as_str(e->kind));
}
