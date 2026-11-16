#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"
#include "engine/engine.h"
#include "demo/demo.h"

// hunter2: the game (focus/game.md). Built from engine parts (focus/engine.md): the ship is an object with push and
// turn actuators and emitters, written out as plain tables below. Here too: the chase camera (not a part yet), the
// screen marks, and a debug grid to see it move.
// Click the canvas to lock the mouse, then the mouse moves the aim and the ship turns toward it, W/S thrust, A/D side
// thrust, Q/E roll, Shift/Control thrust up/down, Space boost, right mouse button drift, hold the left mouse button
// to fire (bullets: an emitter type that sends out copies of the object "bullet"), Esc lets the mouse go.
// Backspace: free fly camera and back.

#define HUNTER2_GRID_SPACING 10.0f // m
#define HUNTER2_GRID_LINES 41      // per direction (400 m across)
#define HUNTER2_GRID_HEIGHT -5.0f  // m, y of the grid plane

static i32 hunter2_grid = 0; // panel: debug grid on or off

// ---- The parts. Defaults = the user's saved settings (2026-11-15, session 5). In the ship's axes: nose -z, y up, right wing +x

enum { HUNTER2_SHIP, HUNTER2_BULLET, HUNTER2_PLANET, HUNTER2_ASTEROID, HUNTER2_OBJECT_COUNT };
#define HUNTER2_DRIFT INPUT_KEY(KEY_MOUSE_RIGHT) // hold: no thrust, less drag, the ship keeps its speed while it turns
static Object hunter2_objects[HUNTER2_OBJECT_COUNT] = {
    [HUNTER2_SHIP] = {
        .name = "ship", .mass = 1000,
        .drag = 0.5f,     // top speed 140 m/s, 350 boosting
        .drag_cut = 80,   // drag 0.5 -> 0.1 per second while drifting
        .drag_cut_input = HUNTER2_DRIFT,
        .radius = 0.9f,   // its ball for gravity and hits: the wing tips and the nose
        .model = -1, .size = 1, .color = {0.85f, 0.87f, 0.92f}, // model built in hunter2_init
    },
    // not in the world itself: the machine gun emitters send out copies. no drag: it keeps its speed until its life is up
    [HUNTER2_BULLET] = {
        .name = "bullet", .copies_only = 1, .mass = 9817.48f, .drag = 0, .lifetime = 7.2f,
        .model = -1, .size = 0.1f, .color = {1.0f, 0.4f, 0.72f}, .glow = 1, .halo = 0.08f, // a ball, built in hunter2_init
        .pull_off = 1, // pulls nothing: every bullet that pulls would cost a pass over every body
        // a hit: one step to push what it hit, then gone, an explosion where it was (hit_burst set in hunter2_init: the
        // emitter types come further down), moving with what it hit
        .hit_steps = 1, .hit_moves_with = 0, .hit_straight_out = 0.5f, .hit_speed_share = 0.13f,
    },
    // put down by the universe's placing "planets", each its own size. mass from density, like Gravity Cloud
    [HUNTER2_PLANET] = {
        .name = "planet", .copies_only = 1, .density = 5000, .mass = 1,
        .model = -1, .size = 1, .color = {0.55f, 0.5f, 0.45f}, // the renderer's sphere, set in hunter2_init
        // land and sea or stripes in sand, deep blue and pale cream, a blue rim of air
        .surface = {.color_two = {0.2f, 0.33f, 0.55f}, .color_three = {0.88f, 0.82f, 0.7f}, .pattern = 2, .detail = 2.5f,
                    .roughness = 0.5f, .air_color = {0.45f, 0.65f, 1.0f}, .air_thickness = 0.6f, .color_sets = 1,
                    .variation = 0.15f},
    },
    // put down by the placing "asteroids": many small rocks, denser than planets, grey and brown patches, no air
    [HUNTER2_ASTEROID] = {
        .name = "asteroid", .copies_only = 1, .density = 1334, .mass = 1,
        .pull_off = 1, // pulled by planets and the ship, pulls nothing: thousands stay cheap
        .model = -1, .size = 1, .color = {0.42f, 0.4f, 0.38f}, // the renderer's sphere, set in hunter2_init
        .surface = {.color_two = {0.3f, 0.25f, 0.2f}, .color_three = {0.6f, 0.57f, 0.52f}, .pattern = 0, .detail = 3,
                    .roughness = 0.8f, .variation = 0.4f},
    },
};

