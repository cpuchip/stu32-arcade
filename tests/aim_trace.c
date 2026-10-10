/* SPDX-License-Identifier: MIT */
/* Aim's logic on the host, printing what the oracle prints (oracle/aim):
 *   aim_trace trace SCRIPT    the state after every key (oracle/aim/rules.py's form)
 *   aim_trace reach LO HI     every set of four digits' reachable targets and counts (oracle/aim/reach.py's form)
 *   aim_trace rounds SEEDS N  the rounds aim_make_round makes: seeds 0..SEEDS-1, each level, round numbers 0..N-1
 *   aim_trace show LO HI      a SHOW key sequence for every set of four digits and reachable target in LO..HI
 *   aim_trace appscript SEED N   an SDK runner script (sdk/runner/apprun.c) for aim_app with -seed SEED and no
 *                             save: the title's ENTER, then N rounds, each won by typing its SHOW keys, ENTER between
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aim_logic.h"
#include "app_keys.h"

static const char *const NAMES[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                                    "ENTER", "PLUS", "MINUS", "TIMES", "DIVIDE", "SWAP", "BACK"};

static aim_solver solver; /* about 15 KB: off the stack, as in the game's ctx */

static void print_frac(aim_frac v)
{
    if (v.d == 1)
        printf("%d", (int)v.n);
    else
        printf("%d/%d", (int)v.n, (int)v.d);
}

static int trace(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 1;
    char line[128];
    aim_round r;
    memset(&r, 0, sizeof r);
    while (fgets(line, sizeof line, f)) {
        int a, b, c, d, t;
        char name[16];
        if (sscanf(line, "round %d %d %d %d %d", &a, &b, &c, &d, &t) == 5) {
            uint8_t tiles[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
            aim_round_init(&r, tiles, t);
        } else if (sscanf(line, "key %15s", name) == 1) {
            int k = 0;
            while (k <= AIM_BACK && strcmp(NAMES[k], name))
                k++;
            if (k > AIM_BACK)
                return 2;
            bool beep = aim_round_key(&r, (enum aim_key)k);
            printf("%s beep=%d entry=%s stack=", name, beep, r.entry);
            for (int i = 0; i < r.sp; i++) {
                if (i)
                    putchar(',');
                print_frac(r.stack[i]);
            }
            printf(" used=");
            for (int i = 0; i < 4; i++)
                putchar(r.used >> i & 1 ? '1' : '0');
            printf(" won=%d\n", r.won);
        }
    }
    fclose(f);
    return 0;
}

/* every multiset of four digits, ascending, as reach.py walks them */
#define EACH_SET(t)                                                                                            \
    for (int a = 1; a <= 9; a++)                                                                               \
        for (int b = a; b <= 9; b++)                                                                           \
            for (int c = b; c <= 9; c++)                                                                       \
                for (int d = c; d <= 9; d++)                                                                   \
                    for (uint8_t t[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d}, once = 1; once; once = 0)

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "trace"))
        return trace(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "reach")) {
        int lo = atoi(argv[2]), hi = atoi(argv[3]);
        EACH_SET(t)
        {
            if (!aim_solve(&solver, t))
                return 3;
            printf("%d %d %d %d:", t[0], t[1], t[2], t[3]);
            for (int v = lo; v <= hi; v++) {
                uint32_t n = aim_solutions(&solver, v);
                if (n)
                    printf(" %d/%u", v, (unsigned)n);
            }
            putchar('\n');
        }
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "rounds")) {
        int seeds = atoi(argv[2]), n = atoi(argv[3]);
        for (int seed = 0; seed < seeds; seed++)
            for (int level = 0; level < AIM_LEVELS; level++)
                for (int i = 0; i < n; i++) {
                    uint8_t t[4];
                    int32_t target;
                    aim_make_round(&solver, (uint64_t)seed, (uint32_t)i, level, t, &target);
                    printf("%d %d %d: %d %d %d %d %d\n", seed, level, i, t[0], t[1], t[2], t[3], (int)target);
                }
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "show")) {
        int lo = atoi(argv[2]), hi = atoi(argv[3]);
        EACH_SET(t)
        {
            if (!aim_solve(&solver, t))
                return 3;
            for (int v = lo; v <= hi; v++) {
                enum aim_key keys[AIM_SHOW_MAX];
                int k = aim_show(&solver, t, v, keys);
                if (!k)
                    continue;
                printf("round %d %d %d %d %d\n", t[0], t[1], t[2], t[3], v);
                for (int i = 0; i < k; i++)
                    printf("key %s\n", NAMES[keys[i]]);
            }
        }
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "appscript")) {
        /* aim_app's own rounds: its seed (app_seed << 32, no save) and its level (the streak / 5, at most 4) */
        static const int app_key[] = {APP_KEY_0, APP_KEY_1, APP_KEY_2, APP_KEY_3, APP_KEY_4, APP_KEY_5, APP_KEY_6,
                                      APP_KEY_7, APP_KEY_8, APP_KEY_9, APP_KEY_ENTER, APP_KEY_PLUS, APP_KEY_MINUS,
                                      APP_KEY_TIMES, APP_KEY_DIVIDE, APP_KEY_SWAP, APP_KEY_BACK};
        uint64_t seed = (uint64_t)strtoul(argv[2], NULL, 0) << 32;
        int n = atoi(argv[3]);
        printf("# aim_trace appscript %s %d: the title, then %d rounds won by their SHOW keys\n", argv[2], n, n);
        /* a key line's ms is the app's clock (it may not run behind the last tick); a tick line adds its ms */
        unsigned clock = 0;
#define TICK(ms) (printf("tick %u\n", (unsigned)(ms)), clock += (ms))
#define PRESS(k) (printf("key %d 1 %u\nkey %d 0 %u\n", (k), clock + 5, (k), clock + 10), TICK(20))
        TICK(20);
        PRESS(APP_KEY_ENTER);
        for (int i = 0; i < n; i++) {
            int level = i / 5 < AIM_LEVELS - 1 ? i / 5 : AIM_LEVELS - 1;
            uint8_t t[4];
            int32_t target;
            enum aim_key keys[AIM_SHOW_MAX];
            aim_make_round(&solver, seed, (uint32_t)i, level, t, &target);
            int k = aim_show(&solver, t, target, keys);
            printf("# round %d: %d %d %d %d to %d\n", i, t[0], t[1], t[2], t[3], (int)target);
            TICK(100);
            for (int j = 0; j < k; j++)
                PRESS(app_key[keys[j]]);
            for (int j = 0; j < 7; j++)
                TICK(100);
            if (i + 1 < n)
                PRESS(APP_KEY_ENTER);
        }
#undef TICK
#undef PRESS
        return 0;
    }
    fprintf(stderr, "usage: aim_trace trace SCRIPT | reach LO HI | rounds SEEDS N | show LO HI | appscript SEED N\n");
    return 1;
}
