#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"
#include "demo/demo.h"
#include "game/game.h"

// Physics runs in fixed steps. Drawing happens once per browser frame
// and blends between the last two steps, so motion stays smooth on any screen.
#define GAME_STEP (1.0 / 60.0)
#define GAME_MAX_FRAME_SECONDS 0.25 // after a stall, do not try to catch up on all of it
#define GAME_MAX_STEPS 4 // per frame. if steps cost more than they cover, drop the rest: slow motion instead of a pile up

// Free fly camera, only while the mouse is locked: mouse looks, WASD moves where you look, E/Q up/down, Shift faster.
// Demos with their own camera (demo_view, own_camera) use theirs. Backspace switches to the free camera, starting
// from where the demo's camera was, and back.
#define CAMERA_SPEED 5.0f      // m/s
#define CAMERA_FAST 4.0f       // speed multiplier with Shift
#define CAMERA_TURN 0.0025f    // radians per pixel of mouse movement
#define CAMERA_MAX_PITCH 1.55f // just short of straight up/down, so the view never flips
#define CAMERA_FOV (F32_PI / 3) // 60 degrees vertical

typedef struct {
    Vector3 position;
    f32 yaw;   // 0 looks down -z, positive turns right
    f32 pitch; // 0 is level, positive looks up
} Camera;

static Camera game_camera; // set from the demo's start view
static i32 game_demo;      // the one running, DEMO_*
static i32 game_demo_picked; // the one asked for: demo bar clicks (JS writes it), [ and ], ?scene=
static i32 game_free_camera_picked; // Backspace, only for demos with their own camera

// time
static f64 game_last_ms = -1;
static f64 game_accumulator; // seconds of physics we still owe

// stats, logged once per second
static f64 game_stats_start_ms;
static i32 game_stats_frames;
static i32 game_stats_steps;
static f64 game_stats_cpu_ms;

// last second's stats, for the on screen text
static i32 game_hud_fps;
static i32 game_hud_cpu_us; // cpu per frame
static i32 game_hud_steps;  // steps per second, under 60 means the physics runs in slow motion

// also for switching: the old demo's sliders go, the new one starts fresh from its start view
static void game_start_demo(i32 demo) {
    plat_ui_clear();
    game_demo = demo;
    game_accumulator = 0;
    game_free_camera_picked = 0;
    game_camera.position = demo_list[demo].camera_position;
    game_camera.yaw = demo_list[demo].camera_yaw;
    game_camera.pitch = demo_list[demo].camera_pitch;
    plat_ui_title(demo_list[demo].name, demo_list[demo].title_color);
    plat_ui_demo_current(demo);
    demo_init(demo);
}

void game_init(void) {
    plat_log("hello from C");

    render_init();
    plat_log_i32("gpu calls at init", plat_gpu_stats.calls);

    for (i32 i = 0; i < DEMO_COUNT; i++) plat_ui_demo(demo_list[i].id, demo_list[i].chapter, demo_list[i].name);
    game_demo_picked = DEMO_HUNTER2;
    plat_ui_demo_bar(&game_demo_picked); // may pick another one from ?scene=
    game_start_demo(game_demo_picked);
}

static Vector3 game_camera_forward(void) {
    f32 c = f32_cos(game_camera.pitch);
    return vector3_create(f32_sin(game_camera.yaw) * c, f32_sin(game_camera.pitch), -f32_cos(game_camera.yaw) * c);
}

// the camera only moves while the mouse is locked (click the canvas), so the panel can be used without it drifting
static void game_update_camera(f32 seconds) {
    if (!demo_free_camera || !plat_mouse_locked()) return;

    game_camera.yaw += plat_mouse_dx() * CAMERA_TURN;
    game_camera.pitch -= plat_mouse_dy() * CAMERA_TURN;
    if (game_camera.pitch > CAMERA_MAX_PITCH) game_camera.pitch = CAMERA_MAX_PITCH;
    if (game_camera.pitch < -CAMERA_MAX_PITCH) game_camera.pitch = -CAMERA_MAX_PITCH;

    Vector3 forward = game_camera_forward();
    Vector3 right = vector3_create(f32_cos(game_camera.yaw), 0, f32_sin(game_camera.yaw));
    Vector3 move = vector3_zero();
    if (plat_key_down(KEY_W)) vector3_add(&move, forward);
    if (plat_key_down(KEY_S)) vector3_subtract(&move, forward);
    if (plat_key_down(KEY_D)) vector3_add(&move, right);
    if (plat_key_down(KEY_A)) vector3_subtract(&move, right);
    if (plat_key_down(KEY_E)) move.y += 1;
    if (plat_key_down(KEY_Q)) move.y -= 1;
    vector3_normalize(&move); // diagonals are not faster
    f32 speed = plat_key_down(KEY_SHIFT) ? CAMERA_SPEED * CAMERA_FAST : CAMERA_SPEED;
    vector3_add_scaled(&game_camera.position, move, speed * seconds);
}

