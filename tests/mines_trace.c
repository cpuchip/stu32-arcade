/* SPDX-License-Identifier: MIT */
/* Mines' logic on the host, printing what the oracle prints (oracle/mines/model.py run):
 *   mines_trace SCRIPT    lines 'game SEED N', 'field C,C,...', 'open C', 'flag C', 'solve F', 'cell C', 'mine C'
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mines_logic.h"

static uint32_t ck(const mines_game *g, uint8_t bit)
{
    uint32_t s = 0;
    for (int c = 0; c < MINES_CELLS; c++)
        if (g->cell[c] & bit)
            s += (uint32_t)(c + 1) * (uint32_t)(c + 7);
    return s;
}

static void line(const char *head, const mines_game *g)
{
    static const char *const names[] = {"play", "lost", "won"};
    printf("%sstate=%s opened=%d open_ck=%" PRIu32 " flags=%d flag_ck=%" PRIu32 " mines_ck=%" PRIu32
           " attempts=%d\n",
           head, names[g->state], g->opened, ck(g, MINES_OPEN), g->flags, ck(g, MINES_FLAG),
           g->dealt ? ck(g, MINES_MINE) : 0, g->attempts);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: mines_trace SCRIPT\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f)
        return 1;
    static mines_game g;
    static char buf[4096];
    static uint16_t mines[MINES_CELLS];
    char head[32];
    while (fgets(buf, sizeof buf, f)) {
        char *w = strtok(buf, " \n");
        if (!w || w[0] == '#')
            continue;
        if (!strcmp(w, "game")) {
            uint64_t seed = strtoull(strtok(NULL, " \n"), NULL, 10);
            uint32_t n = (uint32_t)strtoul(strtok(NULL, " \n"), NULL, 10);
            mines_init(&g, seed, n);
            line("game ", &g);
        } else if (!strcmp(w, "field")) {
            int n = 0;
            for (char *p = strtok(NULL, ", \n"); p && n < MINES_CELLS; p = strtok(NULL, ", \n"))
                mines[n++] = (uint16_t)atoi(p);
            mines_set(&g, mines, n);
            line("field ", &g);
        } else if (!strcmp(w, "open") || !strcmp(w, "flag")) {
            int c = atoi(strtok(NULL, " \n"));
            if (w[0] == 'o')
                mines_open(&g, c);
            else
                mines_flag(&g, c);
            snprintf(head, sizeof head, "%s %d ", w, c);
            line(head, &g);
        } else if (!strcmp(w, "cell")) {
            int c = atoi(strtok(NULL, " \n"));
            printf("cell %d %s\n", c, (g.cell[c] & MINES_OPEN) ? "open" : (g.cell[c] & MINES_FLAG) ? "flag" : "closed");
        } else if (!strcmp(w, "mine")) {
            int c = atoi(strtok(NULL, " \n"));
            printf("mine %d %s\n", c, (g.cell[c] & MINES_MINE) ? "yes" : "no");
        } else if (!strcmp(w, "solve")) {
            int c = atoi(strtok(NULL, " \n")), opened = 0;
            bool ok = mines_solve(&g, c, &opened);
            printf("solve %d %s %d\n", c, ok ? "cleared" : "stuck", opened);
        }
    }
    fclose(f);
    return 0;
}
