#include "base/base.h"
#include "render/render.h"
#include "demo/demo.h"

// The player's ship (focus/game.md). Built in code from triangles: nose toward -z (the way an unturned camera
// looks), y up, 1.8 m long, 1.8 m wide. For now it only spins in place, to check the model and its rotation.

typedef struct {
    Vector3 position;
    Vector3 previous_position;   // from the step before, for blending
    Quaternion rotation;
    Quaternion previous_rotation;
    Vector3 angular_velocity;    // radians per second, world axes
} Ship;

static Ship ship;
static RenderModel ship_model = -1; // built once, kept across demo switches

// a wedge: nose, two wing tips, a low ridge on top and a keel below, all meeting at the back
static void ship_build_model(void) {
    Vector3 nose = {0, 0, -1.2f};
    Vector3 left = {-0.9f, 0, 0.6f};
    Vector3 right = {0.9f, 0, 0.6f};
    Vector3 top = {0, 0.3f, 0.5f};
    Vector3 bottom = {0, -0.2f, 0.6f};
    Vector3 corners[] = {
        nose, left, top,      nose, top, right,     // upper sides
        nose, bottom, left,   nose, right, bottom,  // lower sides
        left, top, right,     left, right, bottom,  // back
    };
    ship_model = render_create_model(corners, (i32)(sizeof(corners) / sizeof(corners[0])) / 3);
}

static void ship_init(Vector3 position) {
    if (ship_model < 0) ship_build_model();
    ship.position = ship.previous_position = position;
    ship.rotation = ship.previous_rotation = quaternion_identity();
    ship.angular_velocity = vector3_create(0, 0.6f, 0);
}

FORCE_INLINE void ship_step(real time_step) {
    ship.previous_position = ship.position;
    ship.previous_rotation = ship.rotation;
    quaternion_add_rotation(&ship.rotation, ship.angular_velocity, time_step);
}

FORCE_INLINE void ship_draw(f32 alpha) {
    Vector3 position = vector3_lerp(ship.previous_position, ship.position, alpha);
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    render_draw_model(ship_model, position, rotation, vector3_create(1, 1, 1), 0.85f, 0.87f, 0.92f);
}
