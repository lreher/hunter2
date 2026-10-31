#ifndef RENDER_H
#define RENDER_H

#include "base/base.h"

void render_init(void);
void render_begin(f32 r, f32 g, f32 b);
void render_draw_text_centered(const char *s, f32 scale, f32 r, f32 g, f32 b);
void render_end(void);

// screen pixels, y down. quads are drawn in the order they are added
void render_draw_rect(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void render_draw_circle(f32 cx, f32 cy, f32 radius, f32 r, f32 g, f32 b, f32 a);

#endif