// the universe (user's save, session 6 end): a few big slow planets and many small fast asteroids, in the universe's
// spread (15 km across)
static Placing hunter2_placings[] = {
    {.name = "planets", .object = HUNTER2_PLANET, .count = 30, .size_min = 282, .size_max = 1144,
     .speed_min = 0, .speed_max = 17.5f, .mostly_small = 0.15f, .small_faster = 1},
    {.name = "asteroids", .object = HUNTER2_ASTEROID, .count = 5000, .size_min = 5, .size_max = 30,
     .speed_min = 5, .speed_max = 30, .mostly_small = 0.95f, .small_faster = 0.95f},
};

// names for now (focus/engine.md)
enum {
    HUNTER2_ACTUATOR_TYPE_ENGINE, HUNTER2_ACTUATOR_TYPE_REVERSE, HUNTER2_ACTUATOR_TYPE_SIDE, HUNTER2_ACTUATOR_TYPE_LIFT,
    HUNTER2_ACTUATOR_TYPE_PITCH, HUNTER2_ACTUATOR_TYPE_YAW, HUNTER2_ACTUATOR_TYPE_ROLL, HUNTER2_ACTUATOR_TYPE_COUNT
};
static ActuatorType hunter2_actuator_types[ENGINE_MAX_ACTUATOR_TYPES] = { // the places after ours: new ones from the panel
    // thrust ramps: response 4 = 63 % in 0.25 s, 95 % in 0.75 s
    [HUNTER2_ACTUATOR_TYPE_ENGINE] = {.name = "engine", .kind = ACTUATOR_PUSH, .force = 70000, .boost = 150, .cutoff = 10, .response = 4},
    [HUNTER2_ACTUATOR_TYPE_REVERSE] = {.name = "reverse", .kind = ACTUATOR_PUSH, .force = 45000, .cutoff = 10, .response = 4},
    [HUNTER2_ACTUATOR_TYPE_SIDE] = {.name = "side thrust", .kind = ACTUATOR_PUSH, .force = 34500, .cutoff = 10, .response = 4},
    [HUNTER2_ACTUATOR_TYPE_LIFT] = {.name = "up/down thrust", .kind = ACTUATOR_PUSH, .force = 30000, .cutoff = 10, .response = 4},
    [HUNTER2_ACTUATOR_TYPE_PITCH] = {.name = "pitch", .kind = ACTUATOR_TURN, .speed = 2.2f, .response = 11.8f},
    [HUNTER2_ACTUATOR_TYPE_YAW] = {.name = "yaw", .kind = ACTUATOR_TURN, .speed = 2.3f, .response = 11.8f},
    [HUNTER2_ACTUATOR_TYPE_ROLL] = {.name = "roll", .kind = ACTUATOR_TURN, .speed = 3.2f, .response = 11.8f},
};

