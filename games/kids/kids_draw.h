/* SPDX-License-Identifier: MIT */
/* The pictures 3720, MOLES and CATCH share so a player who doesn't read can play (docs/3720.md, "The screen"): a score
 * as tally marks folding into stars, a key drawn as a keycap with its own legend, a framed picture to choose a mode or a
 * level, and stars to count a level by.
 * Header-only and static, so each game builds its own copy and no symbol is shared between games. */
#ifndef KIDS_DRAW_H
#define KIDS_DRAW_H

#include <stdint.h>

#include "draw.h"

#define KIDS_STARS_MAX 30     /* the most stars a tally draws */
#define KIDS_STAR_PITCH 9
#define KIDS_GROUP_PITCH 12
#define KIDS_STROKE_PITCH 2

/* star: 7 x 7 */
static const uint8_t KIDS_STAR[] = {
    0x08,  /* ...#... */
    0x08,  /* ...#... */
    0x7f,  /* ####### */
    0x3e,  /* .#####. */
    0x1c,  /* ..###.. */
    0x36,  /* .##.##. */
    0x41,  /* #.....# */
};

/* the tally of n: stars of 25, groups of five, strokes; its width in px */
static inline int kids_tally_width(uint32_t n)
{
    uint32_t stars = n / 25, groups = n % 25 / 5, strokes = n % 5;
    if (stars > KIDS_STARS_MAX)
        stars = KIDS_STARS_MAX;
    return (int)(stars * KIDS_STAR_PITCH + groups * KIDS_GROUP_PITCH + strokes * KIDS_STROKE_PITCH);
}

/* the tally of n from x rightward, its strokes 8 px tall from row y */
static inline void kids_tally(ls027_frame *f, int x, int y, uint32_t n)
{
    uint32_t stars = n / 25, groups = n % 25 / 5, strokes = n % 5;
    if (stars > KIDS_STARS_MAX)
        stars = KIDS_STARS_MAX;
    for (uint32_t i = 0; i < stars; i++, x += KIDS_STAR_PITCH)
        draw_blit(f, x, y, KIDS_STAR, 7, 7, DRAW_OR);
    for (uint32_t i = 0; i < groups; i++, x += KIDS_GROUP_PITCH) {
        for (int s = 0; s < 4; s++)
            draw_line(f, x + 1 + 2 * s, y, x + 1 + 2 * s, y + 7, 1);
        draw_line(f, x, y + 6, x + 8, y + 1, 1);
    }
    for (uint32_t i = 0; i < strokes; i++, x += KIDS_STROKE_PITCH)
        draw_line(f, x + 1, y, x + 1, y + 7, 1);
}

/* n with no more stars than fit in w px, for a tally in a tight place (a row of stars reads as a lot) */
static inline uint32_t kids_tally_fit(uint32_t n, int w)
{
    uint32_t rest = n % 25, fit = (uint32_t)(w - kids_tally_width(rest)) / KIDS_STAR_PITCH;
    return n / 25 > fit ? fit * 25 + rest : n;
}

/* a key as the calculator prints it, framed, with x its left edge; returns the width */
static inline int kids_keycap(ls027_frame *f, int x, int y, const char *legend, int dark)
{
    int w = draw_text_width(legend, 1) + 7;
    if (!dark)
        return w;
    draw_frame_rect(f, x, y, w, 13, 1);
    draw_line(f, x + 1, y + 13, x + w - 1, y + 13, 1);   /* a shadow under it, so it reads as a key */
    draw_text(f, x + 4, y + 3, 1, legend, 1);
    return w;
}

/* a picture's frame: thick when it is the chosen one */
static inline void kids_choice(ls027_frame *f, int x, int y, int w, int h, int chosen)
{
    draw_frame_rect(f, x, y, w, h, 1);
    if (chosen) {
        draw_frame_rect(f, x - 1, y - 1, w + 2, h + 2, 1);
        draw_frame_rect(f, x - 2, y - 2, w + 4, h + 4, 1);
    }
}

/* n small stars centred on cx from row y, under a level's picture, so the level can be counted */
static inline void kids_marks(ls027_frame *f, int cx, int y, int n)
{
    for (int i = 0, x = cx - (n * KIDS_STAR_PITCH - 2) / 2; i < n; i++, x += KIDS_STAR_PITCH)
        draw_blit(f, x, y, KIDS_STAR, 7, 7, DRAW_OR);
}

/* small arrows for a turn or a move */
static inline void kids_arrow(ls027_frame *f, int x, int y, int right)
{
    int d = right ? 1 : -1;
    draw_line(f, x - 4 * d, y, x + 4 * d, y, 1);
    draw_line(f, x + 4 * d, y, x + 1 * d, y - 3, 1);
    draw_line(f, x + 4 * d, y, x + 1 * d, y + 3, 1);
}

#endif
