#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "game/game.h"

// Physics runs in fixed steps. Drawing happens once per browser frame
// and blends between the last two steps, so motion stays smooth on any screen.
#define GAME_STEP (1.0 / 60.0)
#define GAME_MAX_FRAME_SECONDS 0.25 // after a stall, do not try to catch up on all of it

// The world is in meters, y up, z toward the starting camera. The box spans x 0..W, y 0..H, z -D/2..D/2.
#define WORLD_W 16.0f
#define WORLD_H 9.0f
#define WORLD_D 9.0f

// Free fly camera, only while the mouse is locked: mouse looks, WASD moves where you look, E/Q up/down, Shift faster.
#define CAMERA_SPEED 5.0f      // m/s
#define CAMERA_FAST 4.0f       // speed multiplier with Shift
#define CAMERA_TURN 0.0025f    // radians per pixel of mouse movement
#define CAMERA_MAX_PITCH 1.55f // just short of straight up/down, so the view never flips
#define CAMERA_FOV (F32_PI / 3) // 60 degrees vertical

typedef struct {
    V3 position;
    f32 yaw;   // 0 looks down -z, positive turns right
    f32 pitch; // 0 is level, positive looks up
} Camera;

// in front of the box, a bit above, looking at its center
static Camera game_camera = {{WORLD_W / 2, 7.0f, 16.0f}, 0, -0.15f};

#define GAME_MAX_BALLS 1000

// tuned live from the debug panel (web/ui.js writes into these). count and sizes respawn the balls
static f32 game_ball_count = 80;
static f32 game_radius_min = 0.12f; // m
static f32 game_radius_max = 0.35f;
static f32 game_ball_bounce = 0.6f;
static f32 game_wall_bounce = 0.5f;
static i32 game_balls_alive; // how many of game_balls are in play, set at spawn
static f32 game_spawned_count, game_spawned_min, game_spawned_max; // what the current balls were spawned with
#define SOLVER_PASSES 4 // collisions fix each other up, a few passes keeps piles from sinking into each other

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    f32 r, g, b;
    f32 overlap_glow; // debug: seconds left to show this ball white after a drawn overlap
} Ball;

static Ball game_balls[GAME_MAX_BALLS];

static const f32 game_palette[6][3] = {
    {0.95f, 0.35f, 0.30f}, {0.98f, 0.70f, 0.20f}, {0.45f, 0.85f, 0.40f},
    {0.30f, 0.80f, 0.90f}, {0.55f, 0.50f, 0.95f}, {0.95f, 0.50f, 0.80f},
};

// time
static f64 game_last_ms = -1;
static f64 game_accumulator; // seconds of physics we still owe

// stats, logged once per second
static f64 game_stats_start_ms;
static i32 game_stats_frames;
static i32 game_stats_steps;
static f64 game_stats_cpu_ms;
static i32 game_stats_contacts;
static i32 game_stats_overlap_frames; // drawn frames where some pair overlaps more than GAME_OVERLAP_LIMIT

// debug view (O): balls that overlap in the drawn frame turn white for a moment, so short overlaps are visible
#define GAME_OVERLAP_LIMIT 0.01f // m
#define GAME_OVERLAP_GLOW 0.5f   // s
static i32 game_show_overlaps;

// last second's stats, for the on screen text
static i32 game_hud_fps;
static i32 game_hud_overlap_frames;

// true if a ball of this radius at this position would overlap one of the first `placed` balls
static i32 game_overlaps_placed(V3 position, real radius, i32 placed) {
    for (i32 i = 0; i < placed; i++) {
        real gap = radius + game_balls[i].radius;
        if (v3_square_magnitude(v3_subtracted(position, game_balls[i].particle.position)) < gap * gap) return 1;
    }
    return 0;
}