// push: direction = the way it pushes. turn: the axis, input 1 turns the right hand way around it
// (+x lifts the nose, +y swings it left, +z lifts the right wing: so yaw and roll axes point the other way)
enum {
    HUNTER2_ENGINE, HUNTER2_REVERSE, HUNTER2_RIGHT, HUNTER2_LEFT, HUNTER2_UP, HUNTER2_DOWN,
    HUNTER2_PITCH, HUNTER2_YAW, HUNTER2_ROLL, HUNTER2_ACTUATOR_COUNT
};
static Actuator hunter2_actuators[HUNTER2_ACTUATOR_COUNT] = {
    [HUNTER2_ENGINE] = {.name = "engine", .type = HUNTER2_ACTUATOR_TYPE_ENGINE, .object = HUNTER2_SHIP, .direction = {0, 0, -1},
                        .input = INPUT_KEY(KEY_W), .block = HUNTER2_DRIFT, .boost = INPUT_KEY(KEY_SPACE)},
    [HUNTER2_REVERSE] = {.name = "reverse", .type = HUNTER2_ACTUATOR_TYPE_REVERSE, .object = HUNTER2_SHIP, .direction = {0, 0, 1},
                         .input = INPUT_KEY(KEY_S), .block = HUNTER2_DRIFT},
    [HUNTER2_RIGHT] = {.name = "right", .type = HUNTER2_ACTUATOR_TYPE_SIDE, .object = HUNTER2_SHIP, .direction = {1, 0, 0},
                       .input = INPUT_KEY(KEY_D), .block = HUNTER2_DRIFT},
    [HUNTER2_LEFT] = {.name = "left", .type = HUNTER2_ACTUATOR_TYPE_SIDE, .object = HUNTER2_SHIP, .direction = {-1, 0, 0},
                      .input = INPUT_KEY(KEY_A), .block = HUNTER2_DRIFT},
    [HUNTER2_UP] = {.name = "up", .type = HUNTER2_ACTUATOR_TYPE_LIFT, .object = HUNTER2_SHIP, .direction = {0, 1, 0},
                    .input = INPUT_KEY(KEY_SHIFT), .block = HUNTER2_DRIFT},
    [HUNTER2_DOWN] = {.name = "down", .type = HUNTER2_ACTUATOR_TYPE_LIFT, .object = HUNTER2_SHIP, .direction = {0, -1, 0},
                      .input = INPUT_KEY(KEY_CONTROL), .block = HUNTER2_DRIFT},
    [HUNTER2_PITCH] = {.name = "pitch", .type = HUNTER2_ACTUATOR_TYPE_PITCH, .object = HUNTER2_SHIP, .direction = {1, 0, 0},
                       .input = INPUT_AIM_PITCH},
    [HUNTER2_YAW] = {.name = "yaw", .type = HUNTER2_ACTUATOR_TYPE_YAW, .object = HUNTER2_SHIP, .direction = {0, -1, 0},
                     .input = INPUT_AIM_YAW},
    // E rolls the right wing down, Q left. Neither: nothing (space has no up), or the ship levels itself to world up
    // when "when idle" is level (fallback_on 1)
    [HUNTER2_ROLL] = {.name = "roll", .type = HUNTER2_ACTUATOR_TYPE_ROLL, .object = HUNTER2_SHIP, .direction = {0, 0, -1},
                      .input = INPUT_KEY_PAIR(KEY_E, KEY_Q), .fallback = INPUT_TILT},
};

