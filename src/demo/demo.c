#include "base/base.h"
#include "demo/demo.h"

i32 demo_free_camera;

// the demo files are included before this one (unity build), so their statics are visible here
Demo demo_list[DEMO_COUNT] = {
    // behind the ship and a bit above, looking at it. title: the ship's grey
    [DEMO_HUNTER2] = {"hunter2", "hunter2", "", 0xd9deeb, {0, 1.5f, 5.0f}, 0, -0.28f, 1},
    // in front of the box, a bit above, looking at its center
    [DEMO_BALLS] = {"balls", "Erasmos Balls", "3", 0x44ff44, {BALLS_BOX_W / 2, 7.0f, 16.0f}, 0, -0.15f, 0},
    // in front of the cloud, a bit above, looking at the origin. title: the orange of the big bodies
    [DEMO_CLOUD] = {"cloud", "Gravity Cloud", "5", 0xffa040, {0, 4.0f, 22.0f}, 0, -0.18f, 0},
    // in front of the bridge, a bit above the deck, looking at its middle. title: the wood of the deck
    [DEMO_BRIDGE] = {"bridge", "Rope Bridge", "6", 0xd9a066, {0, 5.5f, 13.0f}, 0, -0.12f, 0},
    // like the balls box. title: the waterline blue
    [DEMO_POOL] = {"pool", "Ball Pool", "6", 0x66b8ff, {POOL_BOX_W / 2, 7.0f, 16.0f}, 0, -0.15f, 0},
};

void demo_init(i32 demo) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_init(); break;
    case DEMO_BALLS: balls_init(); break;
    case DEMO_CLOUD: cloud_init(); break;
    case DEMO_BRIDGE: bridge_init(); break;
    case DEMO_POOL: pool_init(); break;
    }
}

void demo_update(i32 demo, f32 seconds) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_update(seconds); break;
    case DEMO_BALLS: balls_update(); break;
    case DEMO_CLOUD: cloud_update(); break;
    case DEMO_BRIDGE: bridge_update(); break;
    case DEMO_POOL: pool_update(); break;
    }
    (void)seconds;
}

FORCE_INLINE void demo_step(i32 demo, real time_step) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_step(time_step); break;
    case DEMO_BALLS: balls_step(time_step); break;
    case DEMO_CLOUD: cloud_step(time_step); break;
    case DEMO_BRIDGE: bridge_step(time_step); break;
    case DEMO_POOL: pool_step(time_step); break;
    }
}

FORCE_INLINE void demo_draw(i32 demo, f32 alpha, f32 seconds, f32 hud_top) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_draw(alpha, seconds, hud_top); break;
    case DEMO_BALLS: balls_draw(alpha, seconds, hud_top); break;
    case DEMO_CLOUD: cloud_draw(alpha, seconds, hud_top); break;
    case DEMO_BRIDGE: bridge_draw(alpha, seconds, hud_top); break;
    case DEMO_POOL: pool_draw(alpha, seconds, hud_top); break;
    }
}

i32 demo_view(i32 demo, f32 alpha, DemoView *view) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_camera_view(alpha, view); return 1;
    }
    return 0;
}

void demo_stats(i32 demo, char *line, i32 cap) {
    switch (demo) {
    case DEMO_HUNTER2: hunter2_stats(line, cap); break;
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
