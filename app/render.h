/* Babal's picture: the level and the ball drawn in 1 bit on the LS027's 400 x 240 frame, as the
 * 1995 original read (a flat track in perspective, the ball a dithered disc), the HUD in our own
 * font. Integer fixed point throughout, so a frame is the same bits on the host and on the S3. */
#ifndef BABAL_RENDER_H
#define BABAL_RENDER_H

#include <stdint.h>

#include "babal.h"
#include "ls027.h"

typedef struct {
    int32_t score;
    int32_t best;          /* the best score kept in the app's save, 0 for none */
    int32_t progress;      /* time progress, 0..1000 */
    const char *message;   /* centred big text over the track (e.g. "FINISH"), or NULL */
} babal_hud;

void babal_render(ls027_frame *f, const bb_level *lvl, const bb_player *p, const babal_hud *hud);

/* Text in the firmware's 5 x 7 face, scaled; returns the x after it. */
int babal_text(ls027_frame *f, int x, int y, int scale, const char *s);

#endif
