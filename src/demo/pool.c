#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "demo/demo.h"

// Ball Pool (book 6.2.4): the balls box with water in it. Each ball has its own FORCE_BUOYANCY (depth = its radius,
// volume = its sphere), plus gravity, our walls and sphere collisions.
// Mass grows with r^2 like Erasmos Balls, so density goes as 1 / r: small balls sink, big ones float.
// The water's level, density and drag act right away. Space respawns, at rest, anywhere in the box.

#define POOL_BOX_W 16.0f
#define POOL_BOX_H 9.0f
#define POOL_BOX_D 9.0f
#define POOL_MAX 1000 // all pairs
#define POOL_SOLVER_PASSES 4
#define POOL_MAX_CONTACTS (POOL_MAX * 8)
#define POOL_GRAVITY 9.8f
// kg per m^2: mass = this * r^2. Erasmos uses 10, which makes every ball under 5% of water's density, so all
// float on top. This puts the line at r 0.3 m in fresh water: density = 3 * this / (4 pi r)
#define POOL_MASS_PER_R2 1257.0f
#define POOL_BALL_BOUNCE 0.3f
#define POOL_WALL_BOUNCE 0.3f
#define POOL_WALL_FRICTION 0.02f

static f32 pool_count = 80;
static f32 pool_radius_min = 0.1f; // m
static f32 pool_radius_max = 0.6f;
static f32 pool_water_level = 4.5f;   // m, live
static f32 pool_water_density = 1000; // kg/m^3, live
static f32 pool_water_drag = 2;       // per second, live. drag = this * mass, the same slowdown for every ball
static i32 pool_alive;
static f32 pool_spawned_count, pool_spawned_min, pool_spawned_max;

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    real reach; // how far it can move this step, for the contact margin
} PoolBall;

static PoolBall pool_balls[POOL_MAX];
static ForceGen pool_buoyancy[POOL_MAX]; // one per ball: it holds the ball's size
static ForceReg pool_regs[POOL_MAX];
static ForceRegistry pool_forces = {pool_regs, 0, POOL_MAX};