static void game_spawn_balls(void) {
    game_balls_alive = (i32)game_ball_count;
    if (game_balls_alive < 1) game_balls_alive = 1;
    if (game_balls_alive > GAME_MAX_BALLS) game_balls_alive = GAME_MAX_BALLS;
    game_spawned_count = game_ball_count;
    game_spawned_min = game_radius_min;
    game_spawned_max = game_radius_max;
    f32 lo = game_radius_min < game_radius_max ? game_radius_min : game_radius_max; // sliders may cross
    f32 hi = game_radius_min < game_radius_max ? game_radius_max : game_radius_min;

    for (i32 i = 0; i < game_balls_alive; i++) {
        Ball *ball = &game_balls[i];
        ball->radius = rand_f32(lo, hi);

        Particle *p = &ball->particle;
        for (i32 attempt = 0; attempt < 100; attempt++) { // keep the last try if the box is too full
            p->position = v3_create(rand_f32(ball->radius, WORLD_W - ball->radius), rand_f32(2.0f, WORLD_H - ball->radius),
                                    rand_f32(-WORLD_D / 2 + ball->radius, WORLD_D / 2 - ball->radius));
            if (!game_overlaps_placed(p->position, ball->radius, i)) break;
        }
        p->velocity = v3_create(rand_f32(-6.0f, 6.0f), rand_f32(-3.0f, 3.0f), rand_f32(-6.0f, 6.0f));
        p->acceleration = v3_create(0, -9.8f, 0);
        p->damping = 1.0f;
        particle_set_mass(p, ball->radius * ball->radius * 10.0f);

        ball->previous_position = p->position;
        const f32 *color = game_palette[i % 6];
        ball->r = color[0];
        ball->g = color[1];
        ball->b = color[2];
    }
    plat_log_i32("balls spawned", game_balls_alive);
}

void game_init(void) {
    plat_log("hello from C");

    render_init();
    plat_log_i32("gpu calls at init", plat_gpu_stats.calls);

    plat_ui_title("Erasmos Balls");
    plat_ui_slider("balls", 1, GAME_MAX_BALLS, 1, &game_ball_count);
    plat_ui_slider("min radius", 0.05f, 0.5f, 0.01f, &game_radius_min);
    plat_ui_slider("max radius", 0.05f, 1.0f, 0.01f, &game_radius_max);
    plat_ui_slider("ball bounce", 0, 1, 0.05f, &game_ball_bounce);
    plat_ui_slider("wall bounce", 0, 1, 0.05f, &game_wall_bounce);

    rand_seed(12345); // same seed, same first layout. every respawn then gets a new one
    game_spawn_balls();
}

static void game_step(void) {
    for (i32 i = 0; i < game_balls_alive; i++) {
        Ball *ball = &game_balls[i];
        ball->previous_position = ball->particle.position;
        particle_integrate(&ball->particle, (real)GAME_STEP);
    }

    V3 box_min = v3_create(0, 0, -WORLD_D / 2);
    V3 box_max = v3_create(WORLD_W, WORLD_H, WORLD_D / 2);
    for (i32 pass = 0; pass < SOLVER_PASSES; pass++) {
        for (i32 i = 0; i < game_balls_alive; i++) {
            for (i32 j = i + 1; j < game_balls_alive; j++) {
                Ball *a = &game_balls[i];
                Ball *b = &game_balls[j];
                i32 hit = particle_collide_spheres(&a->particle, a->radius, &b->particle, b->radius, game_ball_bounce);
                if (pass == 0) game_stats_contacts += hit;
            }
        }
        // walls last, so nothing is left outside the box
        for (i32 i = 0; i < game_balls_alive; i++) {
            Ball *ball = &game_balls[i];
            particle_collide_box(&ball->particle, ball->radius, box_min, box_max, game_wall_bounce, 0.02f);
        }
    }
}

static V3 game_camera_forward(void) {
    f32 c = f32_cos(game_camera.pitch);
    return v3_create(f32_sin(game_camera.yaw) * c, f32_sin(game_camera.pitch), -f32_cos(game_camera.yaw) * c);
}