// names for now (user: "main" is too nondescript, later maybe ship_propulsion_emitter / ship_thruster_emitter)
// spark numbers = the user's save (2026-11-15, session 4). "reverse" is the user's own type, made in the panel
enum {
    HUNTER2_EMITTER_TYPE_MAIN, HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_EMITTER_TYPE_REVERSE, HUNTER2_EMITTER_TYPE_MACHINE_GUN,
    HUNTER2_EMITTER_TYPE_EXPLOSION, HUNTER2_EMITTER_TYPE_COUNT
};
static EmitterType hunter2_emitter_types[ENGINE_MAX_EMITTER_TYPES] = {
    [HUNTER2_EMITTER_TYPE_MAIN] = {
        .name = "main", .rate = 5000, .speed = 28.5f, .speed_spread = 5, .base_speed = 30, .cone = 0.15f,
        .cone_squeeze = 0.3f, .lifetime = 1.5f, .lifetime_spread = 1, .size = 0.05f,
        .color_start = {1.0f, 0.9f, 0.6f}, .color_end = {1.0f, 0.3f, 0.05f}, .stretch = 0, .drag = 0.5f,
    },
    [HUNTER2_EMITTER_TYPE_SIDE] = {
        .name = "side", .rate = 800, .speed = 5, .speed_spread = 5, .base_speed = 16, .cone = 0.2f,
        .cone_squeeze = 0.3f, .lifetime = 1.5f, .lifetime_spread = 1, .size = 0.02f,
        .color_start = {0.51f, 0.51f, 0.51f}, .color_end = {0, 0, 0}, .stretch = 0, .drag = 0.55f,
    },
    [HUNTER2_EMITTER_TYPE_REVERSE] = {
        .name = "reverse", .rate = 900, .speed = 30, .speed_spread = 25.5f, .base_speed = 50, .cone = 0.1f,
        .cone_squeeze = 0.2f, .lifetime = 2, .lifetime_spread = 1, .size = 0.02f,
        .color_start = {0.56f, 0.56f, 0.56f}, .color_end = {0, 0, 0}, .stretch = 0, .drag = 1,
    },
    // bullets: 10 a second from each wing, 145 m/s on top of the ship's own speed
    [HUNTER2_EMITTER_TYPE_MACHINE_GUN] = {
        .name = "machine gun", .emits = EMITS_OBJECTS, .object = HUNTER2_BULLET, .rate = 10, .speed = 145, .speed_spread = 0,
        .cone = 0,
    },
    // where a bullet hits: fires once, a cone 1.24 rad denser in the middle, speeds 0 give or take 16, red to pale yellow
    [HUNTER2_EMITTER_TYPE_EXPLOSION] = {
        .name = "explosion", .burst = 480, .rate = 5000, .speed = 0, .speed_spread = 16, .base_speed = 0, .cone = 1.24f, .cone_middle = 0.55f,
        .lifetime = 1.6f, .lifetime_spread = 0.8f, .size = 0.09f,
        .color_start = {1.0f, 0.04f, 0.04f}, .color_end = {1.0f, 0.99f, 0.66f}, .stretch = 0, .drag = 0.45f,
    },
};

// every nozzle shoots against the push it gives, input = how hard its actuator pushes
#define HUNTER2_EMITTER(name_, type_, actuator, x, y, z, dx, dy, dz) \
    {.name = name_, .type = type_, .object = HUNTER2_SHIP, .position = {x, y, z}, .direction = {dx, dy, dz}, .input = INPUT_ACTUATOR(actuator)}
#define HUNTER2_EMITTER_COUNT 12
static Emitter hunter2_emitters[HUNTER2_EMITTER_COUNT] = {
    HUNTER2_EMITTER("engine left", HUNTER2_EMITTER_TYPE_MAIN, HUNTER2_ENGINE, -0.4f, 0.03f, 0.62f, 0, 0, 1),  // two engines on the back face,
    HUNTER2_EMITTER("engine right", HUNTER2_EMITTER_TYPE_MAIN, HUNTER2_ENGINE, 0.4f, 0.03f, 0.62f, 0, 0, 1),   //   left and right of the middle
    HUNTER2_EMITTER("reverse left", HUNTER2_EMITTER_TYPE_REVERSE, HUNTER2_REVERSE, -0.17f, 0, -0.85f, 0, 0, -1),  // two on the front edges,
    HUNTER2_EMITTER("reverse right", HUNTER2_EMITTER_TYPE_REVERSE, HUNTER2_REVERSE, 0.17f, 0, -0.85f, 0, 0, -1),   //   one each side of the nose
    HUNTER2_EMITTER("left wing", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_RIGHT, -0.6f, 0.09f, 0.4f, -1, 0, 0),   // on top of the left wing
    HUNTER2_EMITTER("right wing", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_LEFT, 0.6f, 0.09f, 0.4f, 1, 0, 0),      // on top of the right wing
    // up / down: the middle of each wing, a little off its faces (upper y 0.106, lower y -0.067 at x 0.45, z 0.3),
    // just behind the machine guns
    HUNTER2_EMITTER("left wing bottom", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_UP, -0.45f, -0.08f, 0.3f, 0, -1, 0),
    HUNTER2_EMITTER("right wing bottom", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_UP, 0.45f, -0.08f, 0.3f, 0, -1, 0),
    HUNTER2_EMITTER("left wing top", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_DOWN, -0.45f, 0.12f, 0.3f, 0, 1, 0),
    HUNTER2_EMITTER("right wing top", HUNTER2_EMITTER_TYPE_SIDE, HUNTER2_DOWN, 0.45f, 0.12f, 0.3f, 0, 1, 0),
    // under the middle of each wing, a little below its lower face (y -0.056 there), firing along the nose
    {.name = "under left wing", .type = HUNTER2_EMITTER_TYPE_MACHINE_GUN, .object = HUNTER2_SHIP, .position = {-0.45f, -0.1f, 0.2f},
     .direction = {0, 0, -1}, .input = INPUT_KEY(KEY_MOUSE_LEFT)},
    {.name = "under right wing", .type = HUNTER2_EMITTER_TYPE_MACHINE_GUN, .object = HUNTER2_SHIP, .position = {0.45f, -0.1f, 0.2f},
     .direction = {0, 0, -1}, .input = INPUT_KEY(KEY_MOUSE_LEFT)},
};

