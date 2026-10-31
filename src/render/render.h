#ifndef RENDER_H
#define RENDER_H

#include "base/base.h"

void render_init(void);
void render_begin(f32 r, f32 g, f32 b);
void render_end(void);

// screen pixels, y down. quads are drawn in the order they are added
void render_draw_rect(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void render_draw_circle(f32 cx, f32 cy, f32 radius, f32 r, f32 g, f32 b, f32 a);
void render_draw_text(const char *s, f32 x, f32 y, f32 scale, f32 r, f32 g, f32 b); // x, y = top left
void render_draw_text_centered(const char *s, f32 scale, f32 r, f32 g, f32 b);

// 3D, world meters, y up. Drawn with depth, before the 2D quads, so 2D stays on top.
void render_set_camera(M4 view_projection, V3 eye); // call once per frame, after render_begin
void render_draw_sphere(V3 center, f32 radius, f32 r, f32 g, f32 b);
void render_draw_box(V3 center, V3 half_size, f32 r, f32 g, f32 b); // axis aligned for now, rotation comes with rigid bodies
void render_draw_line(V3 from, V3 to, f32 r, f32 g, f32 b);         // 1 pixel wide (WebGPU lines)

#endif