// the camera only moves while the mouse is locked (click the canvas), so the panel can be used without it drifting
static void game_update_camera(f32 seconds) {
    if (!plat_mouse_locked()) return;

    game_camera.yaw += plat_mouse_dx() * CAMERA_TURN;
    game_camera.pitch -= plat_mouse_dy() * CAMERA_TURN;
    if (game_camera.pitch > CAMERA_MAX_PITCH) game_camera.pitch = CAMERA_MAX_PITCH;
    if (game_camera.pitch < -CAMERA_MAX_PITCH) game_camera.pitch = -CAMERA_MAX_PITCH;

    V3 forward = game_camera_forward();
    V3 right = v3_create(f32_cos(game_camera.yaw), 0, f32_sin(game_camera.yaw));
    V3 move = v3_zero();
    if (plat_key_down(KEY_W)) v3_add(&move, forward);
    if (plat_key_down(KEY_S)) v3_subtract(&move, forward);
    if (plat_key_down(KEY_D)) v3_add(&move, right);
    if (plat_key_down(KEY_A)) v3_subtract(&move, right);
    if (plat_key_down(KEY_E)) move.y += 1;
    if (plat_key_down(KEY_Q)) move.y -= 1;
    v3_normalize(&move); // diagonals are not faster
    f32 speed = plat_key_down(KEY_SHIFT) ? CAMERA_SPEED * CAMERA_FAST : CAMERA_SPEED;
    v3_add_scaled(&game_camera.position, move, speed * seconds);
}

// the 12 edges of the box
static void game_draw_box_edges(V3 lo, V3 hi, f32 r, f32 g, f32 b) {
    for (i32 i = 0; i < 4; i++) {
        f32 u = (i & 1) ? hi.y : lo.y;
        f32 v = (i & 2) ? hi.z : lo.z;
        render_draw_line(v3_create(lo.x, u, v), v3_create(hi.x, u, v), r, g, b); // along x
        u = (i & 1) ? hi.x : lo.x;
        render_draw_line(v3_create(u, lo.y, v), v3_create(u, hi.y, v), r, g, b); // along y
        v = (i & 2) ? hi.y : lo.y;
        render_draw_line(v3_create(u, v, lo.z), v3_create(u, v, hi.z), r, g, b); // along z
    }
}

// alpha: how far we are between the last step (0) and the next one (1)
static void game_draw(f32 alpha, f32 seconds) {
    f32 aspect = (f32)plat_screen_width() / (f32)plat_screen_height();
    M4 projection = m4_perspective(CAMERA_FOV, aspect, 0.05f, 200.0f);
    V3 eye = game_camera.position;
    M4 view = m4_look_at(eye, v3_added(eye, game_camera_forward()), v3_create(0, 1, 0));

    render_begin(0.02f, 0.02f, 0.04f);
    render_set_camera(m4_multiplied(projection, view), eye);

    // floor: a thin slab just under y = 0
    render_draw_box(v3_create(WORLD_W / 2, -0.05f, 0), v3_create(WORLD_W / 2, 0.05f, WORLD_D / 2), 0.20f, 0.24f, 0.34f);
    game_draw_box_edges(v3_create(0, 0, -WORLD_D / 2), v3_create(WORLD_W, WORLD_H, WORLD_D / 2), 0.45f, 0.55f, 0.75f);

    static V3 positions[GAME_MAX_BALLS];
    for (i32 i = 0; i < game_balls_alive; i++) {
        Ball *ball = &game_balls[i];
        positions[i] = v3_lerp(ball->previous_position, ball->particle.position, alpha);
    }

    // check what is drawn, not the physics state: blending between steps can overlap even when the steps do not
    i32 overlapped = 0;
    for (i32 i = 0; i < game_balls_alive; i++) {
        for (i32 j = i + 1; j < game_balls_alive; j++) {
            real gap = game_balls[i].radius + game_balls[j].radius - GAME_OVERLAP_LIMIT;
            if (v3_square_magnitude(v3_subtracted(positions[i], positions[j])) >= gap * gap) continue;
            game_balls[i].overlap_glow = GAME_OVERLAP_GLOW;
            game_balls[j].overlap_glow = GAME_OVERLAP_GLOW;
            overlapped = 1;
        }
    }
    game_stats_overlap_frames += overlapped;

    for (i32 i = 0; i < game_balls_alive; i++) {
        Ball *ball = &game_balls[i];
        if (game_show_overlaps && ball->overlap_glow > 0) {
            render_draw_sphere(positions[i], ball->radius, 1, 1, 1);
        } else {
            render_draw_sphere(positions[i], ball->radius, ball->r, ball->g, ball->b);
        }
        ball->overlap_glow -= seconds;
    }

    f32 top = (f32)plat_ui_title_bottom() + 8;
    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), "fps ");
    str_append_i32(line, sizeof(line), game_hud_fps);
    render_draw_text(line, 16, top, 2, 0.8f, 0.8f, 0.8f);
    line[0] = 0;
    str_append(line, sizeof(line), game_show_overlaps ? "overlap view (O): on" : "overlap view (O): off");
    render_draw_text(line, 16, top + 24, 2, 0.8f, 0.8f, 0.8f);
    line[0] = 0;
    str_append(line, sizeof(line), "overlap frames/s: ");
    str_append_i32(line, sizeof(line), game_hud_overlap_frames);
    render_draw_text(line, 16, top + 48, 2, 0.8f, 0.8f, 0.8f);
    line[0] = 0;
    str_append(line, sizeof(line), "balls: ");
    str_append_i32(line, sizeof(line), game_balls_alive);
    render_draw_text(line, 16, top + 72, 2, 0.8f, 0.8f, 0.8f);
    render_end();
}