// places for copies: 30 planets + 5000 asteroids + 2 emitters x 10 a second x 7.2 s = 144 bullets, room for the panel
#define HUNTER2_COPY_CAPACITY 8192 // bullets, planets and asteroids
static ObjectCopy hunter2_copies[HUNTER2_COPY_CAPACITY];

static Engine hunter2_engine = {
    .objects = hunter2_objects, .object_count = HUNTER2_OBJECT_COUNT,
    .actuator_types = hunter2_actuator_types, .actuator_type_count = HUNTER2_ACTUATOR_TYPE_COUNT,
    .actuators = hunter2_actuators, .actuator_count = HUNTER2_ACTUATOR_COUNT,
    .emitter_types = hunter2_emitter_types, .emitter_type_count = HUNTER2_EMITTER_TYPE_COUNT,
    .emitters = hunter2_emitters, .emitter_count = HUNTER2_EMITTER_COUNT,
    .copies = hunter2_copies, .copy_capacity = HUNTER2_COPY_CAPACITY,
    .placings = hunter2_placings, .placing_count = (i32)(sizeof hunter2_placings / sizeof hunter2_placings[0]),
    // the user's: 2000 x real G
    .gravity = 2000,
    .bounce = 0.95f,
    .spread = 15000,
    // stars on the sky: most faint, a few bright, white tinted toward blue or orange
    .stars = {.count = 20000, .size = 2.9f, .brightness = 0.6f, .twinkle = 0.1f, .color = {1.0f, 0.97f, 0.92f},
              .tint_one = {0.6f, 0.75f, 1.0f}, .tint_two = {1.0f, 0.92f, 0.4f}},
};
#define hunter2_ship (&hunter2_objects[HUNTER2_SHIP])

static RenderModel hunter2_ship_model = -1; // built once, kept across demo switches
static RenderModel hunter2_ball_model = -1;   // bullets

// a wedge: nose, two wing tips, a low ridge on top and a keel below, all meeting at the back. 1.8 m long and wide
static void hunter2_build_ship_model(void) {
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
    hunter2_ship_model = render_create_model(corners, (i32)(sizeof(corners) / sizeof(corners[0])) / 3);
}

