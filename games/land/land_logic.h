/* SPDX-License-Identifier: MIT */
/* Land's rules (docs/land.md): the physics, the contact test, the judge and the terrain generator, all integers.
 * No drawing and no framework calls, so the host checks drive it directly (oracle/land, tests/land_trace.c). */
#ifndef LAND_LOGIC_H
#define LAND_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define LAND_Q 65536          /* positions in 1/65,536 px, speeds in that a step */
#define LAND_STEP_MS 20
#define LAND_W 400
#define LAND_G 655
#define LAND_A 1500
#define LAND_VY 26214         /* the landing limits, down and across */
#define LAND_VX 13107
#define LAND_FUEL 2000
#define LAND_CRASH_COST 200
#define LAND_TURN_STEPS 4
#define LAND_START_Y 32
#define LAND_VERTS_MAX 48
#define LAND_PADS 3
#define LAND_OUTLINE 7

typedef struct {
    int16_t x, y; /* px */
} land_vert;

typedef struct {
    int16_t x0, x1, y; /* px, ends inclusive */
    uint8_t mult;
} land_pad;

typedef struct {
    uint8_t n;
    land_vert v[LAND_VERTS_MAX];
    land_pad pad[LAND_PADS];
    int32_t start_x, start_vx; /* Q */
} land_terrain;

enum { LAND_KEY_LEFT = 1, LAND_KEY_RIGHT = 2, LAND_KEY_THRUST = 4 };
enum land_end { LAND_FLYING, LAND_LANDED, LAND_CRASHED };

typedef struct {
    int32_t x, y, vx, vy; /* Q */
    uint8_t a;            /* 0-23, 15 degrees each, 0 upright, clockwise on the screen */
    uint8_t timer;        /* steps until the next turn */
    uint8_t held, pressed; /* LAND_KEY_* */
    int32_t fuel;
    bool fired;           /* the engine fired on the last step */
} land_ship;

extern const int16_t land_sin[24], land_cos[24];
extern const int8_t land_outline_px[LAND_OUTLINE][2];

void land_make_terrain(land_terrain *t, uint64_t seed, uint32_t round);
void land_ship_init(land_ship *s, const land_terrain *t, int32_t fuel);
void land_key(land_ship *s, int key, bool down);
/* one step; LAND_LANDED sets *mult to the pad's multiplier */
enum land_end land_step(land_ship *s, const land_terrain *t, int *mult);

int32_t land_ground(const land_terrain *t, int32_t xq);
void land_outline(const land_ship *s, int32_t pts[LAND_OUTLINE][2]);

#endif
