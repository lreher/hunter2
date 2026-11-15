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
//   3. ship_camera_step: the chase camera's turning follows the ship's, it pulls back with speed. Its numbers are in ShipCameraTuning.
// Every number that shapes the feel is in ship_tuning and ship_camera_tuning, and on the panel as sliders.

// What the pilot asks for this step. Each axis -1..1, turning at the ShipTuning turn speeds.
// Keys, the mouse aim and a gamepad stick later all fill it the same way.
typedef struct {
    real thrust; // 1 = full forward, -1 = full reverse
    real side;   // 1 = full thrust to the right, -1 = to the left
    real lift;   // 1 = full thrust up (the ship's up), -1 = down
    real pitch;  // 1 = nose up
    real yaw;    // 1 = nose right
    real roll;   // 1 = right wing down
    i32 drift;   // 1 = drift: no thrust at all, less drag, the ship keeps its speed while it turns
    i32 boost;   // 1 = boost: forward thrust up by ShipTuning.boost %
} ShipInput;

// How the ship drives. The defaults below are the starting point, the panel changes them live.
typedef struct {
    real mass;           // kg. with thrust, sets the acceleration: thrust / mass
    real thrust;         // N, forward
    real boost;          // %, extra forward thrust while boosting. 50 = 1.5 times thrust
    real reverse_thrust; // N, backward
    real side_thrust;    // N, left or right (side thrusters, the nose keeps pointing the same way)
    real lift_thrust;    // N, up or down, along the ship's up
    real drag;           // share of speed lost per second. 0 = real space, it drifts forever.
                         // top speed is where drag cancels thrust: (thrust / mass) / drag
    real pitch_speed;    // rad/s at full input
    real yaw_speed;      // rad/s
    real roll_speed;     // rad/s
    real turn_response;  // per second, how fast turning catches up with the input. high = snappy, low = heavy
    real thrust_response; // per second, how fast each thruster's force catches up with what the keys ask. high = instant
    real engine_cutoff;   // %, of a thruster's full force: it never runs weaker. it lights straight at this and goes out below it
    real drift_drag_cut; // %, how much less drag while drifting. 100 = none, it keeps all its speed
    // auto level: while Q/E are not pressed, the ship rolls itself upright (up = the grid's up)
    real level_angle;    // radians of tilt that ask for level_strength. less tilt, gentler
    real level_strength; // share of roll speed used for leveling, 0..1
} ShipTuning;