// a ball 1 m across the middle (drawn at the object's size): the 8 faces of an octahedron, each cut into cuts x cuts
// smaller triangles, every corner pushed out onto the ball. 4 cuts: 128 flat faces, 16 cuts: 2048
#define HUNTER2_BALL_MAX_CUTS 4
static RenderModel hunter2_build_ball_model(i32 cuts) {
    static Vector3 corners[8 * HUNTER2_BALL_MAX_CUTS * HUNTER2_BALL_MAX_CUTS * 3];
    i32 count = 0;
    for (i32 face = 0; face < 8; face++) {
        // the face's three corners on the axes: x, y, z, each + or - by the face's bits
        Vector3 a = vector3_create(face & 1 ? -1 : 1, 0, 0);
        Vector3 b = vector3_create(0, face & 2 ? -1 : 1, 0);
        Vector3 c = vector3_create(0, 0, face & 4 ? -1 : 1);
        for (i32 row = 0; row < cuts; row++) {
            for (i32 column = 0; column < cuts - row; column++) {
                // a point on the face: row steps from a toward b, column from a toward c
                Vector3 points[4];
                i32 steps[4][2] = {{row, column}, {row + 1, column}, {row, column + 1}, {row + 1, column + 1}};
                for (i32 k = 0; k < 4; k++) {
                    real along_b = (real)steps[k][0] / cuts, along_c = (real)steps[k][1] / cuts;
                    Vector3 point = vector3_multiplied(a, 1 - along_b - along_c);
                    vector3_add_scaled(&point, b, along_b);
                    vector3_add_scaled(&point, c, along_c);
                    vector3_normalize(&point);
                    points[k] = vector3_multiplied(point, 0.5f);
                }
                corners[count++] = points[0]; corners[count++] = points[1]; corners[count++] = points[2];
                if (column < cuts - row - 1) {
                    corners[count++] = points[1]; corners[count++] = points[3]; corners[count++] = points[2];
                }
            }
        }
    }
    return render_create_model(corners, count / 3);
}

// ---- The chase camera: behind and above the ship, fixed to it in the camera's axes, so speeding up never drags it
// off. Its turning lags: it turns after the ship, so a turn shows, but it always looks the way the ship does.
// It pulls back with speed: distance + pull_back per 100 m/s, at most max_distance (boost_max_distance while
// boosting), eased at pull_response so it never jumps
typedef struct {
    real distance;           // m behind the ship, standing still
    real max_distance;       // m, the most it pulls back
    real boost_max_distance; // m, the most while boosting
    real pull_back;          // m further back per 100 m/s of speed
    real pull_response;      // per second, how fast the distance catches up. high = stiff, low = slow
    real height;             // m above it
    real look_ahead;         // m in front of the ship that the camera looks at
    real follow;             // per second, how fast the camera's turning catches up with the ship's. high = stiff, low = loose
} Hunter2CameraTuning;

static Hunter2CameraTuning hunter2_camera_tuning = {
    .distance = 6,
    .max_distance = 10,       // reached at 133 m/s (top speed 140)
    .boost_max_distance = 16, // reached at 333 m/s (boost top speed 350)
    .pull_back = 8.1f,
    .pull_response = 2,       // 63 % in 0.5 s
    .height = 3.3f,
    .look_ahead = 7.9f,
    .follow = 5,
};

typedef struct {
    Quaternion rotation, previous_rotation; // trails the ship's rotation. a roll tilts the view too
    real distance, previous_distance;       // m behind the ship now, pulled back with speed
} Hunter2Camera;
static Hunter2Camera hunter2_camera;

static void hunter2_add_camera_sliders(void) {
    plat_ui_group("camera");
    plat_ui_slider("distance m", 1, 30, 0.1f, &hunter2_camera_tuning.distance); // standing still
    plat_ui_slider("max distance m", 1, 60, 0.1f, &hunter2_camera_tuning.max_distance);
    plat_ui_slider("boost max distance m", 1, 60, 0.1f, &hunter2_camera_tuning.boost_max_distance);
    plat_ui_slider("pull back m per 100 m/s", 0, 20, 0.1f, &hunter2_camera_tuning.pull_back);
    plat_ui_slider("pull response /s", 0.1f, 30, 0.1f, &hunter2_camera_tuning.pull_response);
    plat_ui_slider("height m", -5, 10, 0.1f, &hunter2_camera_tuning.height);
    plat_ui_slider("look ahead m", 0, 30, 0.1f, &hunter2_camera_tuning.look_ahead);
    plat_ui_slider("follow /s", 0.5f, 30, 0.1f, &hunter2_camera_tuning.follow);
}

// boosting = Space with forward thrust asked (W, not S) and no drift, the pilot flying (not the free camera)
static i32 hunter2_boosting(void) {
    return !demo_free_camera && plat_key_down(KEY_SPACE) && plat_key_down(KEY_W) && !plat_key_down(KEY_S) &&
           !plat_key_down(KEY_MOUSE_RIGHT);
}

