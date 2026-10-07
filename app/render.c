/* SPDX-License-Identifier: MIT */
/* Babal's picture (render.h). The camera sits BACK rows behind the ball and follows it sideways,
 * so the ball is always at the centre column. The rules wrap the columns round (an unrolled
 * cylinder); one turn is drawn, the w columns centred on the ball, so the track has edges as the
 * 1995 original's did, and steering past one brings the other side round. */
#include <string.h>

#include "font5x7.h"
#include "render.h"

#define W LS027_W
#define H LS027_H
#define HORIZON 40          /* the track is drawn below this line, the HUD above it */
#define K 200               /* depth: a scanline y shows rows K / (y - HORIZON) ahead of the camera */
#define ZMAX 30             /* rows further than this are not drawn */
#define SEAM_Z 14           /* row seams are drawn only nearer than this (far ones alias) */
#define COL_SHIFT 6         /* 64 px per column at one row's depth */
#define BACK (2 << 16)      /* the camera is 2 rows behind the ball (Q16) */
#define CX (W / 2)
#define BALL_Y (HORIZON + K / 2)   /* the ball's depth, BACK, falls on this scanline */
#define BALL_R 11
#define PX_PER_UNIT 32      /* at the ball's depth: one world unit (a column, a jump unit) */

static inline void put(ls027_frame *f, int x, int y, int dark)
{
    if (x < 0 || x >= W || y < 0 || y >= H)
        return;
    uint8_t m = (uint8_t)(1u << (x & 7));
    if (dark)
        f->px[y][x >> 3] |= m;
    else
        f->px[y][x >> 3] &= (uint8_t)~m;
}

/* A kind's shade on scanline y as a byte: bit (x % 8) set where pixel x is dark. Every pattern has
 * period 2 in x, so one byte serves the whole line: floor solid (its seam every other pixel),
 * boost a 50% checker, overdrive 75%, void light. */
static uint8_t shade(bb_slab_kind k, int y, int seam)
{
    switch (k) {
    case BB_KIND_FLOOR: return seam ? 0x55 : 0xff;
    case BB_KIND_BOOST: return (y & 1) ? 0x55 : 0xaa;
    case BB_KIND_OVERDRIVE: return (y & 1) ? 0x55 : 0xff;
    default: return 0;
    }
}

/* floor(a / b) and ceil(a / b) for b > 0 */
static inline int32_t floor_div(int32_t a, int32_t b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}
static inline int32_t ceil_div(int32_t a, int32_t b)
{
    return -floor_div(-a, b);
}

/* Pixels [x0, x1) of line y take the shade byte m (the line was cleared). */
static void span(ls027_frame *f, int y, int x0, int x1, uint8_t m)
{
    if (!m || x0 >= x1)
        return;
    uint8_t *row = f->px[y];
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    uint8_t first = (uint8_t)(0xff << (x0 & 7)), last = (uint8_t)(0xff >> (7 - ((x1 - 1) & 7)));
    if (b0 == b1) {
        row[b0] |= m & first & last;
        return;
    }
    row[b0] |= m & first;
    for (int b = b0 + 1; b < b1; b++)
        row[b] |= m;
    row[b1] |= m & last;
}

/* Each scanline at one depth z: pixel x is at column offset off(x) = floor((x - CX) * z / 64) from
 * the ball (Q16), its column (ball + off + 1/2) >> 16 taken mod w, drawn while -w/2 <= off < w/2
 * (one turn). off rises with x, so the line is a few spans: each ends where off reaches the next
 * column's boundary or the turn's edge, found by a ceiling division. 32-bit throughout: |x - CX|
 * <= 200 and z <= ZMAX rows, so (x - CX) * z stays under 2^31. */
