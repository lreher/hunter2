#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"
#include "demo/demo.h"

// hunter2: the game (focus/game.md). For now the ship, its chase camera and a debug grid to see it move.
// Click the canvas to lock the mouse, then the mouse moves the aim and the ship turns toward it, W/S thrust, A/D side thrust, Q/E roll, Shift/Control thrust up/down, Space boost, right mouse button drift, Esc lets the mouse go
// (mapping in ship_read_input). Backspace: free fly camera and back.

#define HUNTER2_GRID_SPACING 10.0f // m
#define HUNTER2_GRID_LINES 41      // per direction (400 m across)
#define HUNTER2_GRID_HEIGHT -5.0f  // m, y of the grid plane

static i32 hunter2_grid = 1; // panel: debug grid on or off

static void hunter2_init(void) {
    ship_init(vector3_zero());
    plat_ui_group("debug");
    plat_ui_choice("grid", "Off|On", &hunter2_grid);
}

static void hunter2_update(f32 seconds) {
    (void)seconds;
    ship_update_aim();
}

FORCE_INLINE void hunter2_step(real time_step) {
    ship_step(ship_read_input(), time_step);
    ship_camera_step(time_step);
}

// a square of lines under the ship, moved in whole cells so it seems to go on forever
static void hunter2_draw_grid(Vector3 center) {
    real half = HUNTER2_GRID_SPACING * (HUNTER2_GRID_LINES - 1) / 2;
    real start_x = __builtin_floorf(center.x / HUNTER2_GRID_SPACING) * HUNTER2_GRID_SPACING - half;
    real start_z = __builtin_floorf(center.z / HUNTER2_GRID_SPACING) * HUNTER2_GRID_SPACING - half;
    for (i32 i = 0; i < HUNTER2_GRID_LINES; i++) {
        real x = start_x + i * HUNTER2_GRID_SPACING, z = start_z + i * HUNTER2_GRID_SPACING;
        render_draw_line(vector3_create(x, HUNTER2_GRID_HEIGHT, start_z), vector3_create(x, HUNTER2_GRID_HEIGHT, start_z + 2 * half), 0.2f, 0.25f, 0.35f);
        render_draw_line(vector3_create(start_x, HUNTER2_GRID_HEIGHT, z), vector3_create(start_x + 2 * half, HUNTER2_GRID_HEIGHT, z), 0.2f, 0.25f, 0.35f);
    }
}

FORCE_INLINE void hunter2_draw(f32 alpha, f32 seconds, f32 hud_top) {
    (void)seconds;
    Vector3 position = ship_drawn_position(alpha);
    real reach = 1.5f; // sun shadows around the ship
    Vector3 around = vector3_create(reach, reach, reach);
    render_set_shadow_area(vector3_subtracted(position, around), vector3_added(position, around));
    if (hunter2_grid) hunter2_draw_grid(position);
    ship_draw(alpha);
    ship_draw_exhaust(alpha);
    ship_draw_hint();
    ship_draw_marks(alpha);

    char line[128];
    line[0] = 0;
    str_append(line, sizeof(line), "speed m/s: ");
    str_append_i32(line, sizeof(line), (i32)(vector3_magnitude(ship.particle.velocity) + 0.5f));
    if (!demo_free_camera && plat_key_down(KEY_MOUSE_RIGHT)) str_append(line, sizeof(line), "  drift");
    else if (!demo_free_camera && plat_key_down(KEY_SPACE) && plat_key_down(KEY_W)) str_append(line, sizeof(line), "  boost");
    str_append(line, sizeof(line), "  thrust N ");
    str_append_i32(line, sizeof(line), (i32)ship.thrust.z);
    str_append(line, sizeof(line), "  particles ~");
    str_append_i32(line, sizeof(line), render_particle_estimate());
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
}

static void hunter2_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "speed", (i32)vector3_magnitude(ship.particle.velocity));
}