// after the engine's step: the camera's rotation closes part of the gap to the ship's, and its distance part of
// the gap to where the speed puts it
FORCE_INLINE void hunter2_camera_step(real time_step) {
    Hunter2CameraTuning *tuning = &hunter2_camera_tuning;
    hunter2_camera.previous_rotation = hunter2_camera.rotation;
    real catch_up = tuning->follow * time_step;
    if (catch_up > 1) catch_up = 1;
    hunter2_camera.rotation = quaternion_lerp(hunter2_camera.rotation, hunter2_ship->rotation, catch_up);

    hunter2_camera.previous_distance = hunter2_camera.distance;
    real most = hunter2_boosting() ? tuning->boost_max_distance : tuning->max_distance;
    if (most < tuning->distance) most = tuning->distance;
    real wanted = tuning->distance + tuning->pull_back * vector3_magnitude(hunter2_ship->particle.velocity) / 100;
    if (wanted > most) wanted = most;
    real pull_catch_up = tuning->pull_response * time_step;
    if (pull_catch_up > 1) pull_catch_up = 1;
    hunter2_camera.distance += (wanted - hunter2_camera.distance) * pull_catch_up;
}

// the chase view for this frame, blended between steps like the ship
// eye behind and above the ship in the camera's (trailing) axes, looking look_ahead in front of the ship's nose
static void hunter2_camera_view(f32 alpha, DemoView *view) {
    Quaternion rotation = object_drawn_rotation(hunter2_ship, alpha);
    Quaternion camera = quaternion_lerp(hunter2_camera.previous_rotation, hunter2_camera.rotation, alpha);
    Vector3 position = object_drawn_position(hunter2_ship, alpha);
    real distance = hunter2_camera.previous_distance + (hunter2_camera.distance - hunter2_camera.previous_distance) * alpha;
    Vector3 offset = vector3_create(0, hunter2_camera_tuning.height, distance);
    view->eye = vector3_added(position, quaternion_rotated_vector(camera, offset));
    view->target = vector3_added(position, quaternion_rotated_vector(rotation, vector3_create(0, 0, -hunter2_camera_tuning.look_ahead)));
    view->up = quaternion_rotated_vector(camera, vector3_create(0, 1, 0));
}

// ---- Screen marks: far out along the nose and the aim, a cross where the nose points, a dot for the aim
#define HUNTER2_MARK_DISTANCE 200.0f // m
#define HUNTER2_SHOW_AIM 0           // 1 = draw the aim dot. hidden for now (user), the cross still follows it
static void hunter2_draw_marks(f32 alpha) {
    if (demo_free_camera || !plat_mouse_locked()) return;
    Vector3 position = object_drawn_position(hunter2_ship, alpha);
    Quaternion rotation = object_drawn_rotation(hunter2_ship, alpha);
    Vector3 nose = vector3_added(position, quaternion_rotated_vector(rotation, vector3_create(0, 0, -HUNTER2_MARK_DISTANCE)));
    f32 x, y;
    if (render_world_to_screen(nose, &x, &y)) {
        f32 arm = 10, width = 2; // a cross with a gap in the middle
        render_draw_rect(x - arm, y - width / 2, arm - 3, width, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x + 3, y - width / 2, arm - 3, width, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x - width / 2, y - arm, width, arm - 3, 0.9f, 0.95f, 1, 0.8f);
        render_draw_rect(x - width / 2, y + 3, width, arm - 3, 0.9f, 0.95f, 1, 0.8f);
    }
    Vector3 aim = vector3_added(position, vector3_multiplied(hunter2_ship->aim, HUNTER2_MARK_DISTANCE));
    if (HUNTER2_SHOW_AIM && render_world_to_screen(aim, &x, &y)) render_draw_circle(x, y, 4, 1, 0.75f, 0.3f, 0.95f);
}

