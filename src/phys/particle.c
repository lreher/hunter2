#include "base/base.h"
#include "phys/phys.h"

// Semi-implicit Euler: velocity first, then position with the new velocity.
// The book (ch 3, book page 51) moves position with the old velocity. Under gravity that adds a little
// energy every step, and small bounces never die out. This order loses a little instead, so things settle.
// Two halves, so contacts can fix the velocities in between (see Contacts in phys.h).
void particle_integrate_velocity(Particle *p, real dt) {
    V3 acceleration = p->acceleration;
    v3_add_scaled(&acceleration, p->force_accum, p->inverse_mass);
    v3_add_scaled(&p->velocity, acceleration, dt);
    v3_multiply(&p->velocity, p->damping);
    particle_clear_force_accum(p);
}

void particle_integrate_position(Particle *p, real dt) {
    v3_add_scaled(&p->position, p->velocity, dt);
}

void particle_integrate(Particle *p, real dt) {
    particle_integrate_velocity(p, dt);
    particle_integrate_position(p, dt);
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
