/* A game played from a script in memory (the words of oracle/gen_scripts.py), printing what
 * oracle/ref prints for babalcore. Shared by the host driver and the S3 bench (oracle.h). */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oracle.h"

typedef struct {
    bb_input_steer *steer;
    bb_input_jump *jump;
    bb_instant now;
} adapter;
static bool adapter_pop_jump(void *ctx)
{
    adapter *a = ctx;
    return bb_input_jump_pop_jump(a->jump, a->now);
}
static double adapter_pop_steer(void *ctx)
{
    adapter *a = ctx;
    return bb_input_steer_pop_steer(a->steer, a->now);
}

int game_run(const char *text, size_t len, bb_row *storage, size_t level_window, bb_score_entry *log,
             size_t log_window, bool window_mode, game_result *res)
{
    char line[512], b[8][17], fb[6][9];
    int64_t clock = 0;
    uint64_t seed = 0;
    bb_skill skill = BB_SKILL_DEFAULT;
    bool level_ready = false;
    bb_level lvl;
    bb_player player;
    bb_input_steer steer;
    bb_input_jump jump;
    bb_player_init(&player, log, log_window);
    bb_input_steer_init(&steer);
    bb_input_jump_init(&jump);
    size_t ticks = 0;
    size_t pos = 0;
    while (pos < len) {
        size_t end = pos;
        while (end < len && text[end] != '\n')
            end++;
        size_t n = end - pos < sizeof line ? end - pos : sizeof line - 1;
        memcpy(line, text + pos, n);
        line[n] = '\0';
        pos = end + 1;
        char *w[4] = {0};
        int nw = 0;
        for (char *tok = strtok(line, " \t\r\n"); tok && nw < 4; tok = strtok(NULL, " \t\r\n"))
            w[nw++] = tok;
        if (nw == 0 || w[0][0] == '#')
            continue;
        bb_instant now = clock;
        if (!strcmp(w[0], "seed"))
            seed = strtoull(w[1], NULL, 10);
        else if (!strcmp(w[0], "skill"))
            skill = bb_skill_from_str(w[1]);
        else if (!strcmp(w[0], "steer_factor"))
            bb_player_set_steer_factor(&player, strtod(w[1], NULL));
        else if (!strcmp(w[0], "best"))
            bb_player_set_best_score(&player, (int32_t)strtol(w[1], NULL, 10));
        else if (!strcmp(w[0], "stick")) {
            if (!strcmp(w[1], "none"))
                bb_input_jump_set_stickiness_secs_f64(&jump, false, 0.0);
            else
                bb_input_jump_set_stickiness_secs_f64(&jump, true, strtod(w[1], NULL));
        } else if (!strcmp(w[0], "sens")) {
            steer.mouse_sensibility = strtod(w[1], NULL);
            steer.keyboard_sensibility = strtod(w[2], NULL);
        } else if (!strcmp(w[0], "wait"))
            clock += (int64_t)strtoull(w[1], NULL, 10);
        else if (!strcmp(w[0], "at"))
            clock = (int64_t)strtoull(w[1], NULL, 10);
        else if (!strcmp(w[0], "L") || !strcmp(w[0], "R")) {
            bool state = !strcmp(w[1], "1");
            double intensity = nw > 2 ? strtod(w[2], NULL) : 1.0;
            int64_t id = nw > 3 ? strtoll(w[3], NULL, 10) : w[0][0] == 'L' ? 1 : 2;
            if (w[0][0] == 'L')
                bb_input_steer_left_key(&steer, id, intensity, state, now);
            else
                bb_input_steer_right_key(&steer, id, intensity, state, now);
        } else if (!strcmp(w[0], "J"))
            bb_input_jump_push_jump(&jump, now);
        else if (!strcmp(w[0], "M"))
            bb_input_steer_mouse_move(&steer, strtod(w[1], NULL));
        else if (!strcmp(w[0], "tick") || !strcmp(w[0], "tickd")) {
            if (!level_ready) {
                bb_level_init(&lvl, seed, skill, storage, level_window);
                level_ready = true;
            }
            double delta;
            if (!strcmp(w[0], "tick")) {
                uint64_t ns = strtoull(w[1], NULL, 10);
                clock += (int64_t)ns;
                delta = (double)ns / 1e9;
            } else {
                delta = strtod(w[1], NULL);
            }
            int64_t t0 = res && res->now_us ? res->now_us() : 0;
            /* babalgame's generate_missing: keep 100 rows ahead of the player. */
            while (bb_level_last(&lvl) < bb_player_level_row(&player) + 100)
                bb_level_generate(&lvl, 100);
            adapter a = {&steer, &jump, clock};
            bb_input_query q = {adapter_pop_jump, adapter_pop_steer, &a};
            bb_events ev;
            bb_player_tick(&player, delta, &lvl, &q, &ev);
            if (res && res->now_us) {
                int64_t dt = res->now_us() - t0;
                res->us_total += dt;
                if (dt > res->us_max)
                    res->us_max = dt;
            }
            outf("t %zu ev=", ticks);
            print_events(&ev);
            bb_vec3f p = bb_player_position(&player), s = bb_player_speed(&player);
            outf(" p=%s,%s,%s s=%s,%s,%s col=%td row=%td dist=%s score=%" PRId32 " best_row=%td best_dist=%s boost=%d"
                   " blend=%s sf=%s tpd=%s tpt=%s first=%td last=%td len=%zu\n",
                   b32(p.x, fb[0]), b32(p.y, fb[1]), b32(p.z, fb[2]), b32(s.x, fb[3]), b32(s.y, fb[4]), b32(s.z, fb[5]),
                   bb_player_level_col(&player, bb_level_width(&lvl)), bb_player_level_row(&player),
                   b64(bb_player_dist(&player), b[0]), bb_player_score(&player), bb_player_best_row(&player),
                   b64(bb_player_best_dist(&player), b[1]), bb_player_is_boosting(&player) ? 1 : 0,
                   b64(bb_player_backing_camera_blend(&player), b[2]), b64(bb_player_speed_factor(&player, &lvl), b[3]),
                   b64(bb_player_time_progress_done(&player, &lvl), b[4]), b64(bb_player_time_progress_total(&player), b[5]),
                   bb_level_first(&lvl), bb_level_last(&lvl), bb_level_len(&lvl));
            print_state(&player);
            if (res && res->on_tick)
                res->on_tick(res->user, &lvl, &player, &ev);
            ticks++;
        } else {
            fprintf(stderr, "unknown script word %s\n", w[0]);
            return 1;
        }
    }
    if (res) {
        res->ticks = ticks;
        res->depth_level = level_ready ? lvl.max_read_depth : 0;
        res->depth_log = player.points_counter.log_max_depth;
        res->level_out = level_ready ? lvl.out_of_window : 0;
        res->log_out = player.points_counter.log_out_of_window;
        res->dropped = steer.dropped_keys;
    }
    if ((level_ready && lvl.out_of_window) || player.points_counter.log_out_of_window || steer.dropped_keys)
        return 3;
    if (window_mode)
        return 0;
    print_log(&player.points_counter);
    if (level_ready) {
        outf("rows first=%td last=%td\n", bb_level_first(&lvl), bb_level_last(&lvl));
        size_t w = bb_level_width(&lvl);
        for (size_t i = 0; i < bb_level_len(&lvl); i++) {
            char text[BB_WIDTH_MAX + 1];
            bb_row_format(bb_level_row_at(&lvl, i), text);
            text[w] = '\0';
            outf("%s%s", i ? "\n" : "", text);
        }
        outf("\n");
    }
    return 0;
}
