/* SPDX-License-Identifier: MIT */
/* 3720's logic on the host, printing what the oracle prints (oracle/rocks/model.py; the format is
 * oracle/rocks/TRACE.md): rocks_trace SCRIPT */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rocks_logic.h"

static void state(const rocks_game *g, void *arg)
{
    (void)arg;
    printf("S step=%" PRIu32 " acc=%" PRIu32 " x=%" PRId32 " y=%" PRId32 " vx=%" PRId32 " vy=%" PRId32 " a=%d turn=%d"
           " cool=%d shield=%d lost=%d ships=%d score=%" PRIu32 " next=%" PRIu32 " wave=%d pause=%d clock=%" PRIu32
           " over=%d shots=",
           g->step, g->acc, g->x, g->y, g->vx, g->vy, g->a, g->turn, g->cool, g->shield, g->lost, g->ships, g->score,
           g->next, g->wave, g->pause, g->clock, g->over);
    if (!g->nshot)
        putchar('-');
    for (int i = 0; i < g->nshot; i++) {
        const rocks_shot *s = &g->shot[i];
        printf("%s%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%d", i ? ";" : "", s->x, s->y, s->vx, s->vy, s->life);
    }
    fputs(" rocks=", stdout);
    for (int i = 0; i < ROCKS_SLOTS; i++) {
        const rocks_rock *r = &g->r[i];
        if (i)
            putchar('/');
        if (r->size == ROCKS_NONE)
            putchar('-');
        else
            printf("%c,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%d", " LMS"[r->size], r->x, r->y, r->vx, r->vy,
                   r->outline);
    }
    putchar('\n');
}

static int key_of(const char *k)
{
    static const char *const names[ROCKS_KEYS] = {"4", "S1", "6", "S6", "7", "8", "9", "S2", "G", "5", "X", "S5",
                                                  "E", "P"};
    for (int i = 0; i < ROCKS_KEYS; i++)
        if (!strcmp(k, names[i]))
            return i;
    return -1;
}

static void set_rock(rocks_game *g, int i, const char *c)
{
    rocks_rock *r = &g->r[i];
    long x, y, vx, vy;
    int o;
    const char *p = strchr(" LMS", c[0]);
    if (c[0] == '-')
        *r = (rocks_rock){0, 0, 0, 0, ROCKS_NONE, 0};
    else if (p && *p != ' ' && sscanf(c + 1, ",%ld,%ld,%ld,%ld,%d", &x, &y, &vx, &vy, &o) == 5)
        *r = (rocks_rock){(int32_t)x, (int32_t)y, (int32_t)vx, (int32_t)vy, (uint8_t)(p - " LMS"), (uint8_t)o};
}

static void set(rocks_game *g, const char *name, long v)
{
    if (!strcmp(name, "x")) g->x = (int32_t)v;
    else if (!strcmp(name, "y")) g->y = (int32_t)v;
    else if (!strcmp(name, "vx")) g->vx = (int32_t)v;
    else if (!strcmp(name, "vy")) g->vy = (int32_t)v;
    else if (!strcmp(name, "a")) g->a = (uint8_t)v;
    else if (!strcmp(name, "turn")) g->turn = (uint8_t)v;
    else if (!strcmp(name, "cool")) g->cool = (uint8_t)v;
    else if (!strcmp(name, "shield")) g->shield = (uint8_t)v;
    else if (!strcmp(name, "lost")) g->lost = (uint8_t)v;
    else if (!strcmp(name, "ships")) g->ships = (uint8_t)v;
    else if (!strcmp(name, "score")) g->score = (uint32_t)v;
    else if (!strcmp(name, "next")) g->next = (uint32_t)v;
    else if (!strcmp(name, "wave")) g->wave = (uint16_t)v;
    else if (!strcmp(name, "pause")) g->pause = (uint16_t)v;
    else if (!strcmp(name, "clock")) g->clock = (uint32_t)v;
    else if (!strcmp(name, "acc")) g->acc = (uint32_t)v;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: rocks_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static rocks_game g;
    static char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \r\n");
        if (!w || w[0] == '#')
            continue;
        char *a[5];
        for (int i = 0; i < 5; i++)
            a[i] = strtok(NULL, " \r\n");
        if (!strcmp(w, "game") && a[2]) {
            rocks_init(&g, a[0][0] == 'f', (uint32_t)strtoul(a[1], NULL, 10), (uint32_t)strtoul(a[2], NULL, 10));
        } else if ((!strcmp(w, "down") || !strcmp(w, "up")) && a[0]) {
            rocks_key(&g, key_of(a[0]), w[0] == 'd');
        } else if (!strcmp(w, "tick") && a[0]) {
            rocks_tick(&g, (uint32_t)strtoul(a[0], NULL, 10), state, NULL);
        } else if (!strcmp(w, "steps") && a[0]) {
            for (long n = strtol(a[0], NULL, 10); n > 0 && !g.over; n--) {
                rocks_step(&g);
                state(&g, NULL);
            }
        } else if (!strcmp(w, "set") && a[1]) {
            set(&g, a[0], strtol(a[1], NULL, 10));
        } else if (!strcmp(w, "rock") && a[1]) {
            int i = atoi(a[0]);
            if (i >= 0 && i < ROCKS_SLOTS)
                set_rock(&g, i, a[1]);
        } else if (!strcmp(w, "shot") && a[4]) {
            if (g.nshot < ROCKS_SHOTS)
                g.shot[g.nshot++] = (rocks_shot){(int32_t)strtol(a[0], NULL, 10), (int32_t)strtol(a[1], NULL, 10),
                                                 (int32_t)strtol(a[2], NULL, 10), (int32_t)strtol(a[3], NULL, 10),
                                                 (uint8_t)atoi(a[4])};
        } else if (!strcmp(w, "clear") && a[0]) {
            if (!strcmp(a[0], "rocks"))
                for (int i = 0; i < ROCKS_SLOTS; i++)
                    g.r[i].size = ROCKS_NONE;
            else if (!strcmp(a[0], "shots"))
                g.nshot = 0;
        } else if (!strcmp(w, "draw?")) {
            printf("draw %" PRIu32 "\n", rocks_draw(&g, 1000));
        }
    }
    fclose(f);
    return 0;
}
