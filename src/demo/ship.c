#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "demo/demo.h"

// The player's ship (focus/game.md). Built in code from triangles: nose toward -z (the way an unturned camera
// looks), y up, 1.8 m long, 1.8 m wide.
//
// How driving works:
//   0. ship_update_aim, once per browser frame: mouse movement moves the aim, a direction the ship should point
//      (ShipAimTuning). Two marks on screen: where the nose points, and the aim.
//   1. ship_read_input, every physics step: how far the nose is off the aim, and keys -> ShipInput. Mapping here.
//   2. ship_step: ShipInput + ShipTuning -> forces and turning. Change the flight model here.
//   3. ship_camera_step: the chase camera follows. Its numbers are in ShipCameraTuning.
// Every number that shapes the feel is in ship_tuning and ship_camera_tuning, and on the panel as sliders.

// What the pilot asks for this step. Each axis -1..1, turning at the ShipTuning turn speeds.
// Keys, the mouse aim and a gamepad stick later all fill it the same way.
typedef struct {
    real thrust; // 1 = full forward, -1 = full reverse
    real pitch;  // 1 = nose up
    real yaw;    // 1 = nose right
    real roll;   // 1 = right wing down
} ShipInput;

// How the ship drives. The defaults below are the starting point, the panel changes them live.
typedef struct {
    real mass;           // kg. with thrust, sets the acceleration: thrust / mass
    real thrust;         // N, forward
    real reverse_thrust; // N, backward
    real drag;           // share of speed lost per second. 0 = real space, it drifts forever.
                         // top speed is where drag cancels thrust: (thrust / mass) / drag
    real pitch_speed;    // rad/s at full input
    real yaw_speed;      // rad/s
    real roll_speed;     // rad/s
    real turn_response;  // per second, how fast turning catches up with the input. high = snappy, low = heavy
} ShipTuning;

static ShipTuning ship_tuning = {
    .mass = 1000,
    .thrust = 20000,         // 20 m/s^2
    .reverse_thrust = 10000, // 10 m/s^2
    .drag = 0.5f,            // top speed 40 m/s
    .pitch_speed = 1.5f,
    .yaw_speed = 1.0f,
    .roll_speed = 2.5f,
    .turn_response = 6,
};

// Mouse aim: the mouse moves an aim (a direction), and the ship turns toward it at its own turn speeds.
// The further the nose is off the aim, the harder it turns, full turn speed from full_turn_angle on.
// Lock the mouse (click the canvas) to fly with it.
typedef struct {
    real sensitivity;     // radians the aim moves per pixel of mouse movement
    real full_turn_angle; // radians off the aim that ask for full turn speed. less = sharper, more = gentler
    real limit;           // radians: the aim stays at most this far from the nose
    i32 invert_pitch;     // 0 = mouse up lifts the nose, 1 = mouse up dives (like a flight stick)
} ShipAimTuning;

static ShipAimTuning ship_aim_tuning = {
    .sensitivity = 0.002f,
    .full_turn_angle = 0.6f, // about 35 degrees
    .limit = 1.0f,           // about 57 degrees
    .invert_pitch = 0,
};

// The chase camera: behind and above the ship, in the ship's own axes, catching up with it smoothly.
typedef struct {
    real distance;   // m behind the ship
    real height;     // m above it
    real look_ahead; // m in front of the ship that the camera looks at
    real follow;     // per second, how fast the camera catches up. high = stiff, low = loose and trailing
} ShipCameraTuning;

static ShipCameraTuning ship_camera_tuning = {
    .distance = 6,
    .height = 1.8f,
    .look_ahead = 4,
    .follow = 5,
};

typedef struct {
    Particle particle;          // position, velocity, mass. forces go through particle_add_force
    Vector3 previous_position;  // from the step before, for blending
    Quaternion rotation;
    Quaternion previous_rotation;
    Vector3 turn;               // turn speed now, rad/s in the ship's own axes: x pitch, y yaw, z roll
} Ship;

typedef struct {
    Vector3 eye, previous_eye;
    Vector3 up, previous_up;    // follows the ship's up, so a roll tilts the view
} ShipCamera;

static Ship ship;
static ShipCamera ship_camera;
static Vector3 ship_aim; // unit direction, world axes: where the pilot wants the nose
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

static Vector3 ship_forward(void) { return quaternion_rotated_vector(ship.rotation, vector3_create(0, 0, -1)); }
static Vector3 ship_up(void) { return quaternion_rotated_vector(ship.rotation, vector3_create(0, 1, 0)); }

// where the chase camera wants to be: behind and above, in the ship's axes
static Vector3 ship_camera_goal(void) {
    Vector3 offset = vector3_create(0, ship_camera_tuning.height, ship_camera_tuning.distance);
    return vector3_added(ship.particle.position, quaternion_rotated_vector(ship.rotation, offset));
}

