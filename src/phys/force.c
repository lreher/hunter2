#include "base/base.h"
#include "phys/phys.h"

ForceGen force_gravity(V3 gravity) {
    return (ForceGen){.kind = FORCE_GRAVITY, .gravity = gravity};
}

ForceGen force_drag(real k1, real k2) {
    return (ForceGen){.kind = FORCE_DRAG, .drag = {k1, k2}};
}

ForceGen force_attract(Particle *other, real g) {
    return (ForceGen){.kind = FORCE_ATTRACT, .attract = {other, g}};
}

ForceGen force_explode(V3 center, real force) {
    return (ForceGen){.kind = FORCE_EXPLODE, .explode = {center, force}};
}

ForceGen force_spring(Particle *other, real k, real rest) {
    return (ForceGen){.kind = FORCE_SPRING, .spring = {other, k, rest}};
}

ForceGen force_anchored_spring(V3 anchor, real k, real rest) {
    return (ForceGen){.kind = FORCE_ANCHORED_SPRING, .anchored_spring = {anchor, k, rest}};
}

ForceGen force_bungee(Particle *other, real k, real rest) {
    return (ForceGen){.kind = FORCE_BUNGEE, .bungee = {other, k, rest}};
}

ForceGen force_anchored_bungee(V3 anchor, real k, real rest) {
    return (ForceGen){.kind = FORCE_ANCHORED_BUNGEE, .anchored_bungee = {anchor, k, rest}};
}

ForceGen force_buoyancy(real water_height, real depth, real volume, real density, real gravity, real drag) {
    return (ForceGen){.kind = FORCE_BUOYANCY, .buoyancy = {water_height, depth, volume, density, gravity, drag}};
}

// Hooke's law (book eq 6.1) with d = end - position, so the force points along d when stretched.
// No abs on (length - rest) like the book's ParticleSpring has: with it a compressed spring pulls too.
// pull_only: a bungee, nothing while it is not stretched
FORCE_INLINE void force_hooke(Particle *p, V3 end, real k, real rest, i32 pull_only) {
    V3 d = v3_subtracted(end, p->position);
    real r2 = v3_square_magnitude(d);
    if (r2 == 0) return; // both ends on one spot: no direction
    real length = real_sqrt(r2);
    if (pull_only && length <= rest) return;
    particle_add_force(p, v3_multiplied(d, k * (length - rest) / length));
}

FORCE_INLINE void force_update(ForceGen *g, Particle *p, real dt) {
    (void)dt; // no generator needs it yet. faked stiff springs (6.3) will
    switch (g->kind) {
    case FORCE_GRAVITY:
        if (!particle_has_finite_mass(p)) return;
        particle_add_force(p, v3_multiplied(g->gravity, particle_get_mass(p)));
        break;
    case FORCE_DRAG: {
        // the book: -unit(v) * (k1 |v| + k2 |v|^2). the same as -v * (k1 + k2 |v|),
        // which needs no normalize, so a particle at rest gets zero force instead of NaN
        real speed = v3_magnitude(p->velocity);
        particle_add_force(p, v3_multiplied(p->velocity, -(g->drag.k1 + g->drag.k2 * speed)));
        break;
    }
    case FORCE_ATTRACT: {
        Particle *o = g->attract.other;
        if (o == p || !particle_has_finite_mass(p) || !particle_has_finite_mass(o)) return;
        V3 d = v3_subtracted(o->position, p->position);
        real r2 = v3_square_magnitude(d);
        if (r2 == 0) return; // same spot: no direction
        // g m1 m2 / r^2 along d / r, so d * g m1 m2 / r^3
        real r = real_sqrt(r2);
        real f = g->attract.g * particle_get_mass(p) * particle_get_mass(o) / (r2 * r);
        particle_add_force(p, v3_multiplied(d, f));
        break;
    }
    case FORCE_EXPLODE: {
        // the same force on light and heavy, so the light ones get more speed (dv = F dt / m)
        V3 d = v3_subtracted(p->position, g->explode.center);
        real r2 = v3_square_magnitude(d);
        if (r2 == 0) return; // at the center: no direction
        particle_add_force(p, v3_multiplied(d, g->explode.force / real_sqrt(r2)));
        break;
    }
    case FORCE_SPRING:
        force_hooke(p, g->spring.other->position, g->spring.k, g->spring.rest, 0);
        break;
    case FORCE_ANCHORED_SPRING:
        force_hooke(p, g->anchored_spring.anchor, g->anchored_spring.k, g->anchored_spring.rest, 0);
        break;
    case FORCE_BUNGEE:
        force_hooke(p, g->bungee.other->position, g->bungee.k, g->bungee.rest, 1);
        break;
    case FORCE_ANCHORED_BUNGEE:
        force_hooke(p, g->anchored_bungee.anchor, g->anchored_bungee.k, g->anchored_bungee.rest, 1);
        break;
    case FORCE_BUOYANCY: {
        // the book has d = (y - water - depth) / 2 depth, which runs 0 to -1 going in, so it pulled things down.
        // flipped here
        real depth = g->buoyancy.depth;
        real d = (g->buoyancy.water_height - p->position.y + depth) / (2 * depth);
        if (d <= 0) return; // out of the water
        if (d > 1) d = 1;   // all under: deeper pushes no more water aside
        p->force_accum.y += d * g->buoyancy.density * g->buoyancy.volume * g->buoyancy.gravity;
        particle_add_force(p, v3_multiplied(p->velocity, -d * g->buoyancy.drag));
        break;
    }
    }
}

void force_registry_add(ForceRegistry *r, Particle *p, ForceGen *g) {
    if (r->count >= r->cap) __builtin_trap(); // full: give the registry a bigger array
    r->regs[r->count++] = (ForceReg){p, g};
}

// moves the last pair into the hole. order does not matter, the forces just add up
void force_registry_remove(ForceRegistry *r, Particle *p, ForceGen *g) {
    for (i32 i = 0; i < r->count; i++) {
        if (r->regs[i].particle == p && r->regs[i].gen == g) {
            r->regs[i] = r->regs[--r->count];
            return;
        }
    }
}

void force_registry_clear(ForceRegistry *r) {
    r->count = 0;
}

FORCE_INLINE void force_registry_update(ForceRegistry *r, real dt) {
    for (i32 i = 0; i < r->count; i++) force_update(r->regs[i].gen, r->regs[i].particle, dt);
}