// the box as 6 planes, normals pointing inside: floor, ceiling, left, right, back, front
static const V3 pool_wall_normals[6] = {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
static const real pool_wall_offsets[6] = {0, -POOL_BOX_H, 0, -POOL_BOX_W, -POOL_BOX_D / 2, -POOL_BOX_D / 2};

// this step's contacts and last step's (for the warm start), swapped every step
static ParticleContact pool_contact_lists[2][POOL_MAX_CONTACTS];
static i32 pool_contact_counts[2], pool_contacts_now;
static i32 pool_stats_contacts; // per second, see pool_stats
static i32 pool_stats_full;     // steps per second where the contact list ran full

static i32 pool_overlaps_placed(V3 position, real radius, i32 placed) {
    for (i32 i = 0; i < placed; i++) {
        real gap = radius + pool_balls[i].radius;
        if (v3_square_magnitude(v3_subtracted(position, pool_balls[i].particle.position)) < gap * gap) return 1;
    }
    return 0;
}

// writes the water sliders into every ball's generator. the registry holds pointers, so nothing else changes
static void pool_set_water(void) {
    for (i32 i = 0; i < pool_alive; i++) {
        ForceGen *b = &pool_buoyancy[i];
        b->buoyancy.water_height = pool_water_level;
        b->buoyancy.density = pool_water_density;
        b->buoyancy.drag = pool_water_drag * particle_get_mass(&pool_balls[i].particle);
    }
}

static void pool_spawn(void) {
    pool_alive = (i32)pool_count;
    if (pool_alive < 1) pool_alive = 1;
    if (pool_alive > POOL_MAX) pool_alive = POOL_MAX;
    pool_spawned_count = pool_count;
    pool_spawned_min = pool_radius_min;
    pool_spawned_max = pool_radius_max;
    f32 lo = pool_radius_min < pool_radius_max ? pool_radius_min : pool_radius_max; // sliders may cross
    f32 hi = pool_radius_min < pool_radius_max ? pool_radius_max : pool_radius_min;

    force_registry_clear(&pool_forces);
    for (i32 i = 0; i < pool_alive; i++) {
        PoolBall *ball = &pool_balls[i];
        real r = rand_f32(lo, hi);
        ball->radius = r;
        Particle *p = &ball->particle;
        for (i32 attempt = 0; attempt < 100; attempt++) { // keep the last try if the box is too full
            p->position = v3_create(rand_f32(r, POOL_BOX_W - r), rand_f32(r, POOL_BOX_H - r),
                                    rand_f32(-POOL_BOX_D / 2 + r, POOL_BOX_D / 2 - r));
            if (!pool_overlaps_placed(p->position, r, i)) break;
        }
        p->velocity = v3_zero();
        p->acceleration = v3_create(0, -POOL_GRAVITY, 0);
        p->force_accum = v3_zero();
        p->damping = 1;
        particle_set_mass(p, POOL_MASS_PER_R2 * r * r);
        ball->previous_position = p->position;

        real volume = 4.0f / 3.0f * F32_PI * r * r * r;
        pool_buoyancy[i] = force_buoyancy(pool_water_level, r, volume, pool_water_density, POOL_GRAVITY, 0);
        force_registry_add(&pool_forces, p, &pool_buoyancy[i]);
    }
    pool_set_water();
    pool_contact_counts[0] = pool_contact_counts[1] = 0; // new balls, nothing to warm start from
    plat_log_i32("pool spawned", pool_alive);
}

static void pool_init(void) {
    plat_ui_slider_log("balls", 1, POOL_MAX, 1, &pool_count);
    plat_ui_slider("min radius", 0.05f, 0.5f, 0.01f, &pool_radius_min);
    plat_ui_slider("max radius", 0.05f, 1.0f, 0.01f, &pool_radius_max);
    plat_ui_slider("water level m", 0, POOL_BOX_H, 0.1f, &pool_water_level);
    plat_ui_slider_log("water kg/m3", 100, 3000, 10, &pool_water_density);
    plat_ui_slider("water drag /s", 0, 10, 0.1f, &pool_water_drag);

    rand_seed(12345);
    pool_spawn();
}

static void pool_update(void) {
    i32 settings_changed = pool_count != pool_spawned_count || pool_radius_min != pool_spawned_min ||
                           pool_radius_max != pool_spawned_max;
    if (plat_key_pressed(KEY_SPACE) || settings_changed) pool_spawn();
    pool_set_water();
}

FORCE_INLINE void pool_step(real dt) {
    force_registry_update(&pool_forces, dt);
    for (i32 i = 0; i < pool_alive; i++) {
        PoolBall *ball = &pool_balls[i];
        ball->previous_position = ball->particle.position;
        particle_integrate_velocity(&ball->particle, dt);
        ball->reach = v3_magnitude(ball->particle.velocity) * dt;
    }

    // every pair that could touch this step, then the walls (ids after every ball). sorted for the warm start
    i32 before = pool_contacts_now;
    pool_contacts_now ^= 1;
    ParticleContact *list = pool_contact_lists[pool_contacts_now];
    i32 count = 0;
    for (i32 i = 0; i < pool_alive && count < POOL_MAX_CONTACTS - 6; i++) {
        PoolBall *a = &pool_balls[i];
        for (i32 j = i + 1; j < pool_alive && count < POOL_MAX_CONTACTS - 6; j++) {
            PoolBall *b = &pool_balls[j];
            if (particle_contact_find_spheres(&list[count], &a->particle, a->radius, &b->particle, b->radius,
                                              POOL_BALL_BOUNCE, dt, a->reach + b->reach)) {
                list[count].id_a = i;
                list[count].id_b = j;
                count++;
            }
        }
        for (i32 wall = 0; wall < 6; wall++) {
            if (particle_contact_find_plane(&list[count], &a->particle, a->radius, pool_wall_normals[wall],
                                            pool_wall_offsets[wall], POOL_WALL_BOUNCE, POOL_WALL_FRICTION, dt, a->reach)) {
                list[count].id_a = i;
                list[count].id_b = POOL_MAX + wall;
                count++;
            }
        }
    }
    if (count >= POOL_MAX_CONTACTS - 6) pool_stats_full++;
    pool_contact_counts[pool_contacts_now] = count;
    pool_stats_contacts += count;

    particle_contacts_warm(list, count, pool_contact_lists[before], pool_contact_counts[before]);
    particle_contacts_solve(list, count, POOL_SOLVER_PASSES);
    for (i32 i = 0; i < pool_alive; i++) particle_integrate_position(&pool_balls[i].particle, dt);
    particle_contacts_bounce(list, count, POOL_SOLVER_PASSES);
}

// the 12 edges of the box
static void pool_draw_box_edges(V3 lo, V3 hi, f32 r, f32 g, f32 b) {
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

FORCE_INLINE void pool_draw(f32 alpha, f32 seconds, f32 hud_top) {
    (void)seconds;
    // balls: yellow if lighter than the water (floats), red if heavier (sinks). bluer the more of it is under water
    for (i32 i = 0; i < pool_alive; i++) {
        PoolBall *ball = &pool_balls[i];
        V3 c = v3_lerp(ball->previous_position, ball->particle.position, alpha);
        real r = ball->radius;
        real density = POOL_MASS_PER_R2 * r * r / (4.0f / 3.0f * F32_PI * r * r * r);
        f32 red = 0.98f, green = 0.78f, blue = 0.25f;
        if (density >= pool_water_density) red = 0.80f, green = 0.28f, blue = 0.30f;
        real under = (pool_water_level - c.y + r) / (2 * r);
        if (under < 0) under = 0;
        if (under > 1) under = 1;
        red *= 1 - 0.55f * under;
        green *= 1 - 0.30f * under;
        blue += (1 - blue) * under;
        render_draw_sphere(c, r, red, green, blue);
    }

    // water: a grid on the surface, the waterline brighter on the walls
    f32 y = pool_water_level;
    for (i32 x = 1; x < (i32)POOL_BOX_W; x++)
        render_draw_line(v3_create(x, y, -POOL_BOX_D / 2), v3_create(x, y, POOL_BOX_D / 2), 0.25f, 0.45f, 0.80f);
    for (i32 z = 1; z < (i32)POOL_BOX_D; z++)
        render_draw_line(v3_create(0, y, z - POOL_BOX_D / 2), v3_create(POOL_BOX_W, y, z - POOL_BOX_D / 2), 0.25f, 0.45f, 0.80f);
    pool_draw_box_edges(v3_create(0, y, -POOL_BOX_D / 2), v3_create(POOL_BOX_W, y, POOL_BOX_D / 2), 0.5f, 0.75f, 1.0f);

    render_set_shadow_area(v3_create(0, -0.1f, -POOL_BOX_D / 2), v3_create(POOL_BOX_W, POOL_BOX_H, POOL_BOX_D / 2));
    render_draw_box(v3_create(POOL_BOX_W / 2, -0.05f, 0), v3_create(POOL_BOX_W / 2, 0.05f, POOL_BOX_D / 2), 0.20f, 0.24f, 0.34f);
    pool_draw_box_edges(v3_create(0, 0, -POOL_BOX_D / 2), v3_create(POOL_BOX_W, POOL_BOX_H, POOL_BOX_D / 2), 0.45f, 0.55f, 0.75f);

    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), "balls: ");
    str_append_i32(line, sizeof(line), pool_alive);
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
    // density = 3 k / (4 pi r) is below the water's when r > 3 k / (4 pi water)
    line[0] = 0;
    str_append(line, sizeof(line), "floats if radius > ");
    str_append_i32(line, sizeof(line), (i32)(300 * POOL_MASS_PER_R2 / (4 * F32_PI * pool_water_density) + 0.5f));
    str_append(line, sizeof(line), " cm");
    render_draw_text(line, 16, hud_top + 24, 2, 0.8f, 0.8f, 0.8f);
}

static void pool_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "contacts/s", pool_stats_contacts);
    demo_append_stat(line, cap, "balls", pool_alive);
    demo_append_stat(line, cap, "contacts full", pool_stats_full);
    pool_stats_contacts = 0;
    pool_stats_full = 0;
}
