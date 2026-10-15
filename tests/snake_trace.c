/* SPDX-License-Identifier: MIT */
/* Snake's logic on the host, printing what the oracle prints (oracle/snake/model.py run):
 *   snake_trace SCRIPT    lines 'game SEED N', 'case DIR FX,FY X,Y ...', 'key 8|6|2|4', 'turn L|R', 'step N',
 *                         'interval E', 'end?' (flying when the game has not ended), 'head?'
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snake_logic.h"

static void queue_str(const snake_game *g, char *q)
{
    int n = 0;
    for (int i = 0; i < g->queued; i++)
        q[n++] = (char)('0' + g->queue[i]);
    if (!n)
        q[n++] = '-';
    q[n] = '\0';
}

static void state(int t, const snake_game *g)
{
    uint32_t ck = 0;
    for (int i = 0; i < g->len; i++)
        ck += (uint32_t)snake_at(g, i) * (uint32_t)(i + 1);
    char q[SNAKE_QUEUE + 2];
    queue_str(g, q);
    int h = snake_head(g);
    printf("%d head=%d,%d len=%d dir=%d queue=%s food=%d eaten=%d body=%" PRIu32 "\n", t, h % SNAKE_W, h / SNAKE_W,
           g->len, g->dir, q, g->food, g->eaten, ck);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: snake_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static snake_game g;
    static char line[16384];
    static uint16_t body[SNAKE_CELLS];
    int t = 0;
    while (fgets(line, sizeof line, f)) {
        char *w = strtok(line, " \n");
        if (!w || w[0] == '#')
            continue;
        if (!strcmp(w, "game")) {
            uint64_t seed = strtoull(strtok(NULL, " \n"), NULL, 10);
            uint32_t n = (uint32_t)strtoul(strtok(NULL, " \n"), NULL, 10);
            snake_init(&g, seed, n);
            t = 0;
            state(t, &g);
        } else if (!strcmp(w, "case")) {
            int dir = atoi(strtok(NULL, " \n"));
            char *fp = strtok(NULL, " \n");
            int food = atoi(strchr(fp, ',') + 1) * SNAKE_W + atoi(fp);
            int n = 0;
            char *p;
            while ((p = strtok(NULL, " \n")) && n < SNAKE_CELLS)
                body[n++] = (uint16_t)(atoi(strchr(p, ',') + 1) * SNAKE_W + atoi(p));
            snake_set(&g, body, n, dir, food);
            t = 0;
        } else if (!strcmp(w, "key")) {
            char *k = strtok(NULL, " \n");
            int dir = k[0] == '8' ? SNAKE_UP : k[0] == '6' ? SNAKE_RIGHT : k[0] == '2' ? SNAKE_DOWN : SNAKE_LEFT;
            bool taken = snake_offer(&g, dir);
            char q[SNAKE_QUEUE + 2];
            queue_str(&g, q);
            printf("key %s %s queue=%s\n", k, taken ? "taken" : "dropped", q);
        } else if (!strcmp(w, "turn")) {
            char *k = strtok(NULL, " \n");
            bool taken = snake_turn(&g, k[0] == 'R');
            char q[SNAKE_QUEUE + 2];
            queue_str(&g, q);
            printf("turn %s %s queue=%s\n", k, taken ? "taken" : "dropped", q);
        } else if (!strcmp(w, "step")) {
            int n = atoi(strtok(NULL, " \n"));
            for (int i = 0; i < n && !g.end; i++) {
                enum snake_end e = snake_step(&g);
                state(++t, &g);
                if (e != SNAKE_ON)
                    printf("end %s\n", e == SNAKE_DEAD ? "dead" : "won");
            }
        } else if (!strcmp(w, "end?")) {
            if (!g.end)
                printf("flying\n");
        } else if (!strcmp(w, "head?")) {
            int h = snake_head(&g);
            printf("head %d,%d dir=%d len=%d\n", h % SNAKE_W, h / SNAKE_W, g.dir, g.len);
        } else if (!strcmp(w, "interval")) {
            int e = atoi(strtok(NULL, " \n"));
            printf("interval %d %" PRIu32 "\n", e, snake_interval((uint32_t)e));
        }
    }
    fclose(f);
    return 0;
}