// alpha: how far we are between the last step (0) and the next one (1)
FORCE_INLINE void game_draw(f32 alpha, f32 seconds) {
    f32 aspect = (f32)plat_screen_width() / (f32)plat_screen_height();
    Matrix4 projection = matrix4_perspective(CAMERA_FOV, aspect, 0.05f, 400.0f);
    DemoView demo_camera;
    Vector3 eye = game_camera.position;
    Matrix4 view = matrix4_look_at(eye, vector3_added(eye, game_camera_forward()), vector3_create(0, 1, 0));
    if (!demo_free_camera && demo_view(game_demo, alpha, &demo_camera)) {
        eye = demo_camera.eye;
        view = matrix4_look_at(eye, demo_camera.target, demo_camera.up);
        // so the free camera starts right here when the mouse locks
        Vector3 forward = vector3_subtracted(demo_camera.target, eye);
        game_camera.position = eye;
        game_camera.yaw = f32_atan2(forward.x, -forward.z);
        game_camera.pitch = f32_atan2(forward.y, real_sqrt(forward.x * forward.x + forward.z * forward.z));
    }

    render_begin(seconds, 0.02f, 0.02f, 0.04f);
    render_set_camera(matrix4_multiplied(projection, view), eye);

    f32 top = (f32)plat_ui_title_bottom() + 8;
    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), "fps ");
    str_append_i32(line, sizeof(line), game_hud_fps);
    str_append(line, sizeof(line), "  cpu us ");
    str_append_i32(line, sizeof(line), game_hud_cpu_us);
    str_append(line, sizeof(line), "  steps ");
    str_append_i32(line, sizeof(line), game_hud_steps);
    render_draw_text(line, 16, top, 2, 0.8f, 0.8f, 0.8f);

    demo_draw(game_demo, alpha, seconds, top + 24);
    render_end();
}

static void game_update_stats(f64 now_ms, f64 cpu_ms) {
    game_stats_frames++;
    game_stats_cpu_ms += cpu_ms;

    if (game_stats_start_ms == 0) game_stats_start_ms = now_ms;
    if (now_ms - game_stats_start_ms < 1000.0) return;

    char line[192];
    line[0] = 0;
    str_append(line, sizeof(line), "stats");
    demo_append_stat(line, sizeof(line), "fps", game_stats_frames);
    demo_append_stat(line, sizeof(line), "cpu us/frame", (i32)(game_stats_cpu_ms * 1000.0 / game_stats_frames));
    demo_append_stat(line, sizeof(line), "steps/s", game_stats_steps);
    demo_append_stat(line, sizeof(line), "gpu calls", plat_gpu_stats.calls);
    demo_append_stat(line, sizeof(line), "draws", plat_gpu_stats.draws);
    demo_append_stat(line, sizeof(line), "upload bytes", plat_gpu_stats.upload_bytes);
    demo_stats(game_demo, line, sizeof(line));
    plat_log(line);

    game_hud_fps = game_stats_frames;
    game_hud_cpu_us = (i32)(game_stats_cpu_ms * 1000.0 / game_stats_frames);
    game_hud_steps = game_stats_steps;
    game_stats_start_ms = now_ms;
    game_stats_frames = 0;
    game_stats_steps = 0;
    game_stats_cpu_ms = 0;
}

void game_frame(f64 now_ms) {
    f64 cpu_start_ms = plat_now_ms();

    if (plat_key_pressed(KEY_BRACKET_LEFT)) game_demo_picked = (game_demo + DEMO_COUNT - 1) % DEMO_COUNT;
    if (plat_key_pressed(KEY_BRACKET_RIGHT)) game_demo_picked = (game_demo + 1) % DEMO_COUNT;
    if (game_demo_picked != game_demo && game_demo_picked >= 0 && game_demo_picked < DEMO_COUNT) game_start_demo(game_demo_picked);

    if (plat_key_pressed(KEY_BACKSPACE)) game_free_camera_picked = !game_free_camera_picked;
    demo_free_camera = !demo_list[game_demo].own_camera || game_free_camera_picked;

    if (game_last_ms < 0) game_last_ms = now_ms;
    f64 frame_seconds = (now_ms - game_last_ms) / 1000.0;
    game_last_ms = now_ms;
    if (frame_seconds < 0) frame_seconds = 0;
    if (frame_seconds > GAME_MAX_FRAME_SECONDS) frame_seconds = GAME_MAX_FRAME_SECONDS;
    demo_update(game_demo, (f32)frame_seconds);

    game_accumulator += frame_seconds;
    for (i32 steps = 0; game_accumulator >= GAME_STEP && steps < GAME_MAX_STEPS; steps++) {
        demo_step(game_demo, (real)GAME_STEP);
        game_accumulator -= GAME_STEP;
        game_stats_steps++;
    }
    while (game_accumulator >= GAME_STEP) game_accumulator -= GAME_STEP; // what we could not afford, keep the blend

    game_update_camera((f32)frame_seconds);
    game_draw((f32)(game_accumulator / GAME_STEP), (f32)frame_seconds);
    game_update_stats(now_ms, plat_now_ms() - cpu_start_ms);
}
