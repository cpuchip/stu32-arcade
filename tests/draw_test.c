/* SPDX-License-Identifier: MIT */
/* sdk/draw against a pixel-by-pixel reference written here, from random frames, with coordinates
 * far off the frame and sizes negative; lines judged by their properties (two correct line
 * algorithms may pick different pixels on a tie): both ends set, max(|dx|, |dy|) + 1 pixels when
 * the whole line is on the frame, each pixel within half a pixel of the true line on the minor
 * axis, and 8-connected. Fails when no check ran. Usage: draw_test [OPS] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "font5x7.h"

static int checks, failures;
#define CHECK(cond, ...)                                     \
    do {                                                     \
        checks++;                                            \
        if (!(cond)) {                                       \
            failures++;                                      \
            if (failures < 10) {                             \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);  \
                printf(__VA_ARGS__);                         \
                printf("\n");                                \
            }                                                \
        }                                                    \
    } while (0)

static uint64_t rs = 0x5eed;
static uint32_t rnd(void)
{
    rs = rs * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(rs >> 33);
}
static int between(int lo, int hi)
{
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

/* ---- the reference: one pixel at a time ---- */
static int get(const ls027_frame *f, int x, int y)
{
    return (f->px[y][x / 8] >> (x % 8)) & 1;
}
static void put(ls027_frame *f, long x, long y, int dark)
{
    if (x < 0 || x >= LS027_W || y < 0 || y >= LS027_H)
        return;
    if (dark)
        f->px[y][x / 8] |= (uint8_t)(1 << (x % 8));
    else
        f->px[y][x / 8] &= (uint8_t)~(1 << (x % 8));
}
static void ref_fill(ls027_frame *f, int x, int y, int w, int h, draw_shade s)
{
    for (long r = y; r < (long)y + h; r++)
        for (long c = x; c < (long)x + w; c++) {
            uint8_t byte = (uint8_t)((r & 1) ? s >> 8 : s & 0xff);
            put(f, c, r, (byte >> ((c % 8 + 8) % 8)) & 1);
        }
}
static void ref_blit(ls027_frame *f, int x, int y, const uint8_t *bits, int w, int h, enum draw_mode m)
{
    int stride = (w + 7) / 8;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            long px = (long)x + c, py = (long)y + r;
            if (px < 0 || px >= LS027_W || py < 0 || py >= LS027_H)
                continue;
            int on = (bits[r * stride + c / 8] >> (c % 8)) & 1, old = get(f, (int)px, (int)py);
            int now = m == DRAW_OR ? (old | on) : m == DRAW_SET ? on : m == DRAW_XOR ? (old ^ on) : (old & !on);
            put(f, px, py, now);
        }
}
static void ref_text(ls027_frame *f, int x, int y, int scale, const char *s, int dark)
{
    for (; *s; s++, x += 6 * scale)
        for (int i = 0; i < FONT5X7_COUNT; i++)
            if (FONT5X7[i].cp == (uint8_t)*s) {
                for (int r = 0; r < 7; r++)
                    for (int c = 0; c < 5; c++)
                        if (FONT5X7[i].rows[r] & (0x10 >> c))
                            for (int sy = 0; sy < scale; sy++)
                                for (int sx = 0; sx < scale; sx++)
                                    put(f, (long)x + c * scale + sx, (long)y + r * scale + sy, dark);
                break;
            }
}

