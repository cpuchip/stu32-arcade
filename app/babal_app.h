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
/* The whole level and the whole scores log (docs/stage2.md: a struggling player drifts back
 * without limit, so a window of recent rows is not enough); tools/drift.c builds bigger ones too. */
#ifndef BABAL_ROWS
#define BABAL_ROWS BB_SLIDING_LENGTH_MAX /* 10,000 packed rows, 80 KB */
#endif
#ifndef BABAL_LOG
#define BABAL_LOG 10240    /* rows a game's best can reach before babalcore's own slide breaks it; 80 KB */
#endif
#define BABAL_STEP_MAX_MS 100 /* a tick's step is clamped (quirks 7 and 8: babalcore hangs on wild ones) */
#define BABAL_TICK_MS 50   /* asked for between ticks while playing */
#define BABAL_SCORES 5     /* babalgame's NB_SCORES (Scores.gd) */

enum { BABAL_TITLE, BABAL_PLAY, BABAL_DONE };

typedef struct {
    bb_packed_row rows[BABAL_ROWS];
    double log[BABAL_LOG];
    uint32_t log_present[BB_LOG_PRESENT_WORDS(BABAL_LOG)];
    bb_level level;
    bb_player player;
    bb_input_steer steer;
    bb_input_jump jump;
    int state;
    bb_skill skill;
    uint32_t now_ms;        /* the app's clock: the sum of the ticks' ms */
    uint32_t done_ms;       /* when the run finished */
    uint32_t games;         /* games started, kept in the save (it seeds the next) */
    struct {
        int32_t points;
        uint8_t skill;
    } scores[BABAL_SCORES];  /* the best scores over all skills, highest first, kept in the save */
    int n_scores;
    uint64_t seed;          /* this game's */
    int32_t final_score;
    bool new_record;        /* the rules' high-score event came this game (babalgame's new_record) */
    bool score_saved;       /* this game's score is in the list (babalgame's score_saved) */
    double last_delta;      /* the rules' step at the last tick (the host harness reads it) */
    bb_events last_events;
} babal_ctx;

extern const app_def babal_app;

#endif
