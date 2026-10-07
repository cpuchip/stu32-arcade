/* SPDX-License-Identifier: MIT */
/* draw: drawing for STU-32 games on the LS027 frame (apps.h's ls027_frame: 400 x 240, pixel x of
 * row y in bit x % 8 of byte x / 8, set = dark). Everything clips to the frame; coordinates may be
 * negative or past the edge. No allocation, no stack frame above a few hundred bytes. */
#ifndef STU32_DRAW_H
#define STU32_DRAW_H

#include <stdint.h>

#include "ls027.h"

/* A shade: the byte for even rows in the low half, for odd rows in the high half; bit x % 8 set
 * means pixel x is dark. */
typedef uint16_t draw_shade;
#define DRAW_LIGHT ((draw_shade)0x0000)
#define DRAW_DARK ((draw_shade)0xffff)
#define DRAW_HALF ((draw_shade)0x55aa)      /* a checker */
#define DRAW_QUARTER ((draw_shade)0x0011)   /* one pixel in four dark: even rows, every 4th pixel */
#define DRAW_THREE_QUARTERS ((draw_shade)0x55ff)

enum draw_mode {
    DRAW_OR,    /* the sprite's set bits darken; the rest is left */
    DRAW_SET,   /* the sprite's bits are copied: set dark, clear light */
    DRAW_XOR,   /* the sprite's set bits invert */
    DRAW_CLEAR, /* the sprite's set bits lighten */
};

void draw_clear(ls027_frame *f);
void draw_pixel(ls027_frame *f, int x, int y, int dark);
/* Rows y..y+h-1, columns x..x+w-1, each pixel set as the shade says (light ones cleared). */
void draw_fill(ls027_frame *f, int x, int y, int w, int h, draw_shade shade);
void draw_frame_rect(ls027_frame *f, int x, int y, int w, int h, int dark); /* the outline */
/* Each pixel the one nearest the true line on the minor axis (a tie away from the start); ends are
 * clamped to +-2^29; at most 400 steps however far the ends lie. */
void draw_line(ls027_frame *f, int x0, int y0, int x1, int y1, int dark);
/* A 1-bit sprite: h rows of (w + 7) / 8 bytes, pixel x of a row in bit x % 8 of byte x / 8. */
void draw_blit(ls027_frame *f, int x, int y, const uint8_t *bits, int w, int h, enum draw_mode mode);
/* Text in the firmware's 5 x 7 face, scale times, 6 * scale pixels a character; returns the x after.
 * Characters the face lacks are left blank. */
int draw_text(ls027_frame *f, int x, int y, int scale, const char *s, int dark);
int draw_text_width(const char *s, int scale);

#endif