// The panel: what changes how it flies, and the camera. Mass is left out (only thrust / mass matters, so thrust
// covers it) and so is the aim (ShipAimTuning): change those in code.
static void ship_add_sliders(void) {
    plat_ui_group("flight");
    plat_ui_slider_log("thrust N", 1000, 200000, 100, &ship_tuning.thrust);
    plat_ui_slider_log("reverse N", 100, 200000, 100, &ship_tuning.reverse_thrust);
    plat_ui_slider("drag /s", 0, 3, 0.01f, &ship_tuning.drag);
    plat_ui_slider("pitch rad/s", 0.1f, 6, 0.05f, &ship_tuning.pitch_speed);
    plat_ui_slider("yaw rad/s", 0.1f, 6, 0.05f, &ship_tuning.yaw_speed);
    plat_ui_slider("roll rad/s", 0.1f, 8, 0.05f, &ship_tuning.roll_speed);
    plat_ui_slider_log("turn response /s", 0.5f, 50, 0.1f, &ship_tuning.turn_response);
    plat_ui_group("camera");
    plat_ui_slider("distance m", 1, 30, 0.1f, &ship_camera_tuning.distance);
    plat_ui_slider("height m", -5, 10, 0.1f, &ship_camera_tuning.height);
    plat_ui_slider("look ahead m", 0, 30, 0.1f, &ship_camera_tuning.look_ahead);
    plat_ui_slider_log("follow /s", 0.5f, 50, 0.1f, &ship_camera_tuning.follow);
}

static void ship_init(Vector3 position) {
    if (ship_model < 0) ship_build_model();
    ship.particle.position = ship.previous_position = position;
    ship.particle.velocity = vector3_zero();
    ship.particle.acceleration = vector3_zero();
    ship.particle.force_total = vector3_zero();
    ship.rotation = ship.previous_rotation = quaternion_identity();
    ship.turn = vector3_zero();
    ship_aim = ship_forward();
    ship_camera.eye = ship_camera.previous_eye = ship_camera_goal();
    ship_camera.up = ship_camera.previous_up = ship_up();
    ship_add_sliders();
}

// Once per browser frame. The mouse turns the aim around the camera's up and right, so it moves the way the mouse
// does on screen. While the mouse is free or the free camera is on, the aim sits on the nose: no turning.
static void ship_update_aim(void) {
    Vector3 forward = ship_forward();
    if (!plat_mouse_locked() || demo_free_camera) {
        ship_aim = forward;
        return;
    }
    Vector3 up = ship_camera.up;
    vector3_normalize(&up);
    Vector3 right = vector3_vector_product(ship_aim, up);
    vector3_normalize(&right);
    real sensitivity = ship_aim_tuning.sensitivity;
    real pitch_sign = ship_aim_tuning.invert_pitch ? 1 : -1;
    Quaternion turn = quaternion_multiplied(quaternion_from_axis_angle(up, -plat_mouse_dx() * sensitivity),
                                            quaternion_from_axis_angle(right, pitch_sign * plat_mouse_dy() * sensitivity));
    ship_aim = quaternion_rotated_vector(turn, ship_aim);
    vector3_normalize(&ship_aim);

    // keep it within limit of the nose: same side, pulled in to the edge
    real limit = ship_aim_tuning.limit;
    real along = vector3_scalar_product(ship_aim, forward);
    if (along < f32_cos(limit)) {
        Vector3 side = vector3_subtracted(ship_aim, vector3_multiplied(forward, along));
        if (vector3_square_magnitude(side) < 1e-8f) side = up; // straight behind: pick a side
        vector3_normalize(&side);
        ship_aim = vector3_added(vector3_multiplied(forward, f32_cos(limit)), vector3_multiplied(side, f32_sin(limit)));
    }
}

static real ship_clamp_unit(real value) { return value > 1 ? 1 : value < -1 ? -1 : value; }

// Mapping: nose toward the aim (pitch, yaw), W/S thrust, A/D yaw, Q/E roll. Nothing while the free camera is on.
static ShipInput ship_read_input(void) {
    ShipInput input = {0};
    if (demo_free_camera) return input;
    // the aim in the ship's own axes, then how far off it is up/down and left/right
    Vector3 right = quaternion_rotated_vector(ship.rotation, vector3_create(1, 0, 0));
    real aim_right = vector3_scalar_product(ship_aim, right);
    real aim_up = vector3_scalar_product(ship_aim, ship_up());
    real aim_ahead = vector3_scalar_product(ship_aim, ship_forward());
    input.pitch = f32_atan2(aim_up, aim_ahead) / ship_aim_tuning.full_turn_angle;
    input.yaw = f32_atan2(aim_right, aim_ahead) / ship_aim_tuning.full_turn_angle;
    if (plat_key_down(KEY_W)) input.thrust += 1;
    if (plat_key_down(KEY_S)) input.thrust -= 1;
    if (plat_key_down(KEY_D)) input.yaw += 1;
    if (plat_key_down(KEY_A)) input.yaw -= 1;
    if (plat_key_down(KEY_E)) input.roll += 1;
    if (plat_key_down(KEY_Q)) input.roll -= 1;
    input.pitch = ship_clamp_unit(input.pitch);
    input.yaw = ship_clamp_unit(input.yaw);
    return input;
}

