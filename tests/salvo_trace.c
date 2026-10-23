/* SPDX-License-Identifier: MIT */
/* Salvo's logic on the host, printing what the oracle prints (oracle/salvo/model.py; the format is
 * oracle/salvo/TRACE.md): salvo_trace SCRIPT */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "salvo_logic.h"

static void list(const salvo_pt *p, int n)
{
    if (!n) {
        fputs("-", stdout);
        return;
    }
    for (int i = 0; i < n; i++)
        printf("%s%d,%d", i ? ";" : "", p[i].x, p[i].y);
}

static void state(const salvo_game *g, void *arg)
{
    (void)arg;
    printf("S step=%" PRIu32 " acc=%" PRIu32 " sx=%d lives=%d score=%" PRIu32 " next=%" PRIu32
           " wave=%d over=%d ship=%d wavet=%d fx=%d fy=%d fdir=%d sway=%" PRIu32 " dive=%d cool=%d shots=",
           g->step, g->acc, g->sx, g->lives, g->score, g->next, g->wave, g->over, g->ship_t, g->wave_t, g->fx, g->fy,
           g->fdir, g->sway, g->dive, g->cool);
    list(g->shot, g->nshot);
    fputs(" eshots=", stdout);
    list(g->eshot, g->neshot);
    fputs(" aliens=", stdout);
    for (int i = 0; i < SALVO_ALIENS; i++) {
        const salvo_alien *a = &g->a[i];
        if (i)
            putchar('/');
        if (a->state == SALVO_SLOT)
            putchar('S');
        else if (a->state == SALVO_DEAD)
            putchar('X');
        else if (a->state == SALVO_DIVING)
            printf("D%d,%d,%d,%d,%d", a->x, a->y, a->k, a->vx, a->fire_at);
        else
            printf("R%d,%d", a->x, a->y);
    }
    putchar('\n');
}

static int key_of(const char *k)
{
    static const char *const names[SALVO_KEYS] = {"4", "S1", "6", "S6", "8", "E", "9", "X", "7", "G", "S2", "S5"};
    for (int i = 0; i < SALVO_KEYS; i++)
        if (!strcmp(k, names[i]))
            return i;
    return -1;
}

/* an alien's code as the state line writes it */
static void set_alien(salvo_game *g, int i, const char *c)
{
    salvo_alien *a = &g->a[i];
    int x = 0, y = 0, k = 0, vx = 0, f = 0;
    if (c[0] == 'S') {
        *a = (salvo_alien){SALVO_SLOT, 0, 0, 0, 0, 0};
    } else if (c[0] == 'X') {
        *a = (salvo_alien){SALVO_DEAD, 0, 0, 0, 0, 0};
    } else if (c[0] == 'D' && sscanf(c + 1, "%d,%d,%d,%d,%d", &x, &y, &k, &vx, &f) == 5) {
        *a = (salvo_alien){SALVO_DIVING, (int16_t)x, (int16_t)y, (int16_t)k, (int8_t)vx, (int8_t)f};
    } else if (c[0] == 'R' && sscanf(c + 1, "%d,%d", &x, &y) == 2) {
        *a = (salvo_alien){SALVO_RETURNING, (int16_t)x, (int16_t)y, 0, 0, 0};
    }
}

static void set(salvo_game *g, const char *name, long v)
{
    if (!strcmp(name, "sx")) g->sx = (int16_t)v;
    else if (!strcmp(name, "lives")) g->lives = (uint8_t)v;
    else if (!strcmp(name, "score")) g->score = (uint32_t)v;
    else if (!strcmp(name, "wave")) g->wave = (uint16_t)v;
    else if (!strcmp(name, "over")) g->over = v != 0;
    else if (!strcmp(name, "ship")) g->ship_t = (uint8_t)v;
    else if (!strcmp(name, "wavet")) g->wave_t = (uint8_t)v;
    else if (!strcmp(name, "fx")) g->fx = (int16_t)v;
    else if (!strcmp(name, "fy")) g->fy = (int16_t)v;
    else if (!strcmp(name, "fdir")) g->fdir = (int8_t)v;
    else if (!strcmp(name, "sway")) g->sway = (uint32_t)v;
    else if (!strcmp(name, "dive")) g->dive = (uint16_t)v;
    else if (!strcmp(name, "cool")) g->cool = (uint8_t)v;
    else if (!strcmp(name, "next")) g->next = (uint32_t)v;
    else if (!strcmp(name, "acc")) g->acc = (uint32_t)v;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: salvo_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static salvo_game g;
    static char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \r\n");
        if (!w || w[0] == '#')
            continue;
        char *a1 = strtok(NULL, " \r\n"), *a2 = strtok(NULL, " \r\n");
        if (!strcmp(w, "game") && a1 && a2) {
            salvo_init(&g, (uint32_t)strtoul(a1, NULL, 10), (uint32_t)strtoul(a2, NULL, 10));
        } else if ((!strcmp(w, "down") || !strcmp(w, "up")) && a1) {
            salvo_key(&g, key_of(a1), w[0] == 'd');
        } else if (!strcmp(w, "tick") && a1) {
            salvo_tick(&g, (uint32_t)strtoul(a1, NULL, 10), state, NULL);
        } else if (!strcmp(w, "steps") && a1) {
            for (long n = strtol(a1, NULL, 10); n > 0 && !g.over; n--) {
                salvo_step(&g);
                state(&g, NULL);
            }
        } else if (!strcmp(w, "set") && a1 && a2) {
            set(&g, a1, strtol(a2, NULL, 10));
        } else if (!strcmp(w, "alien") && a1 && a2) {
            int i = atoi(a1);
            if (i >= 0 && i < SALVO_ALIENS)
                set_alien(&g, i, a2);
        } else if ((!strcmp(w, "shot") || !strcmp(w, "eshot")) && a1 && a2) {
            salvo_pt p = {(int16_t)atoi(a1), (int16_t)atoi(a2)};
            if (w[0] == 's' && g.nshot < SALVO_SHOTS)
                g.shot[g.nshot++] = p;
            else if (w[0] == 'e' && g.neshot < SALVO_ESHOTS)
                g.eshot[g.neshot++] = p;
        } else if (!strcmp(w, "clear") && a1) {
            if (!strcmp(a1, "shots"))
                g.nshot = 0;
            else if (!strcmp(a1, "eshots"))
                g.neshot = 0;
        } else if (!strcmp(w, "draw?")) {
            printf("draw %" PRIu32 "\n", salvo_draw(&g, 1000));
        }
    }
    fclose(f);
    return 0;
}
