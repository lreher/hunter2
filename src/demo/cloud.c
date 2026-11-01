#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "demo/demo.h"

// Gravity Cloud: bodies pulling on each other with Newton's gravity (book 3.2, eq 3.3) through force generators (ch 5),
// plus our sphere collisions. One material for all, so mass grows with radius^3 and big bodies pull harder.
// No ground gravity, no box, no damping. Starts centered on the origin, center of mass at rest.

#define CLOUD_MAX 500 // gravity and collisions are both all pairs
#define CLOUD_SOLVER_PASSES 4
#define CLOUD_MAX_CONTACTS (CLOUD_MAX * 16) // a body touches a few others, plus the ones it could reach this step
#define CLOUD_RADIUS 6.0f    // m, start cloud
#define CLOUD_DENSITY 1000.0f // mass = density * r^3, so r 0.1 is 1 kg and r 1 is 1000 kg
#define CLOUD_G_UNIT 0.001f  // the G slider is in these, so it shows readable numbers
#define CLOUD_BLAST_TIME 0.1f // s, how long the explode push lasts

// how the bodies start moving, all scaled by cloud_speed
enum { CLOUD_EXPLODE, CLOUD_RANDOM, CLOUD_SPIN };

// tuned live from the debug panel. count, sizes, start and speed respawn, G and bounce act right away
static f32 cloud_count = 150;
static f32 cloud_radius_min = 0.1f; // m
static f32 cloud_radius_max = 0.6f;
static f32 cloud_g = 2;             // in CLOUD_G_UNIT
static i32 cloud_start = CLOUD_RANDOM;
static f32 cloud_speed = 1;         // m/s. random: top speed, spin: speed at the edge, explode: average. 0 = all at rest
static f32 cloud_bounce = 0.3f;
static i32 cloud_alive;
static f32 cloud_spawned_count, cloud_spawned_min, cloud_spawned_max, cloud_spawned_speed, cloud_applied_g;
static i32 cloud_spawned_start;

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    real reach; // how far it can move this step, for the contact margin
    f32 r, g, b;
} Body;

static Body cloud_bodies[CLOUD_MAX];

// cloud_pulls[j] pulls toward body j and is registered on every other body: n(n-1) pairs
static ForceGen cloud_pulls[CLOUD_MAX];
static ForceReg cloud_regs[CLOUD_MAX * (CLOUD_MAX - 1)];
static ForceRegistry cloud_forces = {cloud_regs, 0, CLOUD_MAX * (CLOUD_MAX - 1)};

// explode: a blast per body (its force depends on the body's size), in their own registry so it can be emptied
// when the push is over
static ForceGen cloud_blast[CLOUD_MAX];
static ForceReg cloud_blast_regs[CLOUD_MAX];
static ForceRegistry cloud_blasts = {cloud_blast_regs, 0, CLOUD_MAX};
static real cloud_blast_left; // s

static i32 cloud_stats_contacts; // per second, see cloud_stats
static i32 cloud_stats_full;     // steps per second where the contact list ran full

// this step's contacts and last step's (for the warm start), swapped every step
static ParticleContact cloud_contact_lists[2][CLOUD_MAX_CONTACTS];
static i32 cloud_contact_counts[2], cloud_contacts_now;

static i32 cloud_overlaps_placed(V3 position, real radius, i32 placed) {
    for (i32 i = 0; i < placed; i++) {
        real gap = radius + cloud_bodies[i].radius;
        if (v3_square_magnitude(v3_subtracted(position, cloud_bodies[i].particle.position)) < gap * gap) return 1;
    }
    return 0;
}

