/* SPDX-License-Identifier: MIT */
/* CATCH's logic on the host, printing what the oracle prints (oracle/catch/model.py; the format is
 * oracle/catch/TRACE.md): catch_trace SCRIPT */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "catch_logic.h"

static void state(const catch_game *g, void *arg)
{
    (void)arg;
    printf("S step=%" PRIu32 " acc=%" PRIu32 " bx=%d score=%" PRIu32 " lives=%d fall=%d gap=%d max=%d count=%d last=%d"
           " bump=%d over=%d stars=",
           g->step, g->acc, g->bx, g->score, g->lives, g->fall, g->gap, g->max, g->count, g->last, g->bump, g->over);
    if (!g->n)
        putchar('-');
    for (int i = 0; i < g->n; i++)
        printf("%s%d,%d", i ? ";" : "", g->star[i].x, g->star[i].y);
    putchar('\n');
}

static int key_of(const char *k)
{
    static const char *const names[CATCH_KEYS] = {"4", "S1", "6", "S6"};
    for (int i = 0; i < CATCH_KEYS; i++)
        if (!strcmp(k, names[i]))
            return i;
    return -1;
}

static void set(catch_game *g, const char *name, long v)
{
    if (!strcmp(name, "bx")) g->bx = (int16_t)v;
    else if (!strcmp(name, "score")) g->score = (uint32_t)v;
    else if (!strcmp(name, "lives")) g->lives = (uint8_t)v;
    else if (!strcmp(name, "fall")) g->fall = (uint8_t)v;
    else if (!strcmp(name, "gap")) g->gap = (uint16_t)v;
    else if (!strcmp(name, "max")) g->max = (uint8_t)v;
    else if (!strcmp(name, "count")) g->count = (uint16_t)v;
    else if (!strcmp(name, "last")) g->last = (int16_t)v;
    else if (!strcmp(name, "acc")) g->acc = (uint32_t)v;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: catch_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static catch_game g;
    static char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \r\n");
        if (!w || w[0] == '#')
            continue;
        char *a1 = strtok(NULL, " \r\n"), *a2 = strtok(NULL, " \r\n"), *a3 = strtok(NULL, " \r\n");
        if (!strcmp(w, "game") && a1 && a2 && a3) {
            int level = a1[0] == 'f' ? CATCH_FULL : a1[0] == 'm' ? CATCH_MIDDLE : CATCH_GENTLE;
            catch_init(&g, level, (uint32_t)strtoul(a2, NULL, 10), (uint32_t)strtoul(a3, NULL, 10));
        } else if ((!strcmp(w, "down") || !strcmp(w, "up")) && a1) {
            catch_key(&g, key_of(a1), w[0] == 'd');
        } else if (!strcmp(w, "tick") && a1) {
            catch_tick(&g, (uint32_t)strtoul(a1, NULL, 10), state, NULL);
        } else if (!strcmp(w, "steps") && a1) {
            for (long n = strtol(a1, NULL, 10); n > 0 && !g.over; n--) {
                catch_step(&g);
                state(&g, NULL);
            }
        } else if (!strcmp(w, "set") && a1 && a2) {
            set(&g, a1, strtol(a2, NULL, 10));
        } else if (!strcmp(w, "star") && a1 && a2) {
            if (g.n < CATCH_STARS)
                g.star[g.n++] = (catch_star){(int16_t)atoi(a1), (int16_t)atoi(a2)};
        } else if (!strcmp(w, "clear") && a1 && !strcmp(a1, "stars")) {
            g.n = 0;
        } else if (!strcmp(w, "draw?")) {
            printf("draw %" PRIu32 "\n", catch_draw(&g, 1000));
        }
    }
    fclose(f);
    return 0;
}
