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
void render_set_camera(Matrix4 view_projection, Vector3 eye); // call once per frame, after render_begin, before drawing
void render_set_shadow_area(Vector3 min, Vector3 max);        // box that gets sun shadows. tighter = sharper shadows
void render_draw_sphere(Vector3 center, f32 radius, f32 r, f32 g, f32 b);
void render_draw_sphere_photo(Vector3 center, f32 radius, f32 r, f32 g, f32 b); // the photo faces the camera. color shows until it loads
void render_add_photo_picker(const char *name); // panel button: the user picks one photo for all spheres. call after render_init
void render_draw_box(Vector3 center, Vector3 half_size, f32 r, f32 g, f32 b); // axis aligned for now, rotation comes with rigid bodies
// Models: a shape made in code, one triangle = 3 corners in a row. Flat shaded, one color per draw.
// Each face is turned to point away from the shape's center, so corner order does not matter, but the shape must be
// convex (every face sees the center from behind). Call after render_init, keep the returned handle.
typedef i32 RenderModel;
RenderModel render_create_model(const Vector3 *corners, i32 triangle_count);
// scale: per axis, 1 = as built. rotation turns the model around its own origin, then it moves to position
void render_draw_model(RenderModel model, Vector3 position, Quaternion rotation, Vector3 scale, f32 r, f32 g, f32 b);
void render_draw_line(Vector3 from, Vector3 to, f32 r, f32 g, f32 b);         // 1 pixel wide (WebGPU lines)

#endif