static void cloud_spawn(void) {
    cloud_alive = (i32)cloud_count;
    if (cloud_alive < 1) cloud_alive = 1;
    if (cloud_alive > CLOUD_MAX) cloud_alive = CLOUD_MAX;
    cloud_spawned_count = cloud_count;
    cloud_spawned_min = cloud_radius_min;
    cloud_spawned_max = cloud_radius_max;
    cloud_spawned_speed = cloud_speed;
    cloud_spawned_start = cloud_start;
    f32 lo = cloud_radius_min < cloud_radius_max ? cloud_radius_min : cloud_radius_max; // sliders may cross
    f32 hi = cloud_radius_min < cloud_radius_max ? cloud_radius_max : cloud_radius_min;

    real mass_total = 0;
    V3 mass_center = v3_zero();
    for (i32 i = 0; i < cloud_alive; i++) {
        Body *body = &cloud_bodies[i];
        body->radius = rand_f32(lo, hi);
        Particle *p = &body->particle;
        real reach = CLOUD_RADIUS - body->radius;
        for (i32 attempt = 0; attempt < 100; attempt++) { // random spot inside the sphere, not on another body
            do p->position = v3_create(rand_f32(-reach, reach), rand_f32(-reach, reach), rand_f32(-reach, reach));
            while (v3_square_magnitude(p->position) > reach * reach);
            if (!cloud_overlaps_placed(p->position, body->radius, i)) break;
        }
        p->acceleration = v3_zero();
        p->force_accum = v3_zero();
        p->damping = 1;
        real mass = CLOUD_DENSITY * body->radius * body->radius * body->radius;
        particle_set_mass(p, mass);
        mass_total += mass;
        v3_add_scaled(&mass_center, p->position, mass);

        f32 t = hi > lo ? (body->radius - lo) / (hi - lo) : 0; // small blue, big orange
        body->r = 0.30f + 0.68f * t;
        body->g = 0.55f;
        body->b = 0.95f - 0.75f * t;
    }

    // center of mass to the origin. spin grows with the distance r from the center, cloud_speed at the edge.
    // in a uniform ball the pull at r also grows with r, so at sqrt(G M / R) every body would be in a circular orbit
    v3_multiply(&mass_center, 1 / mass_total);
    real edge_speed = cloud_speed;
    real per_meter = edge_speed / CLOUD_RADIUS; // for spin
    V3 momentum = v3_zero();
    for (i32 i = 0; i < cloud_alive; i++) {
        Particle *p = &cloud_bodies[i].particle;
        v3_subtract(&p->position, mass_center);
        V3 r = p->position;
        switch (cloud_start) {
        case CLOUD_EXPLODE: p->velocity = v3_zero(); break;                                           // at rest, the blast pushes
        case CLOUD_RANDOM: p->velocity = v3_multiplied(rand_unit_v3(), rand_f32(0, edge_speed)); break; // any way, up to edge_speed
        case CLOUD_SPIN: p->velocity = v3_create(per_meter * r.z, 0, -per_meter * r.x); break;      // around the y axis
        }
        v3_add_scaled(&momentum, p->velocity, particle_get_mass(p));
        cloud_bodies[i].previous_position = p->position;
    }
    V3 drift = v3_multiplied(momentum, 1 / mass_total); // so the cloud as a whole stays put
    for (i32 i = 0; i < cloud_alive; i++) v3_subtract(&cloud_bodies[i].particle.velocity, drift);

    force_registry_clear(&cloud_forces);
    for (i32 j = 0; j < cloud_alive; j++) cloud_pulls[j] = force_attract(&cloud_bodies[j].particle, cloud_g * CLOUD_G_UNIT);
    for (i32 i = 0; i < cloud_alive; i++) {
        for (i32 j = 0; j < cloud_alive; j++) {
            if (i != j) force_registry_add(&cloud_forces, &cloud_bodies[i].particle, &cloud_pulls[j]);
        }
    }
    cloud_applied_g = cloud_g;

    // the blast hits each body by its area, F = k r^2. with m = density r^3 that gives dv = F t / m = k t / (density r):
    // big ones still slower, but only by their size, not their mass. k is picked so bodies leave at edge_speed on average
    force_registry_clear(&cloud_blasts);
    cloud_blast_left = 0;
    if (cloud_start == CLOUD_EXPLODE) {
        real inverse_radius_sum = 0;
        for (i32 i = 0; i < cloud_alive; i++) inverse_radius_sum += 1 / cloud_bodies[i].radius;
        real k = edge_speed * CLOUD_DENSITY * cloud_alive / (CLOUD_BLAST_TIME * inverse_radius_sum);
        for (i32 i = 0; i < cloud_alive; i++) {
            real r = cloud_bodies[i].radius;
            cloud_blast[i] = force_explode(v3_zero(), k * r * r);
            force_registry_add(&cloud_blasts, &cloud_bodies[i].particle, &cloud_blast[i]);
        }
        cloud_blast_left = CLOUD_BLAST_TIME;
    }
    cloud_contact_counts[0] = cloud_contact_counts[1] = 0; // new bodies, nothing to warm start from
    plat_log_i32("cloud spawned", cloud_alive);
}

static void cloud_init(void) {
    plat_ui_slider_log("bodies", 1, CLOUD_MAX, 1, &cloud_count);
    plat_ui_slider("min radius", 0.05f, 0.5f, 0.01f, &cloud_radius_min);
    plat_ui_slider("max radius", 0.05f, 1.0f, 0.01f, &cloud_radius_max);
    plat_ui_slider_log("G (x 0.001)", 0.1f, 50, 0.01f, &cloud_g);
    plat_ui_choice("start", "Explode|Random|Spin", &cloud_start);
    plat_ui_slider("speed m/s", 0, 20, 0.1f, &cloud_speed);
    plat_ui_slider("bounce", 0, 1, 0.05f, &cloud_bounce);

    rand_seed(12345); // same seed, same first cloud. every respawn then gets a new one
    cloud_spawn();
}

