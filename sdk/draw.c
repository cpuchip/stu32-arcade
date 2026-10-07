/* SPDX-License-Identifier: MIT */
/* draw (draw.h). */
#include <string.h>

#include "draw.h"
#include "font5x7.h"

void draw_clear(ls027_frame *f)
{
    memset(f, 0, sizeof *f);
}

void draw_pixel(ls027_frame *f, int x, int y, int dark)
{
    if (x < 0 || x >= LS027_W || y < 0 || y >= LS027_H)
        return;
    uint8_t m = (uint8_t)(1u << (x & 7));
    if (dark)
        f->px[y][x >> 3] |= m;
    else
        f->px[y][x >> 3] &= (uint8_t)~m;
}

/* Pixels [x0, x1) of row y take shade byte m (both set and cleared). */
static void span(ls027_frame *f, int y, int x0, int x1, uint8_t m)
{
    uint8_t *row = f->px[y];
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    uint8_t first = (uint8_t)(0xff << (x0 & 7)), last = (uint8_t)(0xff >> (7 - ((x1 - 1) & 7)));
    if (b0 == b1) {
        uint8_t k = first & last;
        row[b0] = (uint8_t)((row[b0] & ~k) | (m & k));
        return;
    }
    row[b0] = (uint8_t)((row[b0] & ~first) | (m & first));
    for (int b = b0 + 1; b < b1; b++)
        row[b] = m;
    row[b1] = (uint8_t)((row[b1] & ~last) | (m & last));
}

void draw_fill(ls027_frame *f, int x, int y, int w, int h, draw_shade shade)
{
    if (w <= 0 || h <= 0)
        return;
    long x0 = x, y0 = y, x1 = (long)x + w, y1 = (long)y + h;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > LS027_W)
        x1 = LS027_W;
    if (y1 > LS027_H)
        y1 = LS027_H;
    for (long r = y0; r < y1 && x0 < x1; r++)
        span(f, (int)r, (int)x0, (int)x1, (uint8_t)((r & 1) ? shade >> 8 : shade & 0xff));
}

void draw_frame_rect(ls027_frame *f, int x, int y, int w, int h, int dark)
{
    if (w <= 0 || h <= 0)
        return;
    draw_shade s = dark ? DRAW_DARK : DRAW_LIGHT;
    draw_fill(f, x, y, w, 1, s);
    draw_fill(f, x, y + h - 1, w, 1, s);
    draw_fill(f, x, y, 1, h, s);
    draw_fill(f, x + w - 1, y, 1, h, s);
}

#define LINE_LIMIT (1L << 29) /* ends are clamped to this, so 2 * k * |d| fits in 64 bits */

static long clamp_end(int v)
{
    return v < -LINE_LIMIT ? -LINE_LIMIT : v > LINE_LIMIT ? LINE_LIMIT : v;
}

/* Along the major axis, step k (0..|major|) puts the pixel nearest the true line on the minor axis,
 * a tie going away from the start: minor = start + s * floor((2k|minor| + |major|) / (2|major|)).
 * Only the steps that land on the frame are walked, so a line costs at most 400 steps however far
 * its ends lie. */
void draw_line(ls027_frame *f, int x0, int y0, int x1, int y1, int dark)
{
    long ax = clamp_end(x0), ay = clamp_end(y0), bx = clamp_end(x1), by = clamp_end(y1);
    long adx = bx > ax ? bx - ax : ax - bx, ady = by > ay ? by - ay : ay - by;
    long sx = bx >= ax ? 1 : -1, sy = by >= ay ? 1 : -1;
    int xmajor = adx >= ady;
    long major = xmajor ? adx : ady, minor = xmajor ? ady : adx;
    long m0 = xmajor ? ax : ay, n0 = xmajor ? ay : ax, sm = xmajor ? sx : sy, sn = xmajor ? sy : sx;
    long size = xmajor ? LS027_W : LS027_H;
    /* the steps k for which m0 + sm * k lies in [0, size) */
    long klo = sm > 0 ? -m0 : m0 - (size - 1), khi = sm > 0 ? size - 1 - m0 : m0;
    if (klo < 0)
        klo = 0;
    if (khi > major)
        khi = major;
    for (long k = klo; k <= khi; k++) {
        long n = major ? n0 + sn * ((2 * k * minor + major) / (2 * major)) : n0;
        long m = m0 + sm * k;
        if (xmajor)
            draw_pixel(f, (int)m, n < 0 || n >= LS027_H ? -1 : (int)n, dark);
        else
            draw_pixel(f, n < 0 || n >= LS027_W ? -1 : (int)n, (int)m, dark);
    }
}

void draw_blit(ls027_frame *f, int x, int y, const uint8_t *bits, int w, int h, enum draw_mode mode)
{
    int stride = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
        long py = (long)y + r;
        if (py < 0 || py >= LS027_H)
            continue;
        for (int c = 0; c < w; c++) {
            long px = (long)x + c;
            if (px < 0 || px >= LS027_W)
                continue;
            int on = (bits[r * stride + c / 8] >> (c % 8)) & 1;
            uint8_t m = (uint8_t)(1u << (px & 7));
            uint8_t *b = &f->px[py][px >> 3];
            switch (mode) {
            case DRAW_OR: if (on) *b |= m; break;
            case DRAW_SET: *b = on ? (uint8_t)(*b | m) : (uint8_t)(*b & ~m); break;
            case DRAW_XOR: if (on) *b ^= m; break;
            case DRAW_CLEAR: if (on) *b &= (uint8_t)~m; break;
            }
        }
    }
}

static const font_glyph *glyph(uint32_t cp)
{
    for (int i = 0; i < FONT5X7_COUNT; i++)
        if (FONT5X7[i].cp == cp)
            return &FONT5X7[i];
    return NULL;
}

int draw_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 6 * scale;
}

int draw_text(ls027_frame *f, int x, int y, int scale, const char *s, int dark)
{
    for (; *s; s++) {
        const font_glyph *g = glyph((uint8_t)*s);
        if (g)
            for (int r = 0; r < 7; r++)
                for (int c = 0; c < 5; c++)
                    if (g->rows[r] & (0x10 >> c))
                        draw_fill(f, x + c * scale, y + r * scale, scale, scale, dark ? DRAW_DARK : DRAW_LIGHT);
        x += 6 * scale;
    }
    return x;
}