static void game_log_stat(char *line, i32 cap, const char *label, i32 value) {
    str_append(line, cap, " | ");
    str_append(line, cap, label);
    str_append(line, cap, " ");
    str_append_i32(line, cap, value);
}

static void game_update_stats(f64 now_ms, f64 cpu_ms) {
    game_stats_frames++;
    game_stats_cpu_ms += cpu_ms;

    if (game_stats_start_ms == 0) game_stats_start_ms = now_ms;
    if (now_ms - game_stats_start_ms < 1000.0) return;

    char line[192];
    line[0] = 0;
    str_append(line, sizeof(line), "stats");
    game_log_stat(line, sizeof(line), "fps", game_stats_frames);
    game_log_stat(line, sizeof(line), "cpu us/frame", (i32)(game_stats_cpu_ms * 1000.0 / game_stats_frames));
    game_log_stat(line, sizeof(line), "steps/s", game_stats_steps);
    game_log_stat(line, sizeof(line), "contacts/s", game_stats_contacts);
    game_log_stat(line, sizeof(line), "overlap frames", game_stats_overlap_frames);
    game_log_stat(line, sizeof(line), "gpu calls", plat_gpu_stats.calls);
    game_log_stat(line, sizeof(line), "draws", plat_gpu_stats.draws);
    game_log_stat(line, sizeof(line), "upload bytes", plat_gpu_stats.upload_bytes);
    game_log_stat(line, sizeof(line), "balls", game_balls_alive);
    plat_log(line);

    game_hud_fps = game_stats_frames;
    game_hud_overlap_frames = game_stats_overlap_frames;
    game_stats_start_ms = now_ms;
    game_stats_frames = 0;
    game_stats_steps = 0;
    game_stats_cpu_ms = 0;
    game_stats_contacts = 0;
    game_stats_overlap_frames = 0;
}

void game_frame(f64 now_ms) {
    f64 cpu_start_ms = plat_now_ms();

    i32 settings_changed = game_ball_count != game_spawned_count || game_radius_min != game_spawned_min ||
                           game_radius_max != game_spawned_max;
    if (plat_key_pressed(KEY_SPACE) || settings_changed) game_spawn_balls();
    if (plat_key_pressed(KEY_O)) {
        game_show_overlaps = !game_show_overlaps;
        plat_log_i32("show overlaps", game_show_overlaps);
    }

    if (game_last_ms < 0) game_last_ms = now_ms;
    f64 frame_seconds = (now_ms - game_last_ms) / 1000.0;
    game_last_ms = now_ms;
    if (frame_seconds < 0) frame_seconds = 0;
    if (frame_seconds > GAME_MAX_FRAME_SECONDS) frame_seconds = GAME_MAX_FRAME_SECONDS;

    game_accumulator += frame_seconds;
    while (game_accumulator >= GAME_STEP) {
        game_step();
        game_accumulator -= GAME_STEP;
        game_stats_steps++;
    }

    game_update_camera((f32)frame_seconds);
    game_draw((f32)(game_accumulator / GAME_STEP), (f32)frame_seconds);
    game_update_stats(now_ms, plat_now_ms() - cpu_start_ms);
}