static void track(ls027_frame *f, const bb_level *lvl, const bb_player *p)
{
    int w = (int)bb_level_width(lvl);
    /* the model in Q16: the ball's distance in rows, its column (level_col's x * w / 500) */
    int64_t dist_q = (int64_t)(bb_player_dist(p) * 65536.0);
    int32_t ball_u = (int32_t)(int64_t)(p->position_vec.x * (double)w / 500.0 * 65536.0);
    bool boost = bb_player_is_boosting(p);
    int32_t half = (int32_t)w << 15;
    intptr_t prev_row = INTPTR_MIN, kinds_row = INTPTR_MIN;
    bb_slab_kind kinds[BB_WIDTH_MAX];
    for (int y = H - 1; y > HORIZON; y--) {
        int32_t z = (int32_t)(((int64_t)K << 16) / (y - HORIZON));
        if (z > (ZMAX << 16))
            break;
        intptr_t row = (intptr_t)((dist_q - BACK + z + 0x8000) >> 16);
        int seam = row != prev_row && prev_row != INTPTR_MIN && z < (SEAM_Z << 16);
        prev_row = row;
        if (row != kinds_row) {
            for (int c = 0; c < w; c++)
                kinds[c] = bb_level_slab_kind(lvl, c, row, boost);
            kinds_row = row;
        }
        int x = 0;
        while (x < W) {
            int32_t off = ((int32_t)(x - CX) * z) >> COL_SHIFT;
            if (off < -half) { /* left of the turn: light up to its edge */
                x = CX + ceil_div(-half * (1 << COL_SHIFT), z);
                continue;
            }
            if (off >= half)
                break; /* right of the turn: light to the end */
            int32_t r = (ball_u + off + 0x8000) >> 16;
            int32_t next_c = CX + ceil_div(((r + 1) * 65536 - 0x8000 - ball_u) * (1 << COL_SHIFT), z);
            int32_t next_v = CX + ceil_div(half * (1 << COL_SHIFT), z);
            int end = next_c < next_v ? next_c : next_v;
            if (end > W)
                end = W;
            if (end <= x)
                end = x + 1;
            int32_t c = r % w;
            if (c < 0)
                c += w;
            span(f, y, x, end, shade(kinds[c], y, seam));
            x = end;
        }
    }
}

static void ball(ls027_frame *f, const bb_player *p)
{
    int lift = (int)(p->position_vec.y * PX_PER_UNIT / 100.0);
    int cy = BALL_Y - BALL_R - lift;
    for (int dy = -BALL_R; dy <= BALL_R; dy++)
        for (int dx = -BALL_R; dx <= BALL_R; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > BALL_R * BALL_R)
                continue;
            int x = CX + dx, y = cy + dy;
            int rim = d2 > (BALL_R - 1) * (BALL_R - 1);
            put(f, x, y, rim || ((x + y) & 1));
        }
}

static const font_glyph *glyph(uint32_t cp)
{
    for (int i = 0; i < FONT5X7_COUNT; i++)
        if (FONT5X7[i].cp == cp)
            return &FONT5X7[i];
    return NULL;
}

int babal_text(ls027_frame *f, int x, int y, int scale, const char *s)
{
    for (; *s; s++) {
        const font_glyph *g = glyph((uint8_t)*s);
        if (g)
            for (int r = 0; r < 7; r++)
                for (int c = 0; c < 5; c++)
                    if (g->rows[r] & (0x10 >> c))
                        for (int sy = 0; sy < scale; sy++)
                            for (int sx = 0; sx < scale; sx++)
                                put(f, x + c * scale + sx, y + r * scale + sy, 1);
        x += 6 * scale;
    }
    return x;
}

static void number(char *out, const char *label, int32_t v)
{
    char digits[12];
    int n = 0;
    uint32_t u = v < 0 ? 0 : (uint32_t)v;
    do {
        digits[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (*label)
        *out++ = *label++;
    while (n)
        *out++ = digits[--n];
    *out = '\0';
}

void babal_render(ls027_frame *f, const bb_level *lvl, const bb_player *p, const babal_hud *hud)
{
    memset(f, 0, sizeof *f);
    track(f, lvl, p);
    ball(f, p);
    char text[32];
    number(text, "", hud->score);
    babal_text(f, 6, 6, 2, text);
    if (hud->best > 0) {
        number(text, "BEST ", hud->best);
        babal_text(f, W - 6 - (int)strlen(text) * 12, 6, 2, text);
    }
    int32_t prog = hud->progress < 0 ? 0 : hud->progress > 1000 ? 1000 : hud->progress;
    int bar = (W - 12) * prog / 1000;
    for (int x = 6; x < W - 6; x++) {
        put(f, x, 26, 1);
        put(f, x, 31, 1);
        for (int y = 27; y < 31; y++)
            put(f, x, y, x - 6 < bar);
    }
    put(f, 6, 27, 1), put(f, 6, 28, 1), put(f, 6, 29, 1), put(f, 6, 30, 1);
    put(f, W - 7, 27, 1), put(f, W - 7, 28, 1), put(f, W - 7, 29, 1), put(f, W - 7, 30, 1);
    if (hud->message) {
        int len = (int)strlen(hud->message);
        int x = (W - len * 24) / 2, y = 90;
        for (int yy = y - 6; yy < y + 34; yy++)
            for (int xx = x - 8; xx < x + len * 24 + 4; xx++)
                put(f, xx, yy, 0);
        babal_text(f, x, y, 4, hud->message);
    }
}
