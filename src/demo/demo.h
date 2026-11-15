#ifndef DEMO_H
#define DEMO_H

#include "base/base.h"

// The game (hunter2) first, then one demo per thing we built from the book, kept so we can go back to it.
// Each runs on the current engine.
// A demo owns its state and panel sliders. game/ runs the loop, the camera and the shared HUD.
typedef struct {
    const char *id;      // for ?scene= in the URL
    const char *name;
    const char *chapter; // book chapter, shown in the demo bar. "" for the game
    u32 title_color;     // 0xRRGGBB, picked to match the demo
    Vector3 camera_position;  // start view
    f32 camera_yaw;      // 0 looks down -z, positive turns right
    f32 camera_pitch;    // 0 is level, positive looks up
    i32 own_camera;      // 1 = demo_view places the camera. Backspace switches to the free camera and back
} Demo;

// the game, then the demos in book order
enum { DEMO_HUNTER2, DEMO_BALLS, DEMO_CLOUD, DEMO_BRIDGE, DEMO_POOL, DEMO_COUNT };
extern Demo demo_list[DEMO_COUNT];

// set by the game every frame: 1 while the free camera has the keys and mouse, so the demo leaves them alone
extern i32 demo_free_camera;
#define DEMO_FAR 400.0f // m, how far the camera draws unless a demo asks for more
extern f32 demo_far;    // m, set in a demo's init (hunter2: its planets are kilometers apart)

// A camera placed by the demo, like a chase camera. The game uses it unless the free fly camera is on
typedef struct {
    Vector3 eye, target, up;
} DemoView;

// Calls into a demo. Plain switches, not function pointers, so step and draw can be forced inline (see FORCE_INLINE)
void demo_init(i32 demo);   // panel sliders, first spawn
void demo_update(i32 demo, f32 seconds); // once per browser frame, before the steps: keys, mouse, slider changes. seconds: this frame
FORCE_INLINE void demo_step(i32 demo, real time_step);                             // one fixed physics step
FORCE_INLINE void demo_draw(i32 demo, f32 alpha, f32 seconds, f32 hud_top); // scene and HUD lines. alpha: 0 = last step, 1 = next one
i32 demo_view(i32 demo, f32 alpha, DemoView *view); // 1 = the demo placed the camera this frame. 0 = the game's free camera
void demo_stats(i32 demo, char *line, i32 cap); // once per second: append to the stats line, reset counters

void demo_append_stat(char *line, i32 cap, const char *label, i32 value); // " | label value"

#endif