// a hint while the mouse is free
static void hunter2_draw_hint(void) {
    if (demo_free_camera || plat_mouse_locked()) return;
    const char *hint = "click to fly with the mouse";
    f32 scale = 2; // glyphs are 8 px
    f32 x = (f32)plat_screen_width() / 2 - str_len(hint) * 4 * scale, y = (f32)plat_screen_height() * 0.75f;
    render_draw_text(hint, x, y, scale, 0.6f, 0.6f, 0.7f);
}

// ---- The demo

static void hunter2_add_panel(void) {
    engine_add_panel(&hunter2_engine);
    hunter2_add_camera_sliders();
    plat_ui_group("debug");
    plat_ui_choice("grid", "Off|On", &hunter2_grid);
}

static void hunter2_init(void) {
    if (hunter2_ship_model < 0) hunter2_build_ship_model();
    hunter2_ship->model = hunter2_ship_model;
    if (hunter2_ball_model < 0) hunter2_ball_model = hunter2_build_ball_model(4);
    hunter2_objects[HUNTER2_BULLET].model = hunter2_ball_model;
    hunter2_objects[HUNTER2_BULLET].hit_burst = HUNTER2_EMITTER_TYPE_EXPLOSION + 1;
    hunter2_objects[HUNTER2_PLANET].model = OBJECT_MODEL_BALL; // the renderer's smooth sphere, as in the other demos
    hunter2_objects[HUNTER2_ASTEROID].model = OBJECT_MODEL_BALL;
    demo_far = 20000; // planets kilometers apart
    engine_start(&hunter2_engine);
    hunter2_camera.rotation = hunter2_camera.previous_rotation = hunter2_ship->rotation;
    hunter2_camera.distance = hunter2_camera.previous_distance = hunter2_camera_tuning.distance;
    hunter2_add_panel();
}

// once per browser frame: the mouse moves the ship's aim
static void hunter2_update(f32 seconds) {
    (void)seconds;
    if (engine_update_panel(&hunter2_engine)) { // a panel click changed the parts: the panel shows them again
        plat_ui_clear();
        hunter2_add_panel();
    }
    Vector3 camera_up = quaternion_rotated_vector(hunter2_camera.rotation, vector3_create(0, 1, 0));
    object_update_aim(hunter2_ship, camera_up, plat_mouse_locked() && !demo_free_camera);
}

FORCE_INLINE void hunter2_step(real time_step) {
    hunter2_engine.inputs_on = !demo_free_camera;
    engine_step(&hunter2_engine, time_step);
    hunter2_camera_step(time_step);
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
    Vector3 position = object_drawn_position(hunter2_ship, alpha);
    real reach = 1.5f; // sun shadows around the ship
    Vector3 around = vector3_create(reach, reach, reach);
    render_set_shadow_area(vector3_subtracted(position, around), vector3_added(position, around));
    if (hunter2_grid) hunter2_draw_grid(position);
    engine_draw(&hunter2_engine, alpha);
    hunter2_draw_hint();
    hunter2_draw_marks(alpha);

    char line[128];
    line[0] = 0;
    str_append(line, sizeof(line), "speed m/s: ");
    str_append_i32(line, sizeof(line), (i32)(vector3_magnitude(hunter2_ship->particle.velocity) + 0.5f));
    if (!demo_free_camera && plat_key_down(KEY_MOUSE_RIGHT)) str_append(line, sizeof(line), "  drift");
    else if (!demo_free_camera && plat_key_down(KEY_SPACE) && plat_key_down(KEY_W)) str_append(line, sizeof(line), "  boost");
    str_append(line, sizeof(line), "  thrust N ");
    str_append_i32(line, sizeof(line), (i32)(hunter2_actuators[HUNTER2_ENGINE].now - hunter2_actuators[HUNTER2_REVERSE].now));
    str_append(line, sizeof(line), "  particles ~");
    str_append_i32(line, sizeof(line), render_particle_estimate());
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
}

static void hunter2_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "speed", (i32)vector3_magnitude(hunter2_ship->particle.velocity));
}
