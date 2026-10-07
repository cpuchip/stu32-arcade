/* SPDX-License-Identifier: MIT */
/* apprun: a game played on the host through apps.h's calls, in the framework's order (load, start,
 * then key or tick, a draw after every tick and after a key that asks, then save, stop), from a
 * script of keys and ticks. For a game author's tests: the frames as PBM files and hashes, the
 * tones, the save, and the framework's bounds checked (the ctx within APP_CTX_MAX, the name 1 to 5
 * characters, tick never asking for no more ticks while the game runs, keys never before the last
 * tick, save_size within APP_SAVE_MAX). Build it with the game:
 * cc ... -DAPP_DEF=my_app sdk/runner/apprun.c my_game.c firmware/draw.c ...
 * apps.h v1.2 as the framework's host build has it: app_seed() is fixed by -seed N (app_seed_set),
 * else the framework's host choice from the clock; app_tune is counted (-v prints it, past
 * APP_TUNE_MAX notes dropped); load gets up to save_size bytes and save's cap is save_size.
 *
 * Script: `key K D MS` (key K, row * 6 + column, down 1 or up 0, at MS on the game's clock) and
 * `tick MS` (MS since the last tick); `#` comments.
 * Usage: apprun SCRIPT [-pbm DIR] [-every N] [-hashes FILE] [-load SAVE] [-save SAVE] [-seed N] [-v] */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#ifndef APP_DEF
#error "build with -DAPP_DEF=<the game's app_def>"
#endif
extern const app_def APP_DEF;

static uint32_t clock_ms;
static int verbose;
static uint64_t tones;
void app_beep(uint16_t hz, uint16_t ms)
{
    tones++;
    if (verbose)
        printf("tone %u Hz %u ms at %u ms\n", hz, ms, clock_ms);
}
uint32_t app_now_ms(void)
{
    return clock_ms;
}
static uint64_t tunes;
void app_tune(const uint16_t *notes, uint8_t n)
{
    tunes++;
    if (!verbose)
        return;
    printf("tune of %u notes at %u ms:", n, clock_ms);
    for (int i = 0; i < n && i < APP_TUNE_MAX; i++)
        printf(" %u/%u", notes[2 * i], notes[2 * i + 1]);
    printf(n > APP_TUNE_MAX ? " (%d dropped)\n" : "\n", n - APP_TUNE_MAX);
}
static int seed_fixed;
static uint32_t seed_value;
void app_seed_set(uint32_t seed)
{
    seed_fixed = 1;
    seed_value = seed;
}
uint32_t app_seed(void)
{
    if (seed_fixed)
        return seed_value;
    uint32_t x = clock_ms ^ 0x9E3779B9u; /* the framework's host choice (firmware/app.c) */
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return x;
}

