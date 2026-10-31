#include "base/base.h"
#include "phys/phys.h"

void particle_integrate(Particle *p, real dt) {
    v3_add_scaled(&p->position, p->velocity, dt);

    V3 acceleration = p->acceleration;
    v3_add_scaled(&acceleration, p->force_accum, p->inverse_mass);
    v3_add_scaled(&p->velocity, acceleration, dt);

    v3_multiply(&p->velocity, p->damping);
    particle_clear_force_accum(p);
}

void particle_set_mass(Particle *p, real mass) {
    if (mass <= 0) __builtin_trap(); // zero mass would mean infinite acceleration. no assert without libc, so fail loudly
    p->inverse_mass = 1.0f / mass;
}

void particle_set_inverse_mass(Particle *p, real inverse_mass) {
    p->inverse_mass = inverse_mass;
}

real particle_get_mass(Particle *p) {
    return p->inverse_mass == 0 ? REAL_MAX : 1.0f / p->inverse_mass;
}

i32 particle_has_finite_mass(Particle *p) {
    return p->inverse_mass > 0;
}

void particle_add_force(Particle *p, V3 force) {
    v3_add(&p->force_accum, force);
}

void particle_clear_force_accum(Particle *p) {
    p->force_accum = v3_zero();
}

real particle_bounce_factor(real impact_speed, real restitution) {
    return impact_speed < PHYS_REST_SPEED ? 0 : restitution;
}

// one axis of the box. side_a and side_b are the velocity on the other two axes, they keep `keep` of it on impact
static void particle_collide_box_axis(real *position, real *velocity, real *side_a, real *side_b,
                                      real lo, real hi, real restitution, real keep) {
    if (*position < lo) {
        *position = lo;
        *velocity = -*velocity * particle_bounce_factor(-*velocity, restitution);
    } else if (*position > hi) {
        *position = hi;
        *velocity = -*velocity * particle_bounce_factor(*velocity, restitution);
    } else {
        return;
    }
    *side_a *= keep;
    *side_b *= keep;
}

void particle_collide_box(Particle *p, real radius, V3 box_min, V3 box_max, real restitution, real friction) {
    real keep = 1 - friction;
    V3 *pos = &p->position;
    V3 *vel = &p->velocity;
    particle_collide_box_axis(&pos->x, &vel->x, &vel->y, &vel->z, box_min.x + radius, box_max.x - radius, restitution, keep);
    particle_collide_box_axis(&pos->y, &vel->y, &vel->x, &vel->z, box_min.y + radius, box_max.y - radius, restitution, keep);
    particle_collide_box_axis(&pos->z, &vel->z, &vel->x, &vel->y, box_min.z + radius, box_max.z - radius, restitution, keep);
}
