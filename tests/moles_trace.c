/* SPDX-License-Identifier: MIT */
/* MOLES's logic on the host, printing what the oracle prints (oracle/moles/model.py; the format is
 * oracle/moles/TRACE.md): moles_trace SCRIPT */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moles_logic.h"

static void state(const moles_game *g, void *arg)
{
    (void)arg;
    printf("S step=%" PRIu32 " acc=%" PRIu32 " score=%" PRIu32 " bonks=%" PRIu32 " lives=%d wait=%d stay=%d gap=%d"
           " due=%d stun=%d last=%d over=%d holes=",
           g->step, g->acc, g->score, g->bonks, g->lives, g->wait, g->stay, g->gap, g->due, g->stun, g->last, g->over);
    for (int i = 0; i < MOLES_HOLES; i++) {
        static const char code[] = "ERUBD";
        if (i)
            putchar('/');
        if (g->h[i].state == MOLES_EMPTY)
            putchar('E');
        else
            printf("%c%" PRIu32, code[g->h[i].state], g->h[i].n);
    }
    putchar('\n');
}

static void set_hole(moles_game *g, int i, const char *c)
{
    static const char code[] = "ERUBD";
    const char *p = strchr(code, c[0]);
    if (p && *p)
        g->h[i] = (moles_hole){(uint8_t)(p - code), (uint32_t)(c[0] == 'E' ? 0 : strtoul(c + 1, NULL, 10))};
}

static void set(moles_game *g, const char *name, long v)
{
    if (!strcmp(name, "wait")) g->wait = (uint8_t)v;
    else if (!strcmp(name, "stay")) g->stay = (uint8_t)v;
    else if (!strcmp(name, "gap")) g->gap = (uint8_t)v;
    else if (!strcmp(name, "due")) g->due = (uint8_t)v;
    else if (!strcmp(name, "bonks")) g->bonks = (uint32_t)v;
    else if (!strcmp(name, "score")) g->score = (uint32_t)v;
    else if (!strcmp(name, "lives")) g->lives = (uint8_t)v;
    else if (!strcmp(name, "stun")) g->stun = (uint8_t)v;
    else if (!strcmp(name, "last")) g->last = (int8_t)v;
    else if (!strcmp(name, "acc")) g->acc = (uint32_t)v;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: moles_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static moles_game g;
    static char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \r\n");
        if (!w || w[0] == '#')
            continue;
        char *a1 = strtok(NULL, " \r\n"), *a2 = strtok(NULL, " \r\n"), *a3 = strtok(NULL, " \r\n");
        if (!strcmp(w, "game") && a1 && a2 && a3) {
            moles_init(&g, a1[0] == 'f', (uint32_t)strtoul(a2, NULL, 10), (uint32_t)strtoul(a3, NULL, 10));
        } else if ((!strcmp(w, "down") || !strcmp(w, "up")) && a1) {
            if (a1[0] >= '1' && a1[0] <= '9' && !a1[1])
                moles_key(&g, moles_hole_of(a1[0] - '0'), w[0] == 'd');
        } else if (!strcmp(w, "tick") && a1) {
            moles_tick(&g, (uint32_t)strtoul(a1, NULL, 10), state, NULL);
        } else if (!strcmp(w, "steps") && a1) {
            for (long n = strtol(a1, NULL, 10); n > 0 && !g.over; n--) {
                moles_step(&g);
                state(&g, NULL);
            }
        } else if (!strcmp(w, "set") && a1 && a2) {
            set(&g, a1, strtol(a2, NULL, 10));
        } else if (!strcmp(w, "hole") && a1 && a2) {
            int i = atoi(a1);
            if (i >= 0 && i < MOLES_HOLES)
                set_hole(&g, i, a2);
        } else if (!strcmp(w, "draw?")) {
            printf("draw %" PRIu32 "\n", moles_draw(&g, 1000));
        }
    }
    fclose(f);
    return 0;
}
