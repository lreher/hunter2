#include "base/base.h"
#include "render/render.h"
#include "demo/demo.h"

// hunter2: the game (focus/game.md). For now the ship alone, spinning in place.

static void hunter2_init(void) {
    ship_init(vector3_zero());
}

static void hunter2_update(void) {
}

FORCE_INLINE void hunter2_step(real time_step) {
    ship_step(time_step);
}

FORCE_INLINE void hunter2_draw(f32 alpha, f32 seconds, f32 hud_top) {
    (void)seconds;
    (void)hud_top;
    real reach = 1.5f; // sun shadows around the ship
    Vector3 around = vector3_create(reach, reach, reach);
    render_set_shadow_area(vector3_subtracted(ship.position, around), vector3_added(ship.position, around));
    ship_draw(alpha);
}

static void hunter2_stats(char *line, i32 cap) {
    (void)line;
    (void)cap;
}
