/* Babal under APPS (unit 037, apps.h v1): the rules of babal/ played from the STU-32's keys,
 * drawn by render.c, a beep on bounces. Keys (proposed, with Michael through basecamp, d15):
 * 4 and 6 steer, 8 and ENTER jump, C ends (the framework's). */
#ifndef BABAL_APP_H
#define BABAL_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "apps.h"
#include "babal.h"

#define BABAL_KEY_LEFT 31  /* the 4 key (row 5, column 1) */
#define BABAL_KEY_RIGHT 33 /* 6 */
#define BABAL_KEY_JUMP 26  /* 8 */
#define BABAL_KEY_ENTER 18
#define BABAL_ROWS 512     /* the level's window (docs/stage2.md) */
#define BABAL_LOG 256      /* the scores log's window */
#define BABAL_STEP_MAX_MS 100 /* a tick's step is clamped (quirks 7 and 8: babalcore hangs on wild ones) */
#define BABAL_TICK_MS 50   /* asked for between ticks while playing */

enum { BABAL_TITLE, BABAL_PLAY, BABAL_DONE };

typedef struct {
    bb_row rows[BABAL_ROWS];
    bb_score_entry log[BABAL_LOG];
    bb_level level;
    bb_player player;
    bb_input_steer steer;
    bb_input_jump jump;
    int state;
    bb_skill skill;
    uint32_t now_ms;        /* the app's clock: the sum of the ticks' ms */
    uint32_t done_ms;       /* when the run finished */
    uint32_t games;         /* games started, kept in the save (it seeds the next) */
    int32_t best[5];        /* the best score per skill, kept in the save */
    uint64_t seed;          /* this game's */
    int32_t final_score;
    bool new_best;
    double last_delta;      /* the rules' step at the last tick (the host harness reads it) */
    bb_events last_events;
} babal_ctx;

extern const app_def babal_app;

#endif
