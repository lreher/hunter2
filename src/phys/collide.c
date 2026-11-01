#include "base/base.h"
#include "phys/phys.h"

// fills the parts both kinds share. normal_speed: separating speed before any impulse, negative = closing
static void particle_contact_fill(ParticleContact *c, Particle *a, Particle *b, V3 normal, real total_inverse_mass,
                                  real gap, real normal_speed, real restitution, real dt) {
    c->a = a;
    c->b = b;
    c->normal = normal;
    c->total_inverse_mass = total_inverse_mass;
    // a gap may close this step, not more. an overlap may not get deeper, and opens a little each step (fix)
    c->target = gap > 0 ? -gap / dt : 0;
    c->fix = gap < -PHYS_OVERLAP_SLOP ? (-gap - PHYS_OVERLAP_SLOP) * PHYS_OVERLAP_FIX / dt : 0;
    // closing faster than the gap allows: they meet this step. slow impacts do not bounce
    c->bounce = normal_speed < c->target ? -normal_speed * particle_bounce_factor(-normal_speed, restitution) : 0;
    c->impulse = 0;
    c->fix_impulse = 0;
}

i32 particle_contact_find_spheres(ParticleContact *c, Particle *a, real radius_a, Particle *b, real radius_b,
                                  real restitution, real dt, real margin) {
    V3 delta = v3_subtracted(b->position, a->position);
    real min_distance = radius_a + radius_b;
    real reach = min_distance + margin;
    real square_distance = v3_square_magnitude(delta);
    if (square_distance >= reach * reach) return 0;

    real total_inverse_mass = a->inverse_mass + b->inverse_mass;
    if (total_inverse_mass <= 0) return 0; // two immovable things

    // if they sit exactly on top of each other, pick any direction
    real distance = real_sqrt(square_distance);
    V3 normal = distance > 0.00001f ? v3_multiplied(delta, 1 / distance) : v3_create(1, 0, 0);
    real normal_speed = v3_scalar_product(v3_subtracted(b->velocity, a->velocity), normal);
    particle_contact_fill(c, a, b, normal, total_inverse_mass, distance - min_distance, normal_speed, restitution, dt);
    return 1;
}

i32 particle_contact_find_plane(ParticleContact *c, Particle *p, real radius, V3 normal, real offset,
                                real restitution, real friction, real dt, real margin) {
    real gap = v3_scalar_product(p->position, normal) - offset - radius;
    if (gap >= margin) return 0;
    if (p->inverse_mass <= 0) return 0;

    real normal_speed = v3_scalar_product(p->velocity, normal);
    particle_contact_fill(c, p, 0, v3_multiplied(normal, -1), p->inverse_mass, gap, normal_speed, restitution, dt);
    if (normal_speed < c->target) { // touching this step: lose some of the sideways velocity
        V3 side = v3_subtracted(p->velocity, v3_multiplied(normal, normal_speed));
        v3_add_scaled(&p->velocity, side, -friction);
    }
    return 1;
}

static void particle_contact_apply(ParticleContact *c, real impulse) {
    v3_add_scaled(&c->a->velocity, c->normal, -impulse * c->a->inverse_mass);
    if (c->b) v3_add_scaled(&c->b->velocity, c->normal, impulse * c->b->inverse_mass);
}

static real particle_contact_speed(ParticleContact *c) {
    V3 relative = c->b ? v3_subtracted(c->b->velocity, c->a->velocity) : v3_multiplied(c->a->velocity, -1);
    return v3_scalar_product(relative, c->normal);
}

void particle_contacts_warm(ParticleContact *now, i32 count, ParticleContact *before, i32 before_count) {
    i32 k = 0;
    for (i32 i = 0; i < count; i++) {
        ParticleContact *c = &now[i];
        while (k < before_count && (before[k].id_a < c->id_a || (before[k].id_a == c->id_a && before[k].id_b < c->id_b))) k++;
        if (k == before_count) return;
        if (before[k].id_a == c->id_a && before[k].id_b == c->id_b) {
            c->impulse = before[k].impulse;
            particle_contact_apply(c, c->impulse);
        }
    }
}

FORCE_INLINE void particle_contacts_solve(ParticleContact *list, i32 count, i32 passes) {
    // the steady push first: this is what the warm start carries to the next step
    for (i32 pass = 0; pass < passes; pass++) {
        for (i32 i = 0; i < count; i++) {
            ParticleContact *c = &list[i];
            real old = c->impulse;
            real total = old + (c->target - particle_contact_speed(c)) / c->total_inverse_mass;
            if (total < 0) total = 0;
            c->impulse = total;
            particle_contact_apply(c, total - old);
        }
    }
    // then the overlap fix on top. all contacts take part, so neighbors make room. it may take back some of the
    // steady push, but the total never pulls
    for (i32 pass = 0; pass < passes; pass++) {
        for (i32 i = 0; i < count; i++) {
            ParticleContact *c = &list[i];
            real old = c->fix_impulse;
            real total = old + (c->target + c->fix - particle_contact_speed(c)) / c->total_inverse_mass;
            if (total < -c->impulse) total = -c->impulse;
            c->fix_impulse = total;
            particle_contact_apply(c, total - old);
        }
    }
}

// only pairs the solver had to stop (impulse > 0) have met. not accumulated: a bounce is a one off, and the warm
// start should only carry the steady push
void particle_contacts_bounce(ParticleContact *list, i32 count, i32 passes) {
    // the bodies have moved since the contact was found: push along the line between them now, where they touch.
    // the old line would be a little off and give the pair a twist, so angular momentum would change.
    // the stop (solver) and the bounce then push along slightly different lines, which would change the energy a
    // little (always up). so pick the bounce speed that gives the energy one bounce gives: with closing speed u,
    // stop target t, speed along the new line w and bounce speed b, the energy change is m/2 (t^2 - u^2 + b^2 - w^2),
    // and one bounce at restitution e changes it by m/2 (e^2 u^2 - u^2). c->bounce is e u, so b^2 = (e u)^2 + w^2 - t^2
    for (i32 i = 0; i < count; i++) {
        ParticleContact *c = &list[i];
        if (c->bounce <= 0 || c->impulse <= 0 || !c->b) continue;
        V3 delta = v3_subtracted(c->b->position, c->a->position);
        real distance = v3_magnitude(delta);
        if (distance <= 0.00001f) continue;
        c->normal = v3_multiplied(delta, 1 / distance);
        real speed = particle_contact_speed(c);
        real square = c->bounce * c->bounce + speed * speed - c->target * c->target;
        c->bounce = square > 0 ? real_sqrt(square) : 0;
    }
    for (i32 pass = 0; pass < passes; pass++) {
        for (i32 i = 0; i < count; i++) {
            ParticleContact *c = &list[i];
            if (c->bounce <= 0 || c->impulse <= 0) continue;
            real speed = particle_contact_speed(c);
            if (speed < c->bounce) particle_contact_apply(c, (c->bounce - speed) / c->total_inverse_mass);
        }
    }
}
