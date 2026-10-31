#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "game/game.h"

// Physics runs in fixed steps. Drawing happens once per browser frame
// and blends between the last two steps, so motion stays smooth on any screen.
#define GAME_STEP (1.0 / 60.0)
#define GAME_MAX_FRAME_SECONDS 0.25 // after a stall, do not try to catch up on all of it

// The world is in meters, y up, z toward the viewer. It is fitted into the screen with black bars if needed.
#define WORLD_W 16.0f
#define WORLD_H 9.0f
#define WORLD_D 1.0f // physics is 3D, the game is flat: everything stays at z = 0, the box only needs to be deeper than a ball

#define BALL_COUNT 80
#define BALL_BOUNCE 0.9f
#define WALL_BOUNCE 0.85f
#define SOLVER_PASSES 4 // collisions fix each other up, a few passes keeps piles from sinking into each other

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    f32 r, g, b;
} Ball;

static Ball game_balls[BALL_COUNT];

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

// true if a ball of this radius at this position would overlap one of the first `placed` balls
static i32 game_overlaps_placed(V3 position, real radius, i32 placed) {
    for (i32 i = 0; i < placed; i++) {
        real gap = radius + game_balls[i].radius;
        if (v3_square_magnitude(v3_subtracted(position, game_balls[i].particle.position)) < gap * gap) return 1;
    }
    return 0;
}

static void game_spawn_balls(void) {
    for (i32 i = 0; i < BALL_COUNT; i++) {
        Ball *ball = &game_balls[i];
        ball->radius = rand_f32(0.12f, 0.35f);

        Particle *p = &ball->particle;
        for (i32 attempt = 0; attempt < 100; attempt++) { // keep the last try if the box is too full
            p->position = v3_create(rand_f32(ball->radius, WORLD_W - ball->radius), rand_f32(2.0f, WORLD_H - ball->radius), 0);
            if (!game_overlaps_placed(p->position, ball->radius, i)) break;
        }
        p->velocity = v3_create(rand_f32(-6.0f, 6.0f), rand_f32(-3.0f, 3.0f), 0);
        p->acceleration = v3_create(0, -9.8f, 0);
        p->damping = 1.0f;
        particle_set_mass(p, ball->radius * ball->radius * 10.0f);

        ball->previous_position = p->position;
        const f32 *color = game_palette[i % 6];
        ball->r = color[0];
        ball->g = color[1];
        ball->b = color[2];
    }
    plat_log_i32("balls spawned", BALL_COUNT);
}

void game_init(void) {
    plat_log("hello from C");

    render_init();
    plat_log_i32("gpu calls at init", plat_gpu_stats.calls);

    rand_seed(12345); // same seed, same first layout. every respawn then gets a new one
    game_spawn_balls();
}

static void game_step(void) {
    for (i32 i = 0; i < BALL_COUNT; i++) {
        Ball *ball = &game_balls[i];
        ball->previous_position = ball->particle.position;
        particle_integrate(&ball->particle, (real)GAME_STEP);
    }

    V3 box_min = v3_create(0, 0, -WORLD_D / 2);
    V3 box_max = v3_create(WORLD_W, WORLD_H, WORLD_D / 2);
    for (i32 pass = 0; pass < SOLVER_PASSES; pass++) {
        for (i32 i = 0; i < BALL_COUNT; i++) {
            for (i32 j = i + 1; j < BALL_COUNT; j++) {
                Ball *a = &game_balls[i];
                Ball *b = &game_balls[j];
                i32 hit = particle_collide_spheres(&a->particle, a->radius, &b->particle, b->radius, BALL_BOUNCE);
                if (pass == 0) game_stats_contacts += hit;
            }
        }
        // walls last, so nothing is left outside the box
        for (i32 i = 0; i < BALL_COUNT; i++) {
            Ball *ball = &game_balls[i];
            particle_collide_box(&ball->particle, ball->radius, box_min, box_max, WALL_BOUNCE, 0.02f);
        }
    }
}

// alpha: how far we are between the last step (0) and the next one (1)
static void game_draw(f32 alpha) {
    f32 screen_w = (f32)plat_screen_width();
    f32 screen_h = (f32)plat_screen_height();
    f32 scale = screen_w / WORLD_W < screen_h / WORLD_H ? screen_w / WORLD_W : screen_h / WORLD_H;
    f32 origin_x = (screen_w - WORLD_W * scale) / 2;
    f32 origin_y = (screen_h - WORLD_H * scale) / 2;

    render_begin(0.02f, 0.02f, 0.04f);
    render_draw_rect(origin_x, origin_y, WORLD_W * scale, WORLD_H * scale, 0.08f, 0.10f, 0.18f, 1.0f);

    for (i32 i = 0; i < BALL_COUNT; i++) {
        Ball *ball = &game_balls[i];
        V3 position = v3_lerp(ball->previous_position, ball->particle.position, alpha);
        f32 x = origin_x + position.x * scale;
        f32 y = origin_y + (WORLD_H - position.y) * scale; // world y is up, screen y is down
        render_draw_circle(x, y, ball->radius * scale, ball->r, ball->g, ball->b, 1.0f);
    }
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
    game_log_stat(line, sizeof(line), "gpu calls", plat_gpu_stats.calls);
    game_log_stat(line, sizeof(line), "draws", plat_gpu_stats.draws);
    game_log_stat(line, sizeof(line), "upload bytes", plat_gpu_stats.upload_bytes);
    game_log_stat(line, sizeof(line), "balls", BALL_COUNT);
    plat_log(line);

    game_stats_start_ms = now_ms;
    game_stats_frames = 0;
    game_stats_steps = 0;
    game_stats_cpu_ms = 0;
    game_stats_contacts = 0;
}

void game_frame(f64 now_ms) {
    f64 cpu_start_ms = plat_now_ms();

    if (plat_key_pressed(KEY_SPACE)) game_spawn_balls();

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

    game_draw((f32)(game_accumulator / GAME_STEP));
    game_update_stats(now_ms, plat_now_ms() - cpu_start_ms);
}
