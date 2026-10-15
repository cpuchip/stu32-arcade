/* SPDX-License-Identifier: MIT */
/* Frames tied to the model: plays an oracle game script, draws each tick with games/babal/app/render.c, and
 * prints one line per tick, its number and the frame's FNV-1a 64 hash (the S3 must give the same).
 * With TICK... arguments it also writes those frames as PBM files (frame-TICK.pbm).
 * Usage: frames SCRIPT [TICK...] */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oracle.h"
#include "render.h"

static ls027_frame frame;
static int want[64], nwant;

static void null_sink(const char *s, size_t n)
{
    (void)s, (void)n;
}

static void on_tick(void *user, const bb_level *lvl, const bb_player *p, const bb_events *ev)
{
    uint64_t *tick = user;
    (void)ev;
    babal_hud hud = {bb_player_score(p), 0, 0, NULL};
    double total = bb_player_time_progress_total(p);
    hud.progress = (int32_t)(bb_player_time_progress_done(p, lvl) * 1000.0 / total);
    babal_render(&frame, lvl, p, &hud);
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t *b = &frame.px[0][0];
    for (size_t i = 0; i < sizeof frame.px; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    printf("f %" PRIu64 " %016" PRIx64 "\n", *tick, h);
    for (int i = 0; i < nwant; i++)
        if ((uint64_t)want[i] == *tick) {
            char name[64];
            snprintf(name, sizeof name, "frame-%d.pbm", want[i]);
            FILE *o = fopen(name, "wb");
            fprintf(o, "P4\n%d %d\n", LS027_W, LS027_H);
            for (int y = 0; y < LS027_H; y++)
                for (int x = 0; x < LS027_ROW_BYTES; x++) {
                    uint8_t v = frame.px[y][x], r = 0; /* the panel's order is LSB first, PBM's MSB */
                    for (int k = 0; k < 8; k++)
                        r |= (uint8_t)(((v >> k) & 1) << (7 - k));
                    fputc(r, o);
                }
            fclose(o);
        }
    (*tick)++;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: frames SCRIPT [TICK...]\n");
        return 2;
    }
    for (int i = 2; i < argc && nwant < 64; i++)
        want[nwant++] = atoi(argv[i]);
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)size);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size)
        return 1;
    fclose(f);
    static bb_packed_row rows[BB_SLIDING_LENGTH_MAX];
    static double log[10240];
    static uint32_t log_present[BB_LOG_PRESENT_WORDS(10240)];
    uint64_t tick = 0;
    game_result res = {0};
    res.on_tick = on_tick;
    res.user = &tick;
    out_set_sink(null_sink);
    int rc = game_run(text, (size_t)size, rows, BB_SLIDING_LENGTH_MAX, log, log_present, 10240, true, &res);
    free(text);
    return rc;
}
