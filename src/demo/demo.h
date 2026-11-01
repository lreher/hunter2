#ifndef DEMO_H
#define DEMO_H

#include "base/base.h"

// One demo per thing we build from the book, kept so we can go back to it. Each runs on the current engine.
// A demo owns its state and panel sliders. game/ runs the loop, the camera and the shared HUD.
typedef struct {
    const char *id;      // for ?scene= in the URL
    const char *name;
    const char *chapter; // book chapter, shown in the demo bar
    u32 title_color;     // 0xRRGGBB, picked to match the demo
    V3 camera_position;  // start view
    f32 camera_yaw;      // 0 looks down -z, positive turns right
    f32 camera_pitch;    // 0 is level, positive looks up
} Demo;

// in book order
enum { DEMO_BALLS, DEMO_CLOUD, DEMO_BRIDGE, DEMO_POOL, DEMO_COUNT };
extern Demo demo_list[DEMO_COUNT];

// Calls into a demo. Plain switches, not function pointers, so step and draw can be forced inline (see FORCE_INLINE)
void demo_init(i32 demo);   // panel sliders, first spawn
void demo_update(i32 demo); // once per browser frame, before the steps: keys, slider changes
FORCE_INLINE void demo_step(i32 demo, real dt);                             // one fixed physics step
FORCE_INLINE void demo_draw(i32 demo, f32 alpha, f32 seconds, f32 hud_top); // scene and HUD lines. alpha: 0 = last step, 1 = next one
void demo_stats(i32 demo, char *line, i32 cap); // once per second: append to the stats line, reset counters

void demo_append_stat(char *line, i32 cap, const char *label, i32 value); // " | label value"

#endif
