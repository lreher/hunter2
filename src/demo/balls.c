#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "demo/demo.h"

// Erasmos Balls: balls in a box. Particles (ch 3), gravity and damping (ch 3-4), our own walls and sphere collisions.

// The world is in meters, y up, z toward the starting camera. The box spans x 0..W, y 0..H, z -D/2..D/2.
#define BALLS_BOX_W 16.0f
#define BALLS_BOX_H 9.0f
#define BALLS_BOX_D 9.0f

#define BALLS_MAX 2000 // all pairs: past this the step gets too slow (27 ms at 2000). raise after a broadphase (ch 12)
#define BALLS_SOLVER_PASSES 4 // contacts fix each other up, a few passes (warm started) keep piles from sinking
#define BALLS_MAX_CONTACTS (BALLS_MAX * 8) // a ball touches a few others and walls, plus the ones it could reach this step
#define BALLS_WALL_FRICTION 0.02f // share of the sideways speed lost each step a ball touches a wall

// tuned live from the debug panel (web/ui.js writes into these). count and sizes respawn the balls
static f32 balls_count = 80;
static f32 balls_radius_min = 0.12f; // m
static f32 balls_radius_max = 0.35f;
static f32 balls_ball_bounce = 0.6f;
static f32 balls_wall_bounce = 0.5f;
static i32 balls_alive; // how many of balls_list are in play, set at spawn
static f32 balls_spawned_count, balls_spawned_min, balls_spawned_max; // what the current balls were spawned with

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    real reach; // how far it can move this step, for the contact margin
    f32 r, g, b;
    f32 overlap_glow; // debug: seconds left to show this ball white after a drawn overlap
} Ball;

static Ball balls_list[BALLS_MAX];

static const f32 balls_palette[6][3] = {
    {0.95f, 0.35f, 0.30f}, {0.98f, 0.70f, 0.20f}, {0.45f, 0.85f, 0.40f},
    {0.30f, 0.80f, 0.90f}, {0.55f, 0.50f, 0.95f}, {0.95f, 0.50f, 0.80f},
};

// debug view (O): balls that overlap in the drawn frame turn white for a moment, so short overlaps are visible
#define BALLS_OVERLAP_LIMIT 0.01f // m
#define BALLS_OVERLAP_GLOW 0.5f   // s
static i32 balls_show_overlaps;

