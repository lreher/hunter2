#include "base/base.h"
#include "phys/phys.h"

// Semi-implicit Euler: velocity first, then position with the new velocity.
// The book (ch 3, book page 51) moves position with the old velocity. Under gravity that adds a little
// energy every step, and small bounces never die out. This order loses a little instead, so things settle.
// Two halves, so contacts can fix the velocities in between (see Contacts in phys.h).
void particle_apply_forces(Particle *particle, real time_step) {
    Vector3 acceleration = particle->acceleration;
    vector3_add_scaled(&acceleration, particle->force_total, particle->inverse_mass);
    vector3_add_scaled(&particle->velocity, acceleration, time_step);
    vector3_multiply(&particle->velocity, particle->damping);
    particle_clear_force_total(particle);
}

void particle_move(Particle *particle, real time_step) {
    vector3_add_scaled(&particle->position, particle->velocity, time_step);
}

void particle_integrate(Particle *particle, real time_step) {
    particle_apply_forces(particle, time_step);
    particle_move(particle, time_step);
}

void particle_set_mass(Particle *particle, real mass) {
    if (mass <= 0) __builtin_trap(); // zero mass would mean infinite acceleration. no assert without libc, so fail loudly
    particle->inverse_mass = 1.0f / mass;
}

void particle_set_inverse_mass(Particle *particle, real inverse_mass) {
    particle->inverse_mass = inverse_mass;
}

real particle_get_mass(Particle *particle) {
    return particle->inverse_mass == 0 ? REAL_MAX : 1.0f / particle->inverse_mass;
}

i32 particle_has_finite_mass(Particle *particle) {
    return particle->inverse_mass > 0;
}

void particle_add_force(Particle *particle, Vector3 force) {
    vector3_add(&particle->force_total, force);
}

void particle_clear_force_total(Particle *particle) {
    particle->force_total = vector3_zero();
}

real particle_bounce_factor(real impact_speed, real restitution) {
    return impact_speed < PHYS_REST_SPEED ? 0 : restitution;
}
