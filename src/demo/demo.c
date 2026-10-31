#include "base/base.h"
#include "demo/demo.h"

// the demo files are included before this one (unity build), so their statics are visible here
Demo demo_list[DEMO_COUNT] = {
    // in front of the box, a bit above, looking at its center
    [DEMO_BALLS] = {"Erasmos Balls", "3", {BALLS_BOX_W / 2, 7.0f, 16.0f}, 0, -0.15f},
};

void demo_init(i32 demo) {
    switch (demo) {
    case DEMO_BALLS: balls_init(); break;
    }
}

void demo_update(i32 demo) {
    switch (demo) {
    case DEMO_BALLS: balls_update(); break;
    }
}

FORCE_INLINE void demo_step(i32 demo, real dt) {
    switch (demo) {
    case DEMO_BALLS: balls_step(dt); break;
    }
}

FORCE_INLINE void demo_draw(i32 demo, f32 alpha, f32 seconds, f32 hud_top) {
    switch (demo) {
    case DEMO_BALLS: balls_draw(alpha, seconds, hud_top); break;
    }
}

void demo_stats(i32 demo, char *line, i32 cap) {
    switch (demo) {
    case DEMO_BALLS: balls_stats(line, cap); break;
    }
}

void demo_append_stat(char *line, i32 cap, const char *label, i32 value) {
    str_append(line, cap, " | ");
    str_append(line, cap, label);
    str_append(line, cap, " ");
    str_append_i32(line, cap, value);
}
