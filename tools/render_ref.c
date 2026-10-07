/* The reference renderer: render.c as it was at c522381, drawing pixel by pixel (the definition
 * the span renderer was proven byte-identical to: 58,710 frames, and a pixel-by-pixel review probe
 * of 200,000). Symbols renamed; tools/apphost.c draws every frame with it too and compares. Never
 * edit it to follow render.c: a change of look is a change here first, then there. */
/* Babal's picture (render.h). The camera sits BACK rows behind the ball and follows it sideways,
 * so the ball is always at the centre column. The rules wrap the columns round (an unrolled
 * cylinder); one turn is drawn, the w columns centred on the ball, so the track has edges as the
 * HP 48's did, and steering past one brings the other side round. */
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

static int shade(bb_slab_kind k, int x, int y, int seam)
{
    switch (k) {
    case BB_KIND_FLOOR: return seam ? !(x & 1) : 1;
    case BB_KIND_BOOST: return (x ^ y) & 1;
    case BB_KIND_OVERDRIVE: return !((x & 1) && (y & 1));
    default: return 0;
    }
}

static void track(ls027_frame *f, const bb_level *lvl, const bb_player *p)
{
    int w = (int)bb_level_width(lvl);
    /* the model in Q16: the ball's distance in rows, its column (level_col's x * w / 500) */
    int64_t dist_q = (int64_t)(bb_player_dist(p) * 65536.0);
    int64_t ball_u_q = (int64_t)(p->position_vec.x * (double)w / 500.0 * 65536.0);
    bool boost = bb_player_is_boosting(p);
    intptr_t prev_row = INTPTR_MIN;
    for (int y = H - 1; y > HORIZON; y--) {
        int64_t z_q = ((int64_t)K << 16) / (y - HORIZON);
        if (z_q > ((int64_t)ZMAX << 16))
            break;
        int64_t s_q = dist_q - BACK + z_q;
        intptr_t row = (intptr_t)((s_q + 0x8000) >> 16);
        int seam = row != prev_row && prev_row != INTPTR_MIN && z_q < ((int64_t)SEAM_Z << 16);
        prev_row = row;
        bb_slab_kind kinds[BB_WIDTH_MAX];
        for (int c = 0; c < w; c++)
            kinds[c] = bb_level_slab_kind(lvl, c, row, boost);
        int64_t half_q = (int64_t)w << 15;
        for (int x = 0; x < W; x++) {
            int64_t off_q = ((int64_t)(x - CX) * z_q) >> COL_SHIFT;
            if (off_q < -half_q || off_q >= half_q) {
                put(f, x, y, 0);
                continue;
            }
            int64_t u_q = ball_u_q + off_q;
            int64_t c = ((u_q + 0x8000) >> 16) % w;
            if (c < 0)
                c += w;
            put(f, x, y, shade(kinds[c], x, y, seam));
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

int ref_text(ls027_frame *f, int x, int y, int scale, const char *s)
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

void ref_render(ls027_frame *f, const bb_level *lvl, const bb_player *p, const babal_hud *hud)
{
    memset(f, 0, sizeof *f);
    track(f, lvl, p);
    ball(f, p);
    char text[32];
    number(text, "", hud->score);
    ref_text(f, 6, 6, 2, text);
    if (hud->best > 0) {
        number(text, "BEST ", hud->best);
        ref_text(f, W - 6 - (int)strlen(text) * 12, 6, 2, text);
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
        ref_text(f, x, y, 4, hud->message);
    }
}