// the two marks, far out along the nose and the aim: a cross where the nose points, a dot for the aim
#define SHIP_MARK_DISTANCE 200.0f // m
static void ship_draw_marks(f32 alpha) {
    if (demo_free_camera || !plat_mouse_locked()) return;
    Vector3 position = vector3_lerp(ship.previous_position, ship.particle.position, alpha);
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    Vector3 nose = vector3_added(position, quaternion_rotated_vector(rotation, vector3_create(0, 0, -SHIP_MARK_DISTANCE)));
    f32 x, y;
    if (render_world_to_screen(nose, &x, &y)) {
        f32 arm = 10, width = 2; // a cross with a gap in the middle
        render_draw_rect(x - arm, y - width / 2, arm - 3, width, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x + 3, y - width / 2, arm - 3, width, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x - width / 2, y - arm, width, arm - 3, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x - width / 2, y + 3, width, arm - 3, 0.9f, 0.95f, 1, 0.8f);
    }
    if (render_world_to_screen(vector3_added(position, vector3_multiplied(ship_aim, SHIP_MARK_DISTANCE)), &x, &y)) {
        render_draw_circle(x, y, 4, 1, 0.75f, 0.3f, 0.95f);
    }
}

// a hint while the mouse is free
static void ship_draw_hint(void) {
    if (demo_free_camera || plat_mouse_locked()) return;
    const char *hint = "click to fly with the mouse";
    f32 scale = 2; // glyphs are 8 px
    f32 x = (f32)plat_screen_width() / 2 - str_len(hint) * 4 * scale, y = (f32)plat_screen_height() * 0.75f;
    render_draw_text(hint, x, y, scale, 0.6f, 0.6f, 0.7f);
}

// The flight model: thrust along the nose, drag, and turn speeds that ease toward what the input asks for.
FORCE_INLINE void ship_step(ShipInput input, real time_step) {
    ShipTuning *tuning = &ship_tuning;
    ship.previous_position = ship.particle.position;
    ship.previous_rotation = ship.rotation;

    // turning, in the ship's own axes. a positive turn around x lifts the nose (nose is -z),
    // around y swings it left, around z lifts the right wing. so yaw and roll flip sign
    Vector3 wanted = vector3_create(input.pitch * tuning->pitch_speed, -input.yaw * tuning->yaw_speed,
                                    -input.roll * tuning->roll_speed);
    real catch_up = tuning->turn_response * time_step; // share of the gap closed this step
    if (catch_up > 1) catch_up = 1;
    vector3_add_scaled(&ship.turn, vector3_subtracted(wanted, ship.turn), catch_up);
    quaternion_add_rotation(&ship.rotation, quaternion_rotated_vector(ship.rotation, ship.turn), time_step);

    // thrust along the nose. drag as damping: the share of velocity kept this step
    real thrust = input.thrust > 0 ? tuning->thrust : tuning->reverse_thrust;
    particle_set_mass(&ship.particle, tuning->mass);
    particle_add_force(&ship.particle, vector3_multiplied(ship_forward(), input.thrust * thrust));
    real kept = 1 - tuning->drag * time_step;
    ship.particle.damping = kept > 0 ? kept : 0;
    particle_integrate(&ship.particle, time_step);
}

// after ship_step: the camera closes part of the gap to where it wants to be
FORCE_INLINE void ship_camera_step(real time_step) {
    ship_camera.previous_eye = ship_camera.eye;
    ship_camera.previous_up = ship_camera.up;
    real catch_up = ship_camera_tuning.follow * time_step;
    if (catch_up > 1) catch_up = 1;
    vector3_add_scaled(&ship_camera.eye, vector3_subtracted(ship_camera_goal(), ship_camera.eye), catch_up);
    vector3_add_scaled(&ship_camera.up, vector3_subtracted(ship_up(), ship_camera.up), catch_up);
}

static Vector3 ship_drawn_position(f32 alpha) { return vector3_lerp(ship.previous_position, ship.particle.position, alpha); }

FORCE_INLINE void ship_draw(f32 alpha) {
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    render_draw_model(ship_model, ship_drawn_position(alpha), rotation, vector3_create(1, 1, 1), 0.85f, 0.87f, 0.92f);
}

// the chase view for this frame, blended between steps like the ship
static void ship_camera_view(f32 alpha, DemoView *view) {
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    Vector3 ahead = quaternion_rotated_vector(rotation, vector3_create(0, 0, -ship_camera_tuning.look_ahead));
    view->eye = vector3_lerp(ship_camera.previous_eye, ship_camera.eye, alpha);
    view->target = vector3_added(ship_drawn_position(alpha), ahead);
    view->up = vector3_lerp(ship_camera.previous_up, ship_camera.up, alpha);
}
