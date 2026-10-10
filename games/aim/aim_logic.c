/* SPDX-License-Identifier: MIT */
#include <string.h>

#include "aim_logic.h"

enum { AIM_STEP_TILE, AIM_STEP_OP, AIM_STEP_SWAP };

static int32_t gcd(int32_t a, int32_t b)
{
    if (a < 0)
        a = -a;
    while (b) {
        int32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static aim_frac frac(int32_t n, int32_t d)
{
    if (d < 0) {
        n = -n;
        d = -d;
    }
    int32_t g = gcd(n, d);
    if (g > 1) {
        n /= g;
        d /= g;
    }
    return (aim_frac){n, d};
}

static bool feq(aim_frac a, aim_frac b)
{
    return a.n == b.n && a.d == b.d;
}

/* op is AIM_PLUS..AIM_DIVIDE; false for a division by zero */
static bool apply(enum aim_key op, aim_frac a, aim_frac b, aim_frac *out)
{
    switch (op) {
    case AIM_PLUS:
        *out = frac(a.n * b.d + b.n * a.d, a.d * b.d);
        return true;
    case AIM_MINUS:
        *out = frac(a.n * b.d - b.n * a.d, a.d * b.d);
        return true;
    case AIM_TIMES:
        *out = frac(a.n * b.n, a.d * b.d);
        return true;
    default:
        if (b.n == 0)
            return false;
        *out = frac(a.n * b.d, a.d * b.n);
        return true;
    }
}

void aim_round_init(aim_round *r, const uint8_t tiles[4], int32_t target)
{
    memset(r, 0, sizeof *r);
    memcpy(r->tiles, tiles, 4);
    r->target = target;
}

static void push_step(aim_round *r, aim_step s)
{
    if (r->n_steps < AIM_STEPS_MAX) /* cannot fill: see AIM_STEPS_MAX */
        r->steps[r->n_steps++] = s;
}

/* true when there was no entry or it matched an unused tile, now pushed; false when it is refused */
static bool end_entry(aim_round *r)
{
    if (!r->entry[0])
        return true;
    int n = 0;
    for (const char *p = r->entry; *p; p++)
        n = n * 10 + (*p - '0');
    r->entry[0] = 0;
    for (int i = 0; i < 4; i++) {
        if (!(r->used >> i & 1) && r->tiles[i] == n) {
            r->used |= (uint8_t)(1u << i);
            r->stack[r->sp++] = (aim_frac){n, 1};
            push_step(r, (aim_step){.kind = AIM_STEP_TILE, .tile = (uint8_t)i});
            return true;
        }
    }
    return false;
}

static void swap_xy(aim_round *r)
{
    aim_frac t = r->stack[r->sp - 1];
    r->stack[r->sp - 1] = r->stack[r->sp - 2];
    r->stack[r->sp - 2] = t;
}

bool aim_round_key(aim_round *r, enum aim_key k)
{
    if (r->won)
        return true;
    bool beep = false;
    size_t len = strlen(r->entry);
    if (k <= AIM_K9) {
        if (len < AIM_ENTRY_MAX) {
            r->entry[len] = (char)('0' + k);
            r->entry[len + 1] = 0;
        } else {
            beep = true;
        }
    } else if (k == AIM_BACK) {
        if (len) {
            r->entry[len - 1] = 0;
        } else if (r->n_steps) {
            aim_step s = r->steps[--r->n_steps];
            if (s.kind == AIM_STEP_TILE) {
                r->sp--;
                r->used &= (uint8_t)~(1u << s.tile);
            } else if (s.kind == AIM_STEP_OP) {
                r->stack[r->sp - 1] = s.a;
                r->stack[r->sp++] = s.b;
            } else {
                swap_xy(r);
            }
        } else {
            beep = true;
        }
    } else if (k == AIM_ENTER) {
        beep = !len || !end_entry(r);
    } else {
        if (!end_entry(r) || r->sp < 2) {
            beep = true;
        } else if (k == AIM_SWAP) {
            swap_xy(r);
            if (r->n_steps && r->steps[r->n_steps - 1].kind == AIM_STEP_SWAP)
                r->n_steps--;
            else
                push_step(r, (aim_step){.kind = AIM_STEP_SWAP});
        } else {
            aim_frac a = r->stack[r->sp - 2], b = r->stack[r->sp - 1], v;
            if (!apply(k, a, b, &v)) {
                beep = true;
            } else {
                r->sp--;
                r->stack[r->sp - 1] = v;
                push_step(r, (aim_step){.kind = AIM_STEP_OP, .a = a, .b = b});
            }
        }
    }
    if (r->used == 0xF && r->sp == 1 && feq(r->stack[0], (aim_frac){r->target, 1}))
        r->won = true;
    return beep;
}

/* ---- the solver ---- */

static int bits(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1)
        n++;
    return n;
}

/* a subset's tile values as one number, digits ascending: equal multisets give equal keys */
static uint32_t mkey(const uint8_t tiles[4], unsigned m)
{
    uint8_t v[4];
    int n = 0;
    for (int i = 0; i < 4; i++)
        if (m >> i & 1)
            v[n++] = tiles[i];
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && v[j - 1] > v[j]; j--) {
            uint8_t t = v[j];
            v[j] = v[j - 1];
            v[j - 1] = t;
        }
    uint32_t k = 0;
    for (int i = 0; i < n; i++)
        k = k * 10 + v[i];
    return k;
}

