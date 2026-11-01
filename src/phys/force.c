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

FORCE_INLINE void force_update(ForceGen *g, Particle *p, real dt) {
    (void)dt; // no generator needs it yet. springs (ch 6) will
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
