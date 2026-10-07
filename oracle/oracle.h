/* Shared by the C driver (host) and the S3 bench: one output sink, the formatters, and a game
 * played from a script in memory, so the chip prints exactly what the host prints. */
#ifndef ORACLE_H
#define ORACLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../babal/babal.h"

typedef void (*out_sink)(const char *s, size_t n);
void out_set_sink(out_sink sink); /* default: stdout */
void outf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

const char *b64(double x, char buf[17]);
const char *b32(float x, char buf[9]);
void print_events(const bb_events *ev);
void tok_f(double x);
void tok_v(bb_vec3 v);
void print_pc_tokens(const bb_points_counter *pc, bool lead);
void print_state(const bb_player *p);
void print_log(const bb_points_counter *pc);

typedef struct {
    int64_t (*now_us)(void); /* set to time the rules (row generation and Player::tick), else NULL */
    uint64_t ticks;
    int64_t us_total, us_max;
    size_t depth_level, depth_log;
    uint32_t level_out, log_out, dropped;
    /* called after each tick is printed, when set: the frame tool draws there */
    void (*on_tick)(void *user, const bb_level *lvl, const bb_player *p, const bb_events *ev);
    void *user;
} game_result;

/* Plays a script (oracle/gen_scripts.py's words). window_mode leaves out the final log and rows
 * dumps. Returns 0, 1 for a bad script, 3 when a C-only count moved (no dumps then). */
int game_run(const char *text, size_t len, bb_packed_row *storage, size_t level_window, double *log, uint32_t *log_present,
             size_t log_window, bool window_mode, game_result *res);

#endif