static ls027_frame frame;
static void pbm(const char *dir, uint64_t n)
{
    char path[512];
    snprintf(path, sizeof path, "%s/frame-%06" PRIu64 ".pbm", dir, n);
    FILE *o = fopen(path, "wb");
    if (!o) {
        perror(path);
        exit(1);
    }
    fprintf(o, "P4\n%d %d\n", LS027_W, LS027_H);
    for (int y = 0; y < LS027_H; y++)
        for (int x = 0; x < LS027_ROW_BYTES; x++) {
            uint8_t v = frame.px[y][x], r = 0; /* the panel's order is LSB first, PBM's MSB */
            for (int k = 0; k < 8; k++)
                r |= (uint8_t)(((v >> k) & 1) << (7 - k));
            fputc(r, o);
        }
    fclose(o);
}
static uint64_t hash(void)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t *b = &frame.px[0][0];
    for (size_t i = 0; i < sizeof frame.px; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

int main(int argc, char **argv)
{
    const char *script = NULL, *pbm_dir = NULL, *hashes = NULL, *load_path = NULL, *save_path = NULL;
    uint64_t every = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-pbm") && i + 1 < argc)
            pbm_dir = argv[++i];
        else if (!strcmp(argv[i], "-every") && i + 1 < argc)
            every = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "-hashes") && i + 1 < argc)
            hashes = argv[++i];
        else if (!strcmp(argv[i], "-load") && i + 1 < argc)
            load_path = argv[++i];
        else if (!strcmp(argv[i], "-save") && i + 1 < argc)
            save_path = argv[++i];
        else if (!strcmp(argv[i], "-seed") && i + 1 < argc)
            app_seed_set((uint32_t)strtoul(argv[++i], NULL, 0));
        else if (!strcmp(argv[i], "-v"))
            verbose = 1;
        else
            script = argv[i];
    }
    if (!script || !every) {
        fprintf(stderr, "usage: apprun SCRIPT [-pbm DIR] [-every N] [-hashes FILE] [-load SAVE] [-save SAVE] [-seed N] [-v]\n");
        return 2;
    }
    const app_def *a = &APP_DEF;
    size_t namelen = a->name ? strlen(a->name) : 0;
    uint32_t save_size = a->save_size ? a->save_size : APP_SAVE_SIZE;
    if (a->ctx_size > APP_CTX_MAX || namelen < 1 || namelen > 5 || save_size > APP_SAVE_MAX) {
        fprintf(stderr, "apprun: out of bounds: ctx %u (max %u), name \"%s\", save_size %u (max %u)\n", a->ctx_size,
                (unsigned)APP_CTX_MAX, a->name ? a->name : "", save_size, (unsigned)APP_SAVE_MAX);
        return 3;
    }
    void *ctx = calloc(1, a->ctx_size ? a->ctx_size : 1); /* zeroed, as the framework gives it */
    static uint8_t save[APP_SAVE_MAX], out[APP_SAVE_MAX];
    uint32_t n = 0;
    if (load_path) {
        FILE *s = fopen(load_path, "rb");
        if (!s) {
            perror(load_path);
            return 1;
        }
        n = (uint32_t)fread(save, 1, save_size, s);
        fclose(s);
    }
    FILE *in = fopen(script, "r"), *hf = hashes ? fopen(hashes, "w") : NULL;
    if (!in || (hashes && !hf)) {
        perror("apprun");
        return 1;
    }
    a->load(ctx, n ? save : NULL, n);
    a->start(ctx);
    char line[256];
    uint64_t ticks = 0, frames = 0, keys = 0;
    int stopped = 0, lineno = 0;
    while (fgets(line, sizeof line, in)) {
        lineno++;
        int k, d;
        unsigned ms;
        char *c = strchr(line, '#');
        if (c)
            *c = '\0';
        if (sscanf(line, "key %d %d %u", &k, &d, &ms) == 3) {
            if (ms < clock_ms) {
                fprintf(stderr, "apprun: %s:%d: a key at %u ms, before the last tick (%u ms)\n", script, lineno, ms, clock_ms);
                return 4;
            }
            if (k == 42)
                continue; /* C ends the game and is never delivered */
            keys++;
            if (a->key(ctx, k, d != 0, ms)) {
                a->draw(ctx, &frame);
                frames++;
            }
        } else if (sscanf(line, "tick %u", &ms) == 1) {
            if (stopped) {
                fprintf(stderr, "apprun: %s:%d: a tick after the game asked for none\n", script, lineno);
                return 5;
            }
            clock_ms += ms;
            uint32_t next = a->tick(ctx, ms);
            stopped = next == 0;
            a->draw(ctx, &frame);
            if (hf)
                fprintf(hf, "%016" PRIx64 "\n", hash());
            if (pbm_dir && ticks % every == 0)
                pbm(pbm_dir, ticks);
            frames++;
            ticks++;
        } else if (strspn(line, " \t\r\n") != strlen(line)) {
            fprintf(stderr, "apprun: %s:%d: not a key or a tick\n", script, lineno);
            return 2;
        }
    }
    a->save(ctx, out, save_size);
    a->stop(ctx);
    if (save_path) {
        FILE *s = fopen(save_path, "wb");
        if (!s || fwrite(out, 1, save_size, s) != save_size) {
            perror(save_path);
            return 1;
        }
        fclose(s);
    }
    printf("apprun %s: %" PRIu64 " ticks, %" PRIu64 " keys, %" PRIu64 " frames, %" PRIu64 " tones, %" PRIu64
           " tunes, %u ms; ctx %u bytes, save %u%s\n",
           a->name, ticks, keys, frames, tones, tunes, clock_ms, a->ctx_size, save_size,
           stopped ? "; tick asked for no more" : "");
    if (hf)
        fclose(hf);
    fclose(in);
    free(ctx);
    return 0;
}