static bool add_val(aim_solver *s, unsigned m, uint16_t cap, aim_frac v, uint32_t c)
{
    aim_val *p = &s->vals[s->at[m]];
    for (int i = 0; i < s->n[m]; i++)
        if (feq(p[i].v, v)) {
            p[i].count += c;
            return true;
        }
    if (s->n[m] >= cap)
        return false;
    p[s->n[m]++] = (aim_val){v, c};
    return true;
}

bool aim_solve(aim_solver *s, const uint8_t tiles[4])
{
    static const uint16_t caps[5] = {0, 1, AIM_VALS_2, AIM_VALS_3, AIM_VALS_4};
    memset(s->n, 0, sizeof s->n);
    uint16_t at = 0;
    for (int k = 1; k <= 4; k++)
        for (unsigned m = 1; m < 16; m++)
            if (bits(m) == k) {
                s->at[m] = at;
                at = (uint16_t)(at + caps[k]);
            }
    for (int i = 0; i < 4; i++) {
        unsigned m = 1u << i;
        s->vals[s->at[m]] = (aim_val){{tiles[i], 1}, 1};
        s->n[m] = 1;
    }
    for (int k = 2; k <= 4; k++)
        for (unsigned m = 1; m < 16; m++) {
            if (bits(m) != k)
                continue;
            uint32_t seen[14][2];
            int n_seen = 0;
            for (unsigned l = (m - 1) & m; l; l = (l - 1) & m) {
                unsigned r = m ^ l;
                uint32_t kl = mkey(tiles, l), kr = mkey(tiles, r);
                bool dup = false;
                for (int i = 0; i < n_seen; i++)
                    dup |= seen[i][0] == kl && seen[i][1] == kr;
                if (dup)
                    continue;
                seen[n_seen][0] = kl;
                seen[n_seen][1] = kr;
                n_seen++;
                for (int i = 0; i < s->n[l]; i++)
                    for (int j = 0; j < s->n[r]; j++) {
                        aim_val a = s->vals[s->at[l] + i], b = s->vals[s->at[r] + j];
                        uint32_t c = a.count * b.count;
                        for (enum aim_key op = AIM_PLUS; op <= AIM_DIVIDE; op++) {
                            aim_frac v;
                            if (apply(op, a.v, b.v, &v) && !add_val(s, m, caps[k], v, c))
                                return false;
                        }
                    }
            }
        }
    return true;
}

uint32_t aim_solutions(const aim_solver *s, int32_t target)
{
    const aim_val *p = &s->vals[s->at[15]];
    for (int i = 0; i < s->n[15]; i++)
        if (feq(p[i].v, (aim_frac){target, 1}))
            return p[i].count;
    return 0;
}

/* an RPN program for v from subset m: tokens are a tile's index (0-3) or an operator + 4 */
static int find(const aim_solver *s, unsigned m, aim_frac v, uint8_t *out)
{
    if (bits(m) == 1) {
        for (int i = 0; i < 4; i++)
            if (m >> i & 1) {
                out[0] = (uint8_t)i;
                return 1;
            }
    }
    for (unsigned l = (m - 1) & m; l; l = (l - 1) & m) {
        unsigned r = m ^ l;
        for (int i = 0; i < s->n[l]; i++)
            for (int j = 0; j < s->n[r]; j++) {
                aim_frac a = s->vals[s->at[l] + i].v, b = s->vals[s->at[r] + j].v;
                for (enum aim_key op = AIM_PLUS; op <= AIM_DIVIDE; op++) {
                    aim_frac w;
                    if (!apply(op, a, b, &w) || !feq(w, v))
                        continue;
                    int n = find(s, l, a, out);
                    n += find(s, r, b, out + n);
                    out[n++] = (uint8_t)(4 + op - AIM_PLUS);
                    return n;
                }
            }
    }
    return 0;
}

int aim_show(const aim_solver *s, const uint8_t tiles[4], int32_t target, enum aim_key keys[AIM_SHOW_MAX])
{
    uint8_t prog[7];
    if (!aim_solutions(s, target))
        return 0;
    int n = find(s, 15, (aim_frac){target, 1}, prog), k = 0;
    for (int i = 0; i < n; i++) {
        if (prog[i] < 4) {
            keys[k++] = (enum aim_key)(AIM_K0 + tiles[prog[i]]);
            if (i + 1 < n && prog[i + 1] < 4)
                keys[k++] = AIM_ENTER;
        } else {
            keys[k++] = (enum aim_key)(AIM_PLUS + prog[i] - 4);
        }
    }
    return k;
}

/* ---- the generator ---- */

static uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

int aim_level_hi(int level)
{
    return 30 + 10 * level;
}

uint32_t aim_level_cap(int level)
{
    static const uint32_t cap[AIM_LEVELS] = {0, 24, 12, 6, 3};
    return cap[level];
}

void aim_make_round(aim_solver *s, uint64_t seed, uint32_t n, int level, uint8_t tiles[4], int32_t *target)
{
    int32_t cand[61];
    uint64_t x = mix64(seed ^ mix64(n));
    for (;;) {
        for (int i = 0; i < 4; i++) {
            x = mix64(x);
            tiles[i] = (uint8_t)(1 + x % 9);
        }
        if (!aim_solve(s, tiles))
            continue;
        int hi = aim_level_hi(level), nc = 0;
        uint32_t cap = aim_level_cap(level);
        for (int pass = 0; pass < 2 && !nc; pass++)
            for (int32_t t = 10; t <= hi; t++) {
                uint32_t c = aim_solutions(s, t);
                if (c && (pass || !cap || c <= cap))
                    cand[nc++] = t;
            }
        if (nc) {
            x = mix64(x);
            *target = cand[x % (uint64_t)nc];
            return;
        }
    }
}
