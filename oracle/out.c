/* The output sink and the formatters the drivers share (oracle.h). */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oracle.h"

static void stdout_sink(const char *s, size_t n)
{
    fwrite(s, 1, n, stdout);
}
static out_sink sink = stdout_sink;

void out_set_sink(out_sink s)
{
    sink = s;
}

void outf(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof buf) {
        fputs("outf: line too long\n", stderr);
        abort();
    }
    sink(buf, (size_t)n);
}

/* Floats as their bits; any NaN as "nan" (its sign is not meaningful, and Rust's Debug drops it). */
const char *b64(double x, char buf[17])
{
    uint64_t u;
    if (x != x)
        return "nan";
    memcpy(&u, &x, 8);
    snprintf(buf, 17, "%016" PRIx64, u);
    return buf;
}
const char *b32(float x, char buf[9])
{
    uint32_t u;
    if (x != x)
        return "nan";
    memcpy(&u, &x, 4);
    snprintf(buf, 9, "%08" PRIx32, u);
    return buf;
}

void print_events(const bb_events *ev)
{
    for (int i = 0; i < ev->n; i++) {
        if (i)
            outf(",");
        if (ev->v[i].has_arg)
            outf("%s:%" PRId32, bb_event_kind_as_str(ev->v[i].kind), ev->v[i].arg);
        else
            outf("%s", bb_event_kind_as_str(ev->v[i].kind));
    }
}

/* The player's every field in declaration order, as oracle/ref reads it from Debug. */
void tok_f(double x)
{
    char b[17];
    outf(",%s", b64(x, b));
}
void tok_v(bb_vec3 v)
{
    tok_f(v.x);
    tok_f(v.y);
    tok_f(v.z);
}
/* lead: the tokens continue a list (each starts with a comma); otherwise the first has none. */
void print_pc_tokens(const bb_points_counter *pc, bool lead)
{
    char b[17];
    if (lead)
        tok_f(pc->total_points);
    else
        outf("%s", b64(pc->total_points, b));
    tok_f(pc->last_dist);
    outf(",%td", pc->last_row);
    tok_f(pc->last_best_dist);
    outf(",%td", pc->last_best_row);
    tok_f(pc->time_played_tracker.now_sec_f64);
    tok_f(pc->delta_total);
    outf(",%d,%" PRId32 ",%" PRId32 ",%d,%" PRId32, pc->behind ? 1 : 0, pc->best_current_score,
           pc->best_previous_score, pc->high_score ? 1 : 0, pc->best_time_pct);
}
void print_state(const bb_player *p)
{
    char b[17];
    outf("s %d,%s", p->initialized ? 1 : 0, b64(p->steer_factor, b));
    tok_v(p->position_vec);
    tok_v(p->speed_vec);
    tok_f(p->time_tracker.now_sec_f64);
    outf(",%d,%d,%td", p->is_jumping ? 1 : 0, p->boost_state ? 1 : 0, p->cur_level_col);
    if (p->has_backing_data) {
        const bb_point_in_time *pts[4] = {&p->backing_data.begin, &p->backing_data.top_fall,
                                          &p->backing_data.bottom_fall, &p->backing_data.end};
        outf(",S");
        for (int i = 0; i < 4; i++) {
            tok_v(pts[i]->position_vec);
            tok_f(pts[i]->time_sec);
        }
    } else {
        outf(",N");
    }
    print_pc_tokens(&p->points_counter, true);
    outf(",%td,%td\n", p->boost_count, p->overdrive_count);
}

/* The log in row order, as oracle/ref sorts babalcore's HashMap. */
void print_log(const bb_points_counter *pc)
{
    char b[17];
    int first = 1;
    outf("log ");
    for (intptr_t k = 0; k <= pc->log_max_key; k++) {
        const bb_score_entry *e = &pc->log[(size_t)k % pc->log_window];
        if (e->present && e->key == k) {
            outf("%s%td:%s", first ? "" : " ", k, b64(e->points, b));
            first = 0;
        }
    }
    outf("\n");
}