static void cloud_update(void) {
    i32 settings_changed = cloud_count != cloud_spawned_count || cloud_radius_min != cloud_spawned_min ||
                           cloud_radius_max != cloud_spawned_max || cloud_speed != cloud_spawned_speed ||
                           cloud_start != cloud_spawned_start;
    if (plat_key_pressed(KEY_SPACE) || settings_changed) cloud_spawn();
    if (cloud_g != cloud_applied_g) {
        for (i32 j = 0; j < cloud_alive; j++) cloud_pulls[j].attract.g = cloud_g * CLOUD_G_UNIT;
        cloud_applied_g = cloud_g;
    }
}

FORCE_INLINE void cloud_step(real dt) {
    force_registry_update(&cloud_forces, dt);
    if (cloud_blasts.count > 0) {
        force_registry_update(&cloud_blasts, dt);
        cloud_blast_left -= dt;
        if (cloud_blast_left < dt / 2) force_registry_clear(&cloud_blasts); // half a step: float error never adds a step
    }
    for (i32 i = 0; i < cloud_alive; i++) {
        Body *body = &cloud_bodies[i];
        body->previous_position = body->particle.position;
        particle_integrate_velocity(&body->particle, dt);
        body->reach = v3_magnitude(body->particle.velocity) * dt;
    }

    // every pair that could touch this step. found in (i, j) order, so the list is sorted for the warm start
    i32 before = cloud_contacts_now;
    cloud_contacts_now ^= 1;
    ParticleContact *list = cloud_contact_lists[cloud_contacts_now];
    i32 count = 0;
    for (i32 i = 0; i < cloud_alive && count < CLOUD_MAX_CONTACTS; i++) {
        for (i32 j = i + 1; j < cloud_alive && count < CLOUD_MAX_CONTACTS; j++) {
            Body *a = &cloud_bodies[i];
            Body *b = &cloud_bodies[j];
            if (particle_contact_find_spheres(&list[count], &a->particle, a->radius, &b->particle, b->radius,
                                              cloud_bounce, dt, a->reach + b->reach)) {
                list[count].id_a = i;
                list[count].id_b = j;
                count++;
            }
        }
    }
    if (count == CLOUD_MAX_CONTACTS) cloud_stats_full++;
    cloud_contact_counts[cloud_contacts_now] = count;
    cloud_stats_contacts += count;

    particle_contacts_warm(list, count, cloud_contact_lists[before], cloud_contact_counts[before]);
    particle_contacts_solve(list, count, CLOUD_SOLVER_PASSES);
    for (i32 i = 0; i < cloud_alive; i++) particle_integrate_position(&cloud_bodies[i].particle, dt);
    particle_contacts_bounce(list, count, CLOUD_SOLVER_PASSES);
}

FORCE_INLINE void cloud_draw(f32 alpha, f32 seconds, f32 hud_top) {
    (void)seconds;
    V3 lo = v3_create(REAL_MAX, REAL_MAX, REAL_MAX), hi = v3_create(-REAL_MAX, -REAL_MAX, -REAL_MAX);
    for (i32 i = 0; i < cloud_alive; i++) {
        Body *body = &cloud_bodies[i];
        V3 c = v3_lerp(body->previous_position, body->particle.position, alpha);
        render_draw_sphere(c, body->radius, body->r, body->g, body->b);
        real r = body->radius;
        if (c.x - r < lo.x) lo.x = c.x - r;
        if (c.y - r < lo.y) lo.y = c.y - r;
        if (c.z - r < lo.z) lo.z = c.z - r;
        if (c.x + r > hi.x) hi.x = c.x + r;
        if (c.y + r > hi.y) hi.y = c.y + r;
        if (c.z + r > hi.z) hi.z = c.z + r;
    }
    // sun shadows over the bodies. bodies thrown far out would make the area huge and the shadows blurry, so clamp it
    real limit = 30;
    if (lo.x < -limit) lo.x = -limit;
    if (lo.y < -limit) lo.y = -limit;
    if (lo.z < -limit) lo.z = -limit;
    if (hi.x > limit) hi.x = limit;
    if (hi.y > limit) hi.y = limit;
    if (hi.z > limit) hi.z = limit;
    render_set_shadow_area(lo, hi);

    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), "bodies: ");
    str_append_i32(line, sizeof(line), cloud_alive);
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
}

static void cloud_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "contacts/s", cloud_stats_contacts);
    demo_append_stat(line, cap, "bodies", cloud_alive);
    demo_append_stat(line, cap, "contacts full", cloud_stats_full);
    cloud_stats_contacts = 0;
    cloud_stats_full = 0;
}
