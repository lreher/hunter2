#include "base/base.h"
#include "demo/demo.h"

// the demo files are included before this one (unity build), so their statics are visible here
Demo demo_list[DEMO_COUNT] = {
    // in front of the box, a bit above, looking at its center
    [DEMO_BALLS] = {"balls", "Erasmos Balls", "3", {BALLS_BOX_W / 2, 7.0f, 16.0f}, 0, -0.15f},
    // in front of the cloud, a bit above, looking at the origin
    [DEMO_CLOUD] = {"cloud", "Gravity Cloud", "5", {0, 4.0f, 22.0f}, 0, -0.18f},
    // in front of the bridge, a bit above the deck, looking at its middle
    [DEMO_BRIDGE] = {"bridge", "Rope Bridge", "6", {0, 5.5f, 13.0f}, 0, -0.12f},
    // like the balls box
    [DEMO_POOL] = {"pool", "Ball Pool", "6", {POOL_BOX_W / 2, 7.0f, 16.0f}, 0, -0.15f},
};

void demo_init(i32 demo) {
    switch (demo) {
    case DEMO_BALLS: balls_init(); break;
    case DEMO_CLOUD: cloud_init(); break;
    case DEMO_BRIDGE: bridge_init(); break;
    case DEMO_POOL: pool_init(); break;
    }
}

void demo_update(i32 demo) {
    switch (demo) {
    case DEMO_BALLS: balls_update(); break;
    case DEMO_CLOUD: cloud_update(); break;
    case DEMO_BRIDGE: bridge_update(); break;
    case DEMO_POOL: pool_update(); break;
    }
}

FORCE_INLINE void demo_step(i32 demo, real dt) {
    switch (demo) {
    case DEMO_BALLS: balls_step(dt); break;
    case DEMO_CLOUD: cloud_step(dt); break;
    case DEMO_BRIDGE: bridge_step(dt); break;
    case DEMO_POOL: pool_step(dt); break;
    }
}

FORCE_INLINE void demo_draw(i32 demo, f32 alpha, f32 seconds, f32 hud_top) {
    switch (demo) {
    case DEMO_BALLS: balls_draw(alpha, seconds, hud_top); break;
    case DEMO_CLOUD: cloud_draw(alpha, seconds, hud_top); break;
    case DEMO_BRIDGE: bridge_draw(alpha, seconds, hud_top); break;
    case DEMO_POOL: pool_draw(alpha, seconds, hud_top); break;
    }
}

void demo_stats(i32 demo, char *line, i32 cap) {
    switch (demo) {
    case DEMO_BALLS: balls_stats(line, cap); break;
    case DEMO_CLOUD: cloud_stats(line, cap); break;
    case DEMO_BRIDGE: bridge_stats(line, cap); break;
    case DEMO_POOL: pool_stats(line, cap); break;
    }
}

void demo_append_stat(char *line, i32 cap, const char *label, i32 value) {
    str_append(line, cap, " | ");
    str_append(line, cap, label);
    str_append(line, cap, " ");
    str_append_i32(line, cap, value);
}