static ls027_frame a, b;
static void randomize(void)
{
    for (size_t i = 0; i < sizeof a.px; i++)
        (&a.px[0][0])[i] = (uint8_t)rnd();
    b = a;
}
static int same(void)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static void line_props(int x0, int y0, int x1, int y1)
{
    static ls027_frame l;
    memset(&l, 0, sizeof l);
    draw_line(&l, x0, y0, x1, y1, 1);
    int n = 0;
    for (int y = 0; y < LS027_H; y++)
        for (int x = 0; x < LS027_W; x++)
            if (get(&l, x, y)) {
                n++;
                /* within half a pixel of the true line, measured on the minor axis */
                long dx = (long)x1 - x0, dy = (long)y1 - y0;
                if (labs(dx) >= labs(dy) && dx != 0) {
                    double ty = y0 + (double)dy * (x - x0) / (double)dx;
                    CHECK(ty - y <= 0.5 + 1e-9 && y - ty <= 0.5 + 1e-9, "line (%d,%d)-(%d,%d): pixel (%d,%d) off", x0, y0, x1, y1, x, y);
                } else if (dy != 0) {
                    double tx = x0 + (double)dx * (y - y0) / (double)dy;
                    CHECK(tx - x <= 0.5 + 1e-9 && x - tx <= 0.5 + 1e-9, "line (%d,%d)-(%d,%d): pixel (%d,%d) off", x0, y0, x1, y1, x, y);
                }
                /* 8-connected: a neighbour set, unless the line is one pixel */
                int nb = 0;
                for (int ey = -1; ey <= 1; ey++)
                    for (int ex = -1; ex <= 1; ex++)
                        if ((ex || ey) && x + ex >= 0 && x + ex < LS027_W && y + ey >= 0 && y + ey < LS027_H)
                            nb += get(&l, x + ex, y + ey);
                CHECK(nb > 0 || (x0 == x1 && y0 == y1) || x == 0 || x == LS027_W - 1 || y == 0 || y == LS027_H - 1,
                      "line (%d,%d)-(%d,%d): pixel (%d,%d) alone", x0, y0, x1, y1, x, y);
            }
    int on = x0 >= 0 && x0 < LS027_W && y0 >= 0 && y0 < LS027_H && x1 >= 0 && x1 < LS027_W && y1 >= 0 && y1 < LS027_H;
    if (on) {
        long want = labs((long)x1 - x0) > labs((long)y1 - y0) ? labs((long)x1 - x0) + 1 : labs((long)y1 - y0) + 1;
        CHECK(n == want, "line (%d,%d)-(%d,%d): %d pixels, want %ld", x0, y0, x1, y1, n, want);
        CHECK(get(&l, x0, y0) && get(&l, x1, y1), "line (%d,%d)-(%d,%d): an end unset", x0, y0, x1, y1);
    }
}

int main(int argc, char **argv)
{
    int ops = argc > 1 ? atoi(argv[1]) : 20000;
    static const draw_shade shades[] = {DRAW_LIGHT, DRAW_DARK, DRAW_HALF, DRAW_QUARTER, DRAW_THREE_QUARTERS, 0x1234};
    static uint8_t sprite[64 * 8];
    for (int i = 0; i < ops; i++) {
        randomize();
        int x = between(-500, 900), y = between(-400, 640), w = between(-5, 600), h = between(-5, 300);
        switch (rnd() % 5) {
        case 0: {
            draw_shade s = shades[rnd() % 6];
            draw_fill(&a, x, y, w, h, s);
            ref_fill(&b, x, y, w, h, s);
            CHECK(same(), "fill %d %d %d %d %04x", x, y, w, h, s);
            break;
        }
        case 1: {
            int d = (int)(rnd() & 1);
            draw_frame_rect(&a, x, y, w, h, d);
            if (w > 0 && h > 0) {
                draw_shade s = d ? DRAW_DARK : DRAW_LIGHT;
                ref_fill(&b, x, y, w, 1, s), ref_fill(&b, x, y + h - 1, w, 1, s);
                ref_fill(&b, x, y, 1, h, s), ref_fill(&b, x + w - 1, y, 1, h, s);
            }
            CHECK(same(), "frame_rect %d %d %d %d %d", x, y, w, h, d);
            break;
        }
        case 2: {
            int sw = between(1, 64), sh = between(1, 64);
            for (int k = 0; k < (int)sizeof sprite; k++)
                sprite[k] = (uint8_t)rnd();
            enum draw_mode m = (enum draw_mode)(rnd() % 4);
            int bx = between(-80, 420), by = between(-80, 260);
            draw_blit(&a, bx, by, sprite, sw, sh, m);
            ref_blit(&b, bx, by, sprite, sw, sh, m);
            CHECK(same(), "blit %d %d %dx%d mode %d", bx, by, sw, sh, m);
            break;
        }
        case 3: {
            char s[24];
            int n = between(0, 20);
            for (int k = 0; k < n; k++)
                s[k] = (char)between(32, 126);
            s[n] = '\0';
            int sc = between(1, 4), d = (int)(rnd() & 1), tx = between(-200, 420), ty = between(-60, 250);
            int ea = draw_text(&a, tx, ty, sc, s, d);
            ref_text(&b, tx, ty, sc, s, d);
            CHECK(same() && ea == tx + 6 * sc * n && draw_text_width(s, sc) == 6 * sc * n, "text at %d %d scale %d", tx, ty, sc);
            break;
        }
        default:
            line_props(between(-50, 450), between(-50, 290), between(-50, 450), between(-50, 290));
            break;
        }
    }
    /* far-off lines and fills must neither crash nor draw out of the frame (ASan watches) */
    line_props(-2000000000, -2000000000, 2000000000, 2000000000);
    draw_fill(&a, -2000000000, -2000000000, 2147483647, 2147483647, DRAW_HALF);
    if (checks == 0) {
        printf("no checks ran\n");
        return 1;
    }
    printf("draw_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