// the box as 6 planes, normals pointing inside (particle_contact_find_plane): floor, ceiling, left, right, back, front
static const V3 balls_wall_normals[6] = {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
static const real balls_wall_offsets[6] = {0, -BALLS_BOX_H, 0, -BALLS_BOX_W, -BALLS_BOX_D / 2, -BALLS_BOX_D / 2};

// this step's contacts and last step's (for the warm start), swapped every step
static ParticleContact balls_contact_lists[2][BALLS_MAX_CONTACTS];
static i32 balls_contact_counts[2], balls_contacts_now;

// counted per second, see balls_stats
static i32 balls_stats_contacts;
static i32 balls_stats_full; // steps where the contact list ran full
static i32 balls_stats_overlap_frames; // drawn frames where some pair overlaps more than BALLS_OVERLAP_LIMIT
static i32 balls_hud_overlap_frames;   // last second's, for the HUD

// true if a ball of this radius at this position would overlap one of the first `placed` balls
static i32 balls_overlaps_placed(V3 position, real radius, i32 placed) {
    for (i32 i = 0; i < placed; i++) {
        real gap = radius + balls_list[i].radius;
        if (v3_square_magnitude(v3_subtracted(position, balls_list[i].particle.position)) < gap * gap) return 1;
    }
    return 0;
}

static void balls_spawn(void) {
    balls_alive = (i32)balls_count;
    if (balls_alive < 1) balls_alive = 1;
    if (balls_alive > BALLS_MAX) balls_alive = BALLS_MAX;
    balls_spawned_count = balls_count;
    balls_spawned_min = balls_radius_min;
    balls_spawned_max = balls_radius_max;
    f32 lo = balls_radius_min < balls_radius_max ? balls_radius_min : balls_radius_max; // sliders may cross
    f32 hi = balls_radius_min < balls_radius_max ? balls_radius_max : balls_radius_min;

    for (i32 i = 0; i < balls_alive; i++) {
        Ball *ball = &balls_list[i];
        ball->radius = rand_f32(lo, hi);

        Particle *p = &ball->particle;
        for (i32 attempt = 0; attempt < 100; attempt++) { // keep the last try if the box is too full
            p->position = v3_create(rand_f32(ball->radius, BALLS_BOX_W - ball->radius), rand_f32(2.0f, BALLS_BOX_H - ball->radius),
                                    rand_f32(-BALLS_BOX_D / 2 + ball->radius, BALLS_BOX_D / 2 - ball->radius));
            if (!balls_overlaps_placed(p->position, ball->radius, i)) break;
        }
        p->velocity = v3_create(rand_f32(-6.0f, 6.0f), rand_f32(-3.0f, 3.0f), rand_f32(-6.0f, 6.0f));
        p->acceleration = v3_create(0, -9.8f, 0);
        p->damping = 1.0f;
        particle_set_mass(p, ball->radius * ball->radius * 10.0f);

        ball->previous_position = p->position;
        const f32 *color = balls_palette[i % 6];
        ball->r = color[0];
        ball->g = color[1];
        ball->b = color[2];
    }
    balls_contact_counts[0] = balls_contact_counts[1] = 0; // new balls, nothing to warm start from
    plat_log_i32("balls spawned", balls_alive);
}

static void balls_init(void) {
    render_add_photo_picker("photo"); // until a photo is picked, the balls stay plain
    plat_ui_slider_log("balls", 1, BALLS_MAX, 1, &balls_count);
    plat_ui_slider("min radius", 0.05f, 0.5f, 0.01f, &balls_radius_min);
    plat_ui_slider("max radius", 0.05f, 1.0f, 0.01f, &balls_radius_max);
    plat_ui_slider("ball bounce", 0, 1, 0.05f, &balls_ball_bounce);
    plat_ui_slider("wall bounce", 0, 1, 0.05f, &balls_wall_bounce);

    rand_seed(12345); // same seed, same first layout. every respawn then gets a new one
    balls_spawn();
}

static void balls_update(void) {
    i32 settings_changed = balls_count != balls_spawned_count || balls_radius_min != balls_spawned_min ||
                           balls_radius_max != balls_spawned_max;
    if (plat_key_pressed(KEY_SPACE) || settings_changed) balls_spawn();
    if (plat_key_pressed(KEY_O)) {
        balls_show_overlaps = !balls_show_overlaps;
        plat_log_i32("show overlaps", balls_show_overlaps);
    }
}

FORCE_INLINE void balls_step(real dt) {
    for (i32 i = 0; i < balls_alive; i++) {
        Ball *ball = &balls_list[i];
        ball->previous_position = ball->particle.position;
        particle_integrate_velocity(&ball->particle, dt);
        ball->reach = v3_magnitude(ball->particle.velocity) * dt;
    }

    // every pair and wall a ball could touch this step. ids (i, j) for pairs, then (i, BALLS_MAX + wall),
    // so the list comes out sorted for the warm start
    i32 before = balls_contacts_now;
    balls_contacts_now ^= 1;
    ParticleContact *list = balls_contact_lists[balls_contacts_now];
    i32 count = 0;
    for (i32 i = 0; i < balls_alive && count < BALLS_MAX_CONTACTS - 6; i++) {
        Ball *a = &balls_list[i];
        for (i32 j = i + 1; j < balls_alive && count < BALLS_MAX_CONTACTS - 6; j++) {
            Ball *b = &balls_list[j];
            if (particle_contact_find_spheres(&list[count], &a->particle, a->radius, &b->particle, b->radius,
                                              balls_ball_bounce, dt, a->reach + b->reach)) {
                list[count].id_a = i;
                list[count].id_b = j;
                count++;
            }
        }
        for (i32 wall = 0; wall < 6; wall++) {
            if (particle_contact_find_plane(&list[count], &a->particle, a->radius, balls_wall_normals[wall],
                                            balls_wall_offsets[wall], balls_wall_bounce, BALLS_WALL_FRICTION, dt, a->reach)) {
                list[count].id_a = i;
                list[count].id_b = BALLS_MAX + wall;
                count++;
            }
        }
    }
    if (count >= BALLS_MAX_CONTACTS - 6) balls_stats_full++;
    balls_contact_counts[balls_contacts_now] = count;
    balls_stats_contacts += count;

    particle_contacts_warm(list, count, balls_contact_lists[before], balls_contact_counts[before]);
    particle_contacts_solve(list, count, BALLS_SOLVER_PASSES);
    for (i32 i = 0; i < balls_alive; i++) particle_integrate_position(&balls_list[i].particle, dt);
    particle_contacts_bounce(list, count, BALLS_SOLVER_PASSES);
}

// the 12 edges of the box
static void balls_draw_box_edges(V3 lo, V3 hi, f32 r, f32 g, f32 b) {
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

FORCE_INLINE void balls_draw(f32 alpha, f32 seconds, f32 hud_top) {
    render_set_shadow_area(v3_create(0, -0.1f, -BALLS_BOX_D / 2), v3_create(BALLS_BOX_W, BALLS_BOX_H, BALLS_BOX_D / 2)); // the box, floor slab included

    // floor: a thin slab just under y = 0
    render_draw_box(v3_create(BALLS_BOX_W / 2, -0.05f, 0), v3_create(BALLS_BOX_W / 2, 0.05f, BALLS_BOX_D / 2), 0.20f, 0.24f, 0.34f);
    balls_draw_box_edges(v3_create(0, 0, -BALLS_BOX_D / 2), v3_create(BALLS_BOX_W, BALLS_BOX_H, BALLS_BOX_D / 2), 0.45f, 0.55f, 0.75f);

    static V3 positions[BALLS_MAX];
    for (i32 i = 0; i < balls_alive; i++) {
        Ball *ball = &balls_list[i];
        positions[i] = v3_lerp(ball->previous_position, ball->particle.position, alpha);
    }

    // check what is drawn, not the physics state: blending between steps can overlap even when the steps do not
    i32 overlapped = 0;
    for (i32 i = 0; i < balls_alive; i++) {
        for (i32 j = i + 1; j < balls_alive; j++) {
            real gap = balls_list[i].radius + balls_list[j].radius - BALLS_OVERLAP_LIMIT;
            if (v3_square_magnitude(v3_subtracted(positions[i], positions[j])) >= gap * gap) continue;
            balls_list[i].overlap_glow = BALLS_OVERLAP_GLOW;
            balls_list[j].overlap_glow = BALLS_OVERLAP_GLOW;
            overlapped = 1;
        }
    }
    balls_stats_overlap_frames += overlapped;

    for (i32 i = 0; i < balls_alive; i++) {
        Ball *ball = &balls_list[i];
        if (balls_show_overlaps && ball->overlap_glow > 0) {
            render_draw_sphere(positions[i], ball->radius, 1, 1, 1);
        } else {
            render_draw_sphere_photo(positions[i], ball->radius, ball->r, ball->g, ball->b);
        }
        ball->overlap_glow -= seconds;
    }

    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), balls_show_overlaps ? "overlap view (O): on" : "overlap view (O): off");
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
    line[0] = 0;
    str_append(line, sizeof(line), "overlap frames/s: ");
    str_append_i32(line, sizeof(line), balls_hud_overlap_frames);
    render_draw_text(line, 16, hud_top + 24, 2, 0.8f, 0.8f, 0.8f);
    line[0] = 0;
    str_append(line, sizeof(line), "balls: ");
    str_append_i32(line, sizeof(line), balls_alive);
    render_draw_text(line, 16, hud_top + 48, 2, 0.8f, 0.8f, 0.8f);
}

static void balls_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "contacts/s", balls_stats_contacts);
    demo_append_stat(line, cap, "overlap frames", balls_stats_overlap_frames);
    demo_append_stat(line, cap, "balls", balls_alive);
    demo_append_stat(line, cap, "contacts full", balls_stats_full);
    balls_hud_overlap_frames = balls_stats_overlap_frames;
    balls_stats_contacts = 0;
    balls_stats_full = 0;
    balls_stats_overlap_frames = 0;
}
