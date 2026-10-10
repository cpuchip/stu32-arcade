/* SPDX-License-Identifier: MIT */
#include "mines_logic.h"

#include <string.h>

const mines_board mines_boards[MINES_BOARDS] = {{9, 9, 10}, {16, 16, 40}, {30, 16, 99}};
static const int8_t ORDER[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

int mines_neighbours(const mines_game *g, int c, int16_t out[8])
{
    int x = c % g->w, y = c / g->w, n = 0;
    for (int i = 0; i < 8; i++) {
        int nx = x + ORDER[i][0], ny = y + ORDER[i][1];
        if (nx >= 0 && nx < g->w && ny >= 0 && ny < g->h)
            out[n++] = (int16_t)(ny * g->w + nx);
    }
    return n;
}

static void count(mines_game *g, const uint8_t *cell)
{
    for (int c = 0; c < g->cells; c++) {
        int16_t nb[8];
        int n = mines_neighbours(g, c, nb), k = 0;
        for (int i = 0; i < n; i++)
            k += cell[nb[i]] & MINES_MINE;
        g->num[c] = (uint8_t)k;
    }
}

/* open c and, from each 0, its closed neighbours without `stop` set; returns the cells opened. A cell goes on the
 * stack at most once (MINES_QUEUED), so the stack never holds more than the field's cells. */
static int flood(mines_game *g, uint8_t *cell, int c, uint8_t stop)
{
    int sp = 0, opened = 0;
    if (cell[c] & MINES_OPEN)
        return 0;
    g->stack[sp++] = (uint16_t)c;
    cell[c] |= MINES_QUEUED;
    while (sp) {
        c = g->stack[--sp];
        cell[c] = (uint8_t)((cell[c] & ~MINES_QUEUED) | MINES_OPEN);
        opened++;
        if (g->num[c] == 0) {
            int16_t nb[8];
            int n = mines_neighbours(g, c, nb);
            for (int i = n - 1; i >= 0; i--)
                if (!(cell[nb[i]] & (MINES_OPEN | MINES_QUEUED | stop))) {
                    g->stack[sp++] = (uint16_t)nb[i];
                    cell[nb[i]] |= MINES_QUEUED;
                }
        }
    }
    return opened;
}

/* a constraint: an opened number's closed unmarked neighbours U and the mines among them */
typedef struct {
    int16_t u[8];
    int n, need;
} constraint;

static void constrain(const mines_game *g, const uint8_t *s, int c, constraint *k)
{
    int16_t nb[8];
    int n = mines_neighbours(g, c, nb), marked = 0;
    k->n = 0;
    for (int i = 0; i < n; i++) {
        if (s[nb[i]] & MINES_FLAG)
            marked++;
        else if (!(s[nb[i]] & MINES_OPEN))
            k->u[k->n++] = nb[i];
    }
    k->need = g->num[c] - marked;
}

static bool in(const constraint *k, int c)
{
    for (int i = 0; i < k->n; i++)
        if (k->u[i] == c)
            return true;
    return false;
}

/* the solver's own field: MINES_OPEN its opens, MINES_FLAG its marks (known mines, not the player's flags) */
bool mines_solve(mines_game *g, int f, int *opened_out)
{
    uint8_t *s = g->s_cell;
    memset(s, 0, (size_t)g->cells);
    int opened = flood(g, s, f, MINES_FLAG), marked = 0;
    for (;;) {
        bool changed = true;
        while (changed) { /* A */
            changed = false;
            for (int c = 0; c < g->cells; c++) {
                if (!(s[c] & MINES_OPEN) || g->num[c] == 0)
                    continue;
                constraint k;
                constrain(g, s, c, &k);
                if (!k.n)
                    continue;
                if (k.need == 0) {
                    for (int i = 0; i < k.n; i++)
                        if (!(s[k.u[i]] & MINES_OPEN))
                            opened += flood(g, s, k.u[i], MINES_FLAG);
                    changed = true;
                } else if (k.need == k.n) {
                    for (int i = 0; i < k.n; i++)
                        s[k.u[i]] |= MINES_FLAG;
                    marked += k.n;
                    changed = true;
                }
            }
        }
        bool done = false; /* B: a strict subset can only lie within 2 cells, so b is looked for there */
        for (int a = 0; a < g->cells && !done; a++) {
            if (!(s[a] & MINES_OPEN) || g->num[a] == 0)
                continue;
            constraint ka;
            constrain(g, s, a, &ka);
            if (!ka.n)
                continue;
            int ax = a % g->w, ay = a / g->w;
            for (int by = ay - 2; by <= ay + 2 && !done; by++)
                for (int bx = ax - 2; bx <= ax + 2 && !done; bx++) {
                    if (bx < 0 || bx >= g->w || by < 0 || by >= g->h)
                        continue;
                    int b = by * g->w + bx;
                    if (!(s[b] & MINES_OPEN) || g->num[b] == 0)
                        continue;
                    constraint kb;
                    constrain(g, s, b, &kb);
                    if (kb.n <= ka.n)
                        continue;
                    bool sub = true;
                    for (int i = 0; i < ka.n && sub; i++)
                        sub = in(&kb, ka.u[i]);
                    if (!sub)
                        continue;
                    int nd = kb.need - ka.need, nr = kb.n - ka.n;
                    if (nd != 0 && nd != nr)
                        continue;
                    /* the cells of U_b not in U_a, in row order */
                    int16_t d[8];
                    int m = 0;
                    for (int c = 0; c < g->cells && m < nr; c++)
                        if (in(&kb, c) && !in(&ka, c))
                            d[m++] = (int16_t)c;
                    for (int i = 0; i < m; i++) {
                        if (nd == 0) {
                            if (!(s[d[i]] & MINES_OPEN))
                                opened += flood(g, s, d[i], MINES_FLAG);
                        } else {
                            s[d[i]] |= MINES_FLAG;
                            marked++;
                        }
                    }
                    done = true;
                }
        }
        if (done)
            continue;
        int rest = g->cells - opened - marked; /* C */
        if (rest && marked == g->n) {
            for (int c = 0; c < g->cells; c++)
                if (!(s[c] & (MINES_OPEN | MINES_FLAG)))
                    opened += flood(g, s, c, MINES_FLAG);
            continue;
        }
        break;
    }
    if (opened_out)
        *opened_out = opened;
    return opened == g->cells - g->n;
}

bool mines_deal_start(mines_game *g, int f)
{
    if (g->dealt || g->deal_f >= 0 || g->state != MINES_PLAY || (g->cell[f] & MINES_FLAG))
        return false;
    g->deal_f = (int16_t)f;
    g->attempts = 0;
    return true;
}

/* an attempt: the allowed cells (all but f and its neighbours) in row order, the first n of a Fisher-Yates draw */
static void attempt(mines_game *g, int f)
{
    int16_t nb[8];
    int nn = mines_neighbours(g, f, nb), a = 0;
    for (int c = 0; c < g->cells; c++) {
        bool banned = c == f;
        for (int i = 0; i < nn; i++)
            banned |= c == nb[i];
        if (!banned)
            g->list[a++] = (uint16_t)c;
    }
    for (int i = 0; i < g->n; i++) {
        g->rng = mix64(g->rng);
        int j = i + (int)(g->rng % (uint64_t)(a - i));
        uint16_t x = g->list[i];
        g->list[i] = g->list[j];
        g->list[j] = x;
    }
    for (int c = 0; c < g->cells; c++)
        g->cell[c] &= (uint8_t)~MINES_MINE;
    for (int i = 0; i < g->n; i++)
        g->cell[g->list[i]] |= MINES_MINE;
    count(g, g->cell);
}

bool mines_deal_step(mines_game *g, int d)
{
    for (int i = 0; i < d && !g->dealt && g->deal_f >= 0; i++) {
        int f = g->deal_f;
        attempt(g, f);
        g->attempts++;
        if (g->classic || g->attempts == MINES_TRIES || mines_solve(g, f, NULL)) {
            g->mines = (uint16_t)g->n;
            g->dealt = true;
            g->deal_f = -1;
        }
    }
    return g->dealt;
}

void mines_init(mines_game *g, uint64_t seed, uint32_t game, int board, bool classic)
{
    memset(g, 0, sizeof *g);
    g->w = mines_boards[board].w;
    g->h = mines_boards[board].h;
    g->cells = g->w * g->h;
    g->n = mines_boards[board].n;
    g->classic = classic;
    g->rng = mix64(seed ^ mix64(game));
    g->hit = -1;
    g->deal_f = -1;
}

void mines_set(mines_game *g, int board, const uint16_t *mines, int n)
{
    mines_init(g, 0, 0, board, false);
    for (int i = 0; i < n; i++)
        g->cell[mines[i]] |= MINES_MINE;
    for (int c = 0; c < g->cells; c++)
        g->mines = (uint16_t)(g->mines + (g->cell[c] & MINES_MINE));
    count(g, g->cell);
    g->dealt = true;
}

/* true when c is a mine */
static bool open_one(mines_game *g, int c)
{
    if (g->cell[c] & MINES_MINE)
        return true;
    g->opened = (uint16_t)(g->opened + flood(g, g->cell, c, MINES_FLAG));
    return false;
}

void mines_open(mines_game *g, int c)
{
    if (g->state != MINES_PLAY || (g->cell[c] & MINES_FLAG) || g->deal_f >= 0)
        return;
    if (!g->dealt) {
        mines_deal_start(g, c);
        while (!mines_deal_step(g, 1))
            ;
    }
    bool lost = false;
    if (!(g->cell[c] & MINES_OPEN)) {
        lost = open_one(g, c);
        g->hit = (int16_t)(lost ? c : -1);
    } else {
        int16_t nb[8];
        int n = mines_neighbours(g, c, nb), flags = 0;
        for (int i = 0; i < n; i++)
            flags += !!(g->cell[nb[i]] & MINES_FLAG);
        if (flags != g->num[c]) /* an opened 0 with no flags around opens its closed neighbours too */
            return;
        g->hit = -1;
        for (int i = 0; i < n; i++)
            if (!(g->cell[nb[i]] & (MINES_OPEN | MINES_FLAG)) && open_one(g, nb[i])) {
                lost = true;
                if (g->hit < 0)
                    g->hit = nb[i];
            }
    }
    if (lost) {
        g->state = MINES_LOST;
    } else if (g->opened == g->cells - g->mines) {
        g->state = MINES_WON;
        g->flags = 0;
        for (int i = 0; i < g->cells; i++) {
            g->cell[i] = (uint8_t)((g->cell[i] & ~MINES_FLAG) | ((g->cell[i] & MINES_MINE) ? MINES_FLAG : 0));
            g->flags = (uint16_t)(g->flags + !!(g->cell[i] & MINES_FLAG));
        }
    }
}

void mines_flag(mines_game *g, int c)
{
    if (g->state != MINES_PLAY || (g->cell[c] & MINES_OPEN))
        return;
    g->cell[c] ^= MINES_FLAG;
    g->flags = (uint16_t)((g->cell[c] & MINES_FLAG) ? g->flags + 1 : g->flags - 1);
}
