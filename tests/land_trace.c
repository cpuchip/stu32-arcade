/* SPDX-License-Identifier: MIT */
/* Land's logic on the host, printing what the oracle prints (oracle/land/model.py run_script):
 *   land_trace SCRIPT    lines 'round SEED N [FUEL]', 'case X Y VX VY A X,Y ...', 'key 4|6|8 0|1', 'step N', 'end?'
 *                        (end? prints flying when the flight has not ended)
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "land_logic.h"

static void state(int t, const land_ship *s)
{
    printf("%d a=%d x=%" PRId32 " y=%" PRId32 " vx=%" PRId32 " vy=%" PRId32 " fuel=%" PRId32 " timer=%d\n", t, s->a, s->x,
           s->y, s->vx, s->vy, s->fuel, s->timer);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: land_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static land_terrain t;
    static land_ship s;
    static char line[1024];
    int step_n = 0, done = 0;
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \n");
        if (!w || w[0] == '#')
            continue;
        if (!strcmp(w, "round")) {
            uint64_t seed = strtoull(strtok(NULL, " \n"), NULL, 10);
            uint32_t n = (uint32_t)strtoul(strtok(NULL, " \n"), NULL, 10);
            char *fuel = strtok(NULL, " \n");
            land_make_terrain(&t, seed, n);
            land_ship_init(&s, &t, fuel ? (int32_t)strtol(fuel, NULL, 10) : LAND_FUEL);
            step_n = done = 0;
            printf("terrain");
            for (int i = 0; i < t.n; i++)
                printf(" %d,%d", t.v[i].x, t.v[i].y);
            printf("\npads");
            for (int k = 0; k < LAND_PADS; k++)
                printf(" %d,%d,%d,%d", t.pad[k].x0, t.pad[k].x1, t.pad[k].y, t.pad[k].mult);
            printf("\n");
            state(0, &s);
        } else if (!strcmp(w, "case")) {
            /* a hand-made terrain; a pad wherever two neighbours share y, each x5, at most LAND_PADS */
            int32_t v[5];
            for (int i = 0; i < 5; i++)
                v[i] = (int32_t)strtol(strtok(NULL, " \n"), NULL, 10);
            memset(&t, 0, sizeof t);
            char *p;
            while ((p = strtok(NULL, " \n")) && t.n < LAND_VERTS_MAX) {
                t.v[t.n].x = (int16_t)atoi(p);
                t.v[t.n].y = (int16_t)atoi(strchr(p, ',') + 1);
                t.n++;
            }
            int k = 0;
            for (int i = 0; i + 1 < t.n && k < LAND_PADS; i++)
                if (t.v[i].y == t.v[i + 1].y)
                    t.pad[k++] = (land_pad){t.v[i].x, t.v[i + 1].x, t.v[i].y, 5};
            for (; k < LAND_PADS; k++)
                t.pad[k] = (land_pad){-1000, -1000, 0, 0}; /* no pad */
            t.start_x = v[0];
            t.start_vx = v[2];
            land_ship_init(&s, &t, LAND_FUEL);
            s.y = v[1];
            s.vy = v[3];
            s.a = (uint8_t)v[4];
            step_n = done = 0;
        } else if (!strcmp(w, "end?")) {
            if (!done)
                printf("flying\n");
        } else if (!strcmp(w, "key")) {
            int k = atoi(strtok(NULL, " \n"));
            int down = atoi(strtok(NULL, " \n"));
            land_key(&s, k == 4 ? LAND_KEY_LEFT : k == 6 ? LAND_KEY_RIGHT : LAND_KEY_THRUST, down);
        } else if (!strcmp(w, "step")) {
            int n = atoi(strtok(NULL, " \n"));
            for (int i = 0; i < n && !done; i++) {
                int mult = 0;
                enum land_end e = land_step(&s, &t, &mult);
                state(++step_n, &s);
                if (e != LAND_FLYING) {
                    printf("end %s %d\n", e == LAND_LANDED ? "land" : "crash", mult);
                    done = 1;
                }
            }
        }
    }
    fclose(f);
    return 0;
}
