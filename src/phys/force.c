#include "base/base.h"
#include "phys/phys.h"

ForceGen force_gravity(Vector3 gravity) {
    return (ForceGen){.kind = FORCE_GRAVITY, .gravity = gravity};
}

ForceGen force_drag(real k1, real k2) {
    return (ForceGen){.kind = FORCE_DRAG, .drag = {k1, k2}};
}

ForceGen force_attract(Particle *other, real gravitational_constant) {
    return (ForceGen){.kind = FORCE_ATTRACT, .attract = {other, gravitational_constant}};
}

ForceGen force_explode(Vector3 center, real force) {
    return (ForceGen){.kind = FORCE_EXPLODE, .explode = {center, force}};
}

ForceGen force_spring(Particle *other, real stiffness, real rest) {
    return (ForceGen){.kind = FORCE_SPRING, .spring = {other, stiffness, rest}};
}

ForceGen force_anchored_spring(Vector3 anchor, real stiffness, real rest) {
    return (ForceGen){.kind = FORCE_ANCHORED_SPRING, .anchored_spring = {anchor, stiffness, rest}};
}

ForceGen force_bungee(Particle *other, real stiffness, real rest) {
    return (ForceGen){.kind = FORCE_BUNGEE, .bungee = {other, stiffness, rest}};
}

ForceGen force_anchored_bungee(Vector3 anchor, real stiffness, real rest) {
    return (ForceGen){.kind = FORCE_ANCHORED_BUNGEE, .anchored_bungee = {anchor, stiffness, rest}};
}

ForceGen force_buoyancy(real water_height, real depth, real volume, real density, real gravity, real drag) {
    return (ForceGen){.kind = FORCE_BUOYANCY, .buoyancy = {water_height, depth, volume, density, gravity, drag}};
}

// Hooke's law (book eq 6.1) with to_end = end - position, so the force points along to_end when stretched.
// No abs on (length - rest) like the book's ParticleSpring has: with it a compressed spring pulls too.
// pull_only: a bungee, nothing while it is not stretched
FORCE_INLINE void force_hooke(Particle *particle, Vector3 end, real stiffness, real rest, i32 pull_only) {
    Vector3 to_end = vector3_subtracted(end, particle->position);
    real distance_squared = vector3_square_magnitude(to_end);
    if (distance_squared == 0) return; // both ends on one spot: no direction
    real length = real_sqrt(distance_squared);
    if (pull_only && length <= rest) return;
    particle_add_force(particle, vector3_multiplied(to_end, stiffness * (length - rest) / length));
}

FORCE_INLINE void force_update(ForceGen *generator, Particle *particle, real time_step) {
    (void)time_step; // no generator needs it yet. faked stiff springs (6.3) will
    switch (generator->kind) {
    case FORCE_GRAVITY:
        if (!particle_has_finite_mass(particle)) return;
        particle_add_force(particle, vector3_multiplied(generator->gravity, particle_get_mass(particle)));
        break;
    case FORCE_DRAG: {
        // the book: -unit(v) * (k1 |v| + k2 |v|^2). the same as -v * (k1 + k2 |v|),
        // which needs no normalize, so a particle at rest gets zero force instead of NaN
        real speed = vector3_magnitude(particle->velocity);
        particle_add_force(particle, vector3_multiplied(particle->velocity, -(generator->drag.k1 + generator->drag.k2 * speed)));
        break;
    }
    case FORCE_ATTRACT: {
        Particle *o = generator->attract.other;
        if (o == particle || !particle_has_finite_mass(particle) || !particle_has_finite_mass(o)) return;
        Vector3 d = vector3_subtracted(o->position, particle->position);
        real r2 = vector3_square_magnitude(d);
        if (r2 == 0) return; // same spot: no direction
        // G m1 m2 / r^2 along d / r, so d * G m1 m2 / r^3
        real r = real_sqrt(r2);
        real f = generator->attract.gravitational_constant * particle_get_mass(particle) * particle_get_mass(o) / (r2 * r);
        particle_add_force(particle, vector3_multiplied(d, f));
        break;
    }
    case FORCE_EXPLODE: {
        // the same force on light and heavy, so the light ones get more speed (speed gained = force * time_step / mass)
        Vector3 d = vector3_subtracted(particle->position, generator->explode.center);
        real r2 = vector3_square_magnitude(d);
        if (r2 == 0) return; // at the center: no direction
        particle_add_force(particle, vector3_multiplied(d, generator->explode.force / real_sqrt(r2)));
        break;
    }
    case FORCE_SPRING:
        force_hooke(particle, generator->spring.other->position, generator->spring.stiffness, generator->spring.rest, 0);
        break;
    case FORCE_ANCHORED_SPRING:
        force_hooke(particle, generator->anchored_spring.anchor, generator->anchored_spring.stiffness, generator->anchored_spring.rest, 0);
        break;
    case FORCE_BUNGEE:
        force_hooke(particle, generator->bungee.other->position, generator->bungee.stiffness, generator->bungee.rest, 1);
        break;
    case FORCE_ANCHORED_BUNGEE:
        force_hooke(particle, generator->anchored_bungee.anchor, generator->anchored_bungee.stiffness, generator->anchored_bungee.rest, 1);
        break;
    case FORCE_BUOYANCY: {
        // the book has d = (y - water - depth) / 2 depth, which runs 0 to -1 going in, so it pulled things down.
        // flipped here
        real depth = generator->buoyancy.depth;
        real d = (generator->buoyancy.water_height - particle->position.y + depth) / (2 * depth);
        if (d <= 0) return; // out of the water
        if (d > 1) d = 1;   // all under: deeper pushes no more water aside
        particle->force_total.y += d * generator->buoyancy.density * generator->buoyancy.volume * generator->buoyancy.gravity;
        particle_add_force(particle, vector3_multiplied(particle->velocity, -d * generator->buoyancy.drag));
        break;
    }
    }
}

void force_registry_add(ForceRegistry *r, Particle *particle, ForceGen *generator) {
    if (r->count >= r->cap) __builtin_trap(); // full: give the registry a bigger array
    r->regs[r->count++] = (ForceReg){particle, generator};
}

// moves the last pair into the hole. order does not matter, the forces just add up
void force_registry_remove(ForceRegistry *r, Particle *particle, ForceGen *generator) {
    for (i32 i = 0; i < r->count; i++) {
        if (r->regs[i].particle == particle && r->regs[i].gen == generator) {
            r->regs[i] = r->regs[--r->count];
            return;
        }
    }
}

void force_registry_clear(ForceRegistry *r) {
    r->count = 0;
}

FORCE_INLINE void force_registry_update(ForceRegistry *r, real time_step) {
    for (i32 i = 0; i < r->count; i++) force_update(r->regs[i].gen, r->regs[i].particle, time_step);
}
