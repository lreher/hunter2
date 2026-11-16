#ifndef RENDER_H
#define RENDER_H

#include "base/base.h"

void render_init(void);
void render_begin(f32 seconds, f32 r, f32 g, f32 b); // seconds: since last frame, moves the particles
void render_end(void);

// screen pixels, y down. quads are drawn in the order they are added
void render_draw_rect(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void render_draw_circle(f32 cx, f32 cy, f32 radius, f32 r, f32 g, f32 b, f32 a);
void render_draw_text(const char *s, f32 x, f32 y, f32 scale, f32 r, f32 g, f32 b); // x, y = top left
void render_draw_text_centered(const char *s, f32 scale, f32 r, f32 g, f32 b);

// 3D, world meters, y up. Drawn with depth, before the 2D quads, so 2D stays on top.
void render_set_camera(Matrix4 view_projection, Vector3 eye); // call once per frame, after render_begin, before drawing
i32 render_world_to_screen(Vector3 point, f32 *x, f32 *y); // screen pixels (y down) under this frame's camera. 0 = behind it
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
// glow: 0 = lit by the sun and sky, 1 = drawn in its full color everywhere, no shading (a bullet). between: a mix
void render_draw_model(RenderModel model, Vector3 position, Quaternion rotation, Vector3 scale, f32 r, f32 g, f32 b,
                       f32 glow);
// a soft light around center, brightest there, gone at radius (m). added on top of what is behind it (a halo)
void render_draw_glow(Vector3 center, f32 radius, f32 r, f32 g, f32 b);
void render_draw_line(Vector3 from, Vector3 to, f32 r, f32 g, f32 b);         // 1 pixel wide (WebGPU lines)

// Stars on the sky (render/stars.c): infinitely far, they turn with the camera but never get closer. Look only.
// f32 fields so panel sliders can point at them. Call once per frame between render_begin and render_end
#define RENDER_MAX_STARS 100000
typedef struct {
    f32 count;
    f32 size;       // pixels across the brightest stars' core, faint ones are smaller
    f32 brightness; // 1 = the brightest stars at full color
    f32 twinkle;    // 0..1: how far each star's brightness flickers, 0 = steady
    f32 color[3];   // every star is this color, tinted a random amount toward one of the two tints
    f32 tint_one[3], tint_two[3];
} RenderStars;
void render_draw_stars(const RenderStars *stars);

// Particles: glowing sparks that live on the GPU, look only (render/particles.c). Keep a RenderEmitter, set its
// fields every frame and call render_emit once per frame. memory is render_emit's own: keep it, never set it.
// Two kinds:
//   world (local = 0): a spark is on its own once it is out, it keeps its speed and the emitter moves on (a trail).
//   local (local = 1): sparks live in the emitter's own space, so they move and turn with it (a flame stuck to a
//     nozzle). position and rotation say where that space is this frame. velocity is not used.
typedef struct {
    Vector3 previous_position; // where it was last frame: new world sparks are spread along the way from there
    f32 carry;                 // part of a particle left over from last frame
    i32 started;
    i32 anchor;                // local: which anchor (the emitter's space on the GPU) it has, 0 = none yet
} RenderEmitterMemory;
typedef struct {
    i32 local;                 // 1 = sparks move and turn with the emitter, 0 = world
    Vector3 position;          // world, where particles come out now
    Quaternion rotation;       // local: the emitter's turn, its space turns with it
    Vector3 direction;         // world, unit, the way they shoot out
    Vector3 velocity;          // world: m/s every particle gets on top (what it comes out of, like the ship)
    f32 rate;                  // particles per second
    f32 speed, speed_spread;   // m/s along the direction, give or take speed_spread
    f32 cone;                  // radians, half the angle of the cone they spread in
    f32 cone_middle;           // 0 = even over the cone, 1 = densest in the middle, thinning to none at the edge (per type)
    f32 lifetime, lifetime_spread; // seconds, give or take lifetime_spread
    f32 size;                  // m, half the width of the glow
    Vector3 color_start, color_end; // 0..1, the color at birth and at death. it fades out on the way
    i32 type;                  // 0..15, which emitter type it is. its sparks remember it for their whole life
    f32 drag;                  // share of speed lost per second, for every live spark of this type
    f32 stretch;               // seconds: a moving spark of this type is drawn as a streak as long as the way it goes in this time
    f32 burst;                 // > 0: this many sparks now, instead of rate (a one off burst, give it fresh memory)
    RenderEmitterMemory memory;
} RenderEmitter;
// between render_begin and render_end. up to 16 per frame. local emitters: up to 64 in all (their anchors are never
// given back), past that they act as world emitters. drag and stretch apply to all sparks of its type (older ones
// too), so emit every frame even at rate 0, and give emitters of one type the same drag and stretch.
// returns 0 when its new sparks did not fit this frame (16 emitters already), 1 otherwise
i32 render_emit(RenderEmitter *emitter);

// For every particle. f32 so a panel slider can point at it
typedef struct {
    f32 capacity; // slots in the ring, up to 1048576. must be more than rate * lifetime, or old sparks vanish early
} RenderParticleTuning;
extern RenderParticleTuning render_particle_tuning;
i32 render_particle_estimate(void); // particles alive, about (rate * lifetime of this frame's emitters, at most capacity)

#endif