// defaults = the user's saved panel settings, 2026-11-14
static ShipTuning ship_tuning = {
    .mass = 1000,
    .thrust = 70000,         // 70 m/s^2
    .boost = 150,            // 175 m/s^2 while boosting
    .reverse_thrust = 45000, // 45 m/s^2
    .side_thrust = 34500,    // 34.5 m/s^2
    .lift_thrust = 30000,    // 30 m/s^2
    .drag = 0.5f,            // top speed 140 m/s, 350 boosting
    .pitch_speed = 2.2f,
    .yaw_speed = 2.3f,
    .roll_speed = 3.2f,
    .turn_response = 11.8f,
    .thrust_response = 4, // 63 % in 0.25 s, 95 % in 0.75 s
    .engine_cutoff = 10,
    .drift_drag_cut = 80, // drag 0.5 -> 0.1 per second
    .level_angle = 0.8f,
    .level_strength = 0.5f,
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

// The chase camera: behind and above the ship, fixed to it in the camera's axes, so speeding up never drags it off.
// Its turning lags: it turns after the ship, so a turn shows, but it always looks the way the ship does.
// It pulls back with speed: distance + pull_back per 100 m/s, at most max_distance (boost_max_distance while boosting),
// eased at pull_response so it never jumps
typedef struct {
    real distance;   // m behind the ship, standing still
    real max_distance;       // m, the most it pulls back
    real boost_max_distance; // m, the most while boosting
    real pull_back;          // m further back per 100 m/s of speed
    real pull_response;      // per second, how fast the distance catches up. high = stiff, low = slow
    real height;     // m above it
    real look_ahead; // m in front of the ship that the camera looks at
    real follow;     // per second, how fast the camera's turning catches up with the ship's. high = stiff, low = loose
} ShipCameraTuning;

static ShipCameraTuning ship_camera_tuning = {
    .distance = 6,
    .max_distance = 10,       // reached at 133 m/s (top speed 140)
    .boost_max_distance = 16, // reached at 333 m/s (boost top speed 350)
    .pull_back = 3,
    .pull_response = 2,       // 63 % in 0.5 s
    .height = 2.4f,
    .look_ahead = 7.9f,
    .follow = 5,
};

typedef struct {
    Particle particle;          // position, velocity, mass. forces go through particle_add_force
    Vector3 previous_position;  // from the step before, for blending
    Quaternion rotation;
    Quaternion previous_rotation;
    Vector3 turn;               // turn speed now, rad/s in the ship's own axes: x pitch, y yaw, z roll
    Vector3 thrust;             // thruster force now, N in the ship's own axes: x side (+ right), y lift (+ up),
                                // z forward (+ forward, - reverse, boost included). eases toward what the keys ask
} Ship;

typedef struct {
    Quaternion rotation, previous_rotation; // trails the ship's rotation. a roll tilts the view too
    real distance, previous_distance;       // m behind the ship now, pulled back with speed
} ShipCamera;

static Ship ship;
static ShipCamera ship_camera;
// Thruster exhaust: one emitter per nozzle, each fires with its thruster's force (ship_draw_exhaust).
// A thruster can have more than one nozzle (the main engine has two), they share its sparks.
// Every nozzle shoots against the push it gives: the main thruster pushes forward, so it shoots out the back.
enum { SHIP_THRUSTER_MAIN, SHIP_THRUSTER_REVERSE, SHIP_THRUSTER_RIGHT, SHIP_THRUSTER_LEFT, SHIP_THRUSTER_UP,
       SHIP_THRUSTER_DOWN, SHIP_THRUSTER_COUNT }; // named after the push: SHIP_THRUSTER_RIGHT pushes the ship right

typedef struct {
    i32 thruster;      // SHIP_THRUSTER_*
    Vector3 position;  // ship axes, m (nose -z, y up, right wing +x)
    Vector3 direction; // ship axes, unit: the way the exhaust leaves
} ShipNozzle;

#define SHIP_NOZZLE_COUNT 8
static const ShipNozzle ship_nozzles[SHIP_NOZZLE_COUNT] = {
    {SHIP_THRUSTER_MAIN, {-0.4f, 0.03f, 0.62f}, {0, 0, 1}},     // two engines on the back face, left and right
    {SHIP_THRUSTER_MAIN, {0.4f, 0.03f, 0.62f}, {0, 0, 1}},      //   of the middle, out the back
    {SHIP_THRUSTER_REVERSE, {-0.17f, 0, -0.85f}, {0, 0, -1}},   // two on the front edges, one each side of the
    {SHIP_THRUSTER_REVERSE, {0.17f, 0, -0.85f}, {0, 0, -1}},    //   nose, out the front
    {SHIP_THRUSTER_RIGHT, {-0.6f, 0.09f, 0.4f}, {-1, 0, 0}},    // on top of the left wing, out to the left
    {SHIP_THRUSTER_LEFT, {0.6f, 0.09f, 0.4f}, {1, 0, 0}},       // on top of the right wing, out to the right
    {SHIP_THRUSTER_UP, {0, -0.25f, 0.5f}, {0, -1, 0}},          // the keel, out the bottom
    {SHIP_THRUSTER_DOWN, {0, 0.35f, 0.45f}, {0, 1, 0}},         // the top ridge, out the top
};

// How every nozzle's exhaust looks (panel group particles). rate, speed and cone are the main thruster at full,
// each nozzle scales them by its thrust share (ship_draw_exhaust)
static RenderEmitter ship_exhaust = {
    .rate = 20000,
    .speed = 90,
    .speed_spread = 5,
    .cone = 0.15f,
    .lifetime = 1.0f,
    .lifetime_spread = 1.55f,
    .size = 0.05f,
    .color_start = {1.0f, 0.9f, 0.6f},
    .color_end = {1.0f, 0.3f, 0.05f},
};
static f32 ship_base_speed = 30; // m/s, spark speed at no thrust: speed = base + (look speed - base) * share (user: always some speed)
static f32 ship_cone_squeeze = 0.3f; // how much more thrust narrows the cone: cone / (1 + squeeze * (share - 1)). 0 = never
static RenderEmitter ship_nozzle_emitters[SHIP_NOZZLE_COUNT]; // the look above, plus each one's own memory
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


// The panel: what changes how it flies, and the camera. Mass is left out (only thrust / mass matters, so thrust
// covers it) and so is the aim (ShipAimTuning): change those in code.
static void ship_add_sliders(void) {
    plat_ui_group("flight");
    plat_ui_slider("thrust N", 0, 100000, 500, &ship_tuning.thrust);
    plat_ui_slider("boost %", 0, 300, 1, &ship_tuning.boost);
    plat_ui_slider("reverse N", 0, 100000, 500, &ship_tuning.reverse_thrust);
    plat_ui_slider("side N", 0, 100000, 500, &ship_tuning.side_thrust);
    plat_ui_slider("up/down N", 0, 100000, 500, &ship_tuning.lift_thrust);
    plat_ui_slider("drag /s", 0, 3, 0.01f, &ship_tuning.drag);
    plat_ui_slider("pitch rad/s", 0.1f, 6, 0.05f, &ship_tuning.pitch_speed);
    plat_ui_slider("yaw rad/s", 0.1f, 6, 0.05f, &ship_tuning.yaw_speed);
    plat_ui_slider("roll rad/s", 0.1f, 8, 0.05f, &ship_tuning.roll_speed);
    plat_ui_slider("turn response /s", 0.5f, 30, 0.1f, &ship_tuning.turn_response);
    plat_ui_slider("thrust response /s", 0.5f, 30, 0.1f, &ship_tuning.thrust_response);
    plat_ui_slider("engine cutoff %", 0, 50, 1, &ship_tuning.engine_cutoff);
    plat_ui_slider("drift drag cut %", 0, 100, 1, &ship_tuning.drift_drag_cut);
    plat_ui_group("camera");
    plat_ui_slider("distance m", 1, 30, 0.1f, &ship_camera_tuning.distance); // standing still
    plat_ui_slider("max distance m", 1, 60, 0.1f, &ship_camera_tuning.max_distance);
    plat_ui_slider("boost max distance m", 1, 60, 0.1f, &ship_camera_tuning.boost_max_distance);
    plat_ui_slider("pull back m per 100 m/s", 0, 20, 0.1f, &ship_camera_tuning.pull_back);
    plat_ui_slider("pull response /s", 0.1f, 30, 0.1f, &ship_camera_tuning.pull_response);
    plat_ui_slider("height m", -5, 10, 0.1f, &ship_camera_tuning.height);
    plat_ui_slider("look ahead m", 0, 30, 0.1f, &ship_camera_tuning.look_ahead);
    plat_ui_slider("follow /s", 0.5f, 30, 0.1f, &ship_camera_tuning.follow);
    plat_ui_group("particles");
    plat_ui_slider_log("max particles", 1024, 1048576, 1024, &render_particle_tuning.capacity); // log (user)
    plat_ui_slider_log("rate /s", 100, 1000000, 100, &ship_exhaust.rate); // log (user), so no 0. the main thruster at full
    plat_ui_slider("life s", 0.05f, 10, 0.05f, &ship_exhaust.lifetime);
    plat_ui_slider("life spread s", 0, 5, 0.05f, &ship_exhaust.lifetime_spread);
    plat_ui_slider("speed m/s", 0, 200, 0.5f, &ship_exhaust.speed); // the main thruster at full, others scale by force
    plat_ui_slider("base speed m/s", 0, 200, 0.5f, &ship_base_speed);
    plat_ui_slider("speed spread m/s", 0, 100, 0.5f, &ship_exhaust.speed_spread);
    plat_ui_slider("cone rad", 0, 3.14f, 0.01f, &ship_exhaust.cone); // the main thruster at full
    plat_ui_slider("cone squeeze", 0, 1, 0.01f, &ship_cone_squeeze);
    plat_ui_slider("size m", 0.005f, 1, 0.005f, &ship_exhaust.size);
    plat_ui_slider("streak s", 0, 0.2f, 0.002f, &render_particle_tuning.stretch);
    plat_ui_slider("particle drag /s", 0, 10, 0.05f, &render_particle_tuning.drag);
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
    ship_camera.rotation = ship_camera.previous_rotation = ship.rotation;
    ship_camera.distance = ship_camera.previous_distance = ship_camera_tuning.distance;
    for (i32 i = 0; i < SHIP_NOZZLE_COUNT; i++) ship_nozzle_emitters[i].memory.started = 0; // the ship jumped: no trail from where it was
    ship.thrust = vector3_zero();
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
    Vector3 up = quaternion_rotated_vector(ship_camera.rotation, vector3_create(0, 1, 0));
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

// Mapping: nose toward the aim (pitch, yaw), W/S thrust, A/D side thrust, Q/E roll, Shift/Control thrust up/down, Space boost,
// right mouse button drift. Nothing while the free camera is on.
static ShipInput ship_read_input(void) {
    ShipInput input = {0};
    if (demo_free_camera) return input;
    // the aim in the ship's own axes, then how far off it is up/down and left/right
    Vector3 right = quaternion_rotated_vector(ship.rotation, vector3_create(1, 0, 0));
    real aim_right = vector3_scalar_product(ship_aim, right);
    real aim_up = vector3_scalar_product(ship_aim, ship_up());
    real aim_ahead = vector3_scalar_product(ship_aim, ship_forward());
    // one turn, straight at the aim: (aim_right, aim_up) is the way the nose has to go, the angle off sets how hard.
    // pitch and yaw speeds differ, so the top speed in that direction is on the ellipse between them:
    // speed = 1 / sqrt((up / pitch_speed)^2 + (right / yaw_speed)^2) for the unit direction (right, up).
    // both inputs then get the same share of it, so the turn keeps its direction and the path stays straight
    real off_length = real_sqrt(aim_right * aim_right + aim_up * aim_up);
    if (off_length > 1e-6f) {
        real up = aim_up / off_length, right_part = aim_right / off_length;
        real share = f32_atan2(off_length, aim_ahead) / ship_aim_tuning.full_turn_angle;
        if (share > 1) share = 1;
        real per_pitch = up / ship_tuning.pitch_speed, per_yaw = right_part / ship_tuning.yaw_speed;
        real speed = 1 / real_sqrt(per_pitch * per_pitch + per_yaw * per_yaw);
        input.pitch = per_pitch * speed * share; // = up * speed * share / pitch_speed
        input.yaw = per_yaw * speed * share;
    }
    if (plat_key_down(KEY_W)) input.thrust += 1;
    if (plat_key_down(KEY_S)) input.thrust -= 1;
    if (plat_key_down(KEY_D)) input.side += 1;
    if (plat_key_down(KEY_A)) input.side -= 1;
    if (plat_key_down(KEY_SHIFT)) input.lift += 1;
    if (plat_key_down(KEY_CONTROL)) input.lift -= 1;
    input.drift = plat_key_down(KEY_MOUSE_RIGHT); // thrust keys keep being read: let go and they fire right away
    input.boost = plat_key_down(KEY_SPACE);
    if (plat_key_down(KEY_E)) input.roll += 1;
    if (plat_key_down(KEY_Q)) input.roll -= 1;
    if (input.roll == 0) { // auto level
        // the grid's up as the ship sees it, looking down its nose: how far it is tilted to the right of the ship's up.
        // pointing straight up or down there is no tilt to fix, so it fades out by how level the nose is
        Vector3 world_up = vector3_create(0, 1, 0);
        Vector3 forward = ship_forward();
        Vector3 across = vector3_subtracted(world_up, vector3_multiplied(forward, vector3_scalar_product(world_up, forward)));
        real level = vector3_magnitude(across); // 1 = nose level, 0 = straight up or down
        real tilt = f32_atan2(vector3_scalar_product(across, right), vector3_scalar_product(across, ship_up()));
        input.roll = ship_clamp_unit(tilt / ship_tuning.level_angle) * ship_tuning.level_strength * level;
    }
    input.pitch = ship_clamp_unit(input.pitch);
    input.yaw = ship_clamp_unit(input.yaw);
    return input;
}

// the two marks, far out along the nose and the aim: a cross where the nose points, a dot for the aim
#define SHIP_MARK_DISTANCE 200.0f // m
#define SHIP_SHOW_AIM 0           // 1 = draw the aim dot. hidden for now (user), the cross still follows it
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
    if (SHIP_SHOW_AIM && render_world_to_screen(vector3_added(position, vector3_multiplied(ship_aim, SHIP_MARK_DISTANCE)), &x, &y)) {
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

// One thruster axis (N, signed): eases toward wanted, but never runs weaker than cutoff (N): when the keys ask for
// more it lights straight at cutoff, when they ask for less it goes out once it drops below it
static real ship_thruster_ease(real now, real wanted, real catch_up, real cutoff) {
    now += (wanted - now) * catch_up;
    real size = now < 0 ? -now : now;
    real wanted_size = wanted < 0 ? -wanted : wanted;
    if (size < cutoff) now = wanted_size >= cutoff ? (wanted > 0 ? cutoff : -cutoff) : 0;
    return now;
}

// The flight model: thrust along the nose, side thrust along the wings, drag, and turn speeds that ease toward what the input asks for.
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

    // drift: all thrusters off, drag cut
    real drag = tuning->drag;
    if (input.drift) {
        input.thrust = 0;
        input.side = 0;
        input.lift = 0;
        drag *= 1 - tuning->drift_drag_cut / 100;
    }

    // the force the keys ask for, then each thruster's force eases toward it (like turning above)
    real forward = input.thrust > 0 ? tuning->thrust : tuning->reverse_thrust;
    if (input.thrust > 0 && input.boost) forward *= 1 + tuning->boost / 100;
    Vector3 wanted_thrust = vector3_create(input.side * tuning->side_thrust, input.lift * tuning->lift_thrust,
                                           input.thrust * forward);
    real thrust_catch_up = tuning->thrust_response * time_step;
    if (thrust_catch_up > 1) thrust_catch_up = 1;
    real cutoff = tuning->engine_cutoff / 100; // share of each thruster's full force
    real forward_full = (wanted_thrust.z != 0 ? wanted_thrust.z : ship.thrust.z) >= 0 ? tuning->thrust : tuning->reverse_thrust;
    ship.thrust.x = ship_thruster_ease(ship.thrust.x, wanted_thrust.x, thrust_catch_up, tuning->side_thrust * cutoff);
    ship.thrust.y = ship_thruster_ease(ship.thrust.y, wanted_thrust.y, thrust_catch_up, tuning->lift_thrust * cutoff);
    ship.thrust.z = ship_thruster_ease(ship.thrust.z, wanted_thrust.z, thrust_catch_up, forward_full * cutoff);

    // thrust along the nose, the right wing and the ship's up. drag as damping: the share of velocity kept this step
    particle_set_mass(&ship.particle, tuning->mass);
    particle_add_force(&ship.particle, vector3_multiplied(ship_forward(), ship.thrust.z));
    Vector3 right = quaternion_rotated_vector(ship.rotation, vector3_create(1, 0, 0));
    particle_add_force(&ship.particle, vector3_multiplied(right, ship.thrust.x));
    particle_add_force(&ship.particle, vector3_multiplied(ship_up(), ship.thrust.y));
    real kept = 1 - drag * time_step;
    ship.particle.damping = kept > 0 ? kept : 0;
    particle_integrate(&ship.particle, time_step);
}

// after ship_step: the camera's rotation closes part of the gap to the ship's, and its distance part of the gap
// to where the speed puts it. boosting = Space with forward thrust asked and no drift (the same as ship_step)
FORCE_INLINE void ship_camera_step(ShipInput input, real time_step) {
    ShipCameraTuning *tuning = &ship_camera_tuning;
    ship_camera.previous_rotation = ship_camera.rotation;
    real catch_up = tuning->follow * time_step;
    if (catch_up > 1) catch_up = 1;
    ship_camera.rotation = quaternion_lerp(ship_camera.rotation, ship.rotation, catch_up);

    ship_camera.previous_distance = ship_camera.distance;
    i32 boosting = input.boost && input.thrust > 0 && !input.drift;
    real most = boosting ? tuning->boost_max_distance : tuning->max_distance;
    if (most < tuning->distance) most = tuning->distance;
    real wanted = tuning->distance + tuning->pull_back * vector3_magnitude(ship.particle.velocity) / 100;
    if (wanted > most) wanted = most;
    real pull_catch_up = tuning->pull_response * time_step;
    if (pull_catch_up > 1) pull_catch_up = 1;
    ship_camera.distance += (wanted - ship_camera.distance) * pull_catch_up;
}

static Vector3 ship_drawn_position(f32 alpha) { return vector3_lerp(ship.previous_position, ship.particle.position, alpha); }

FORCE_INLINE void ship_draw(f32 alpha) {
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    render_draw_model(ship_model, ship_drawn_position(alpha), rotation, vector3_create(1, 1, 1), 0.85f, 0.87f, 0.92f);
}

// every nozzle, every frame: placed and turned with the ship. Its thrust share = its thruster's force now / main
// thrust (1 = main at full, more with boost, less for weaker thrusters or part input). From it (user):
//   rate = the look's rate * share / the thruster's nozzle count (two nozzles split the sparks), spark speed = ship_base_speed + (the look's speed - ship_base_speed) * share,
//   cone = the look's cone / (1 + ship_cone_squeeze * (share - 1)): more thrust, a little narrower.
// A nozzle that is off still calls render_emit (rate 0), so it remembers where it was when it starts again
static void ship_draw_exhaust(f32 alpha) {
    Vector3 force = ship.thrust; // N now, eased in ship_step
    real thruster_force[SHIP_THRUSTER_COUNT] = {
        [SHIP_THRUSTER_MAIN] = force.z > 0 ? force.z : 0,
        [SHIP_THRUSTER_REVERSE] = force.z < 0 ? -force.z : 0,
        [SHIP_THRUSTER_RIGHT] = force.x > 0 ? force.x : 0,
        [SHIP_THRUSTER_LEFT] = force.x < 0 ? -force.x : 0,
        [SHIP_THRUSTER_UP] = force.y > 0 ? force.y : 0,
        [SHIP_THRUSTER_DOWN] = force.y < 0 ? -force.y : 0,
    };
    i32 nozzle_count[SHIP_THRUSTER_COUNT] = {0};
    for (i32 i = 0; i < SHIP_NOZZLE_COUNT; i++) nozzle_count[ship_nozzles[i].thruster]++;
    real per_newton = ship_tuning.thrust > 0 ? 1 / ship_tuning.thrust : 0;
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    Vector3 position = ship_drawn_position(alpha);
    for (i32 i = 0; i < SHIP_NOZZLE_COUNT; i++) {
        RenderEmitter *emitter = &ship_nozzle_emitters[i];
        RenderEmitterMemory memory = emitter->memory;
        *emitter = ship_exhaust; // the look from the panel
        emitter->memory = memory;
        // world sparks (user): once out they fly free, with their shoot speed + the ship's velocity at that moment
        i32 thruster = ship_nozzles[i].thruster;
        real share = thruster_force[thruster] * per_newton;
        real squeeze = 1 + ship_cone_squeeze * (share - 1);
        emitter->rate = ship_exhaust.rate * share / nozzle_count[thruster];
        emitter->speed = ship_base_speed + (ship_exhaust.speed - ship_base_speed) * share;
        emitter->cone = ship_exhaust.cone / (squeeze > 0.1f ? squeeze : 0.1f);
        emitter->position = vector3_added(position, quaternion_rotated_vector(rotation, ship_nozzles[i].position));
        emitter->direction = quaternion_rotated_vector(rotation, ship_nozzles[i].direction);
        emitter->velocity = ship.particle.velocity;
        render_emit(emitter);
    }
}

// the chase view for this frame, blended between steps like the ship
// eye behind and above the ship in the camera's (trailing) axes, looking look_ahead in front of the ship's nose
static void ship_camera_view(f32 alpha, DemoView *view) {
    Quaternion rotation = quaternion_lerp(ship.previous_rotation, ship.rotation, alpha);
    Quaternion camera = quaternion_lerp(ship_camera.previous_rotation, ship_camera.rotation, alpha);
    Vector3 position = ship_drawn_position(alpha);
    Vector3 up = quaternion_rotated_vector(camera, vector3_create(0, 1, 0));
    real distance = ship_camera.previous_distance + (ship_camera.distance - ship_camera.previous_distance) * alpha;
    Vector3 offset = vector3_create(0, ship_camera_tuning.height, distance);
    view->eye = vector3_added(position, quaternion_rotated_vector(camera, offset));
    view->target = vector3_added(position, quaternion_rotated_vector(rotation, vector3_create(0, 0, -ship_camera_tuning.look_ahead)));
    view->up = up;
}
