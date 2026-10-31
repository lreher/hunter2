#include "base/base.h"
#include "phys/phys.h"

i32 particle_collide_spheres(Particle *a, real radius_a, Particle *b, real radius_b, real restitution) {
    V3 delta = v3_subtracted(b->position, a->position);
    real min_distance = radius_a + radius_b;
    real square_distance = v3_square_magnitude(delta);
    if (square_distance >= min_distance * min_distance) return 0;

    real total_inverse_mass = a->inverse_mass + b->inverse_mass;
    if (total_inverse_mass <= 0) return 0; // two immovable things

    // normal points from a to b. if they sit exactly on top of each other, pick any direction
    real distance = real_sqrt(square_distance);
    V3 normal = distance > 0.00001f ? v3_multiplied(delta, 1 / distance) : v3_create(1, 0, 0);

    // push apart, the lighter one moves more
    real penetration = min_distance - distance;
    v3_add_scaled(&a->position, normal, -penetration * a->inverse_mass / total_inverse_mass);
    v3_add_scaled(&b->position, normal, penetration * b->inverse_mass / total_inverse_mass);

    // negative = moving toward each other. if they are separating already, leave the velocities alone
    real approach_speed = v3_scalar_product(v3_subtracted(b->velocity, a->velocity), normal);
    if (approach_speed >= 0) return 1;

    real bounce = particle_bounce_factor(-approach_speed, restitution);
    real impulse = -(1 + bounce) * approach_speed / total_inverse_mass;
    v3_add_scaled(&a->velocity, normal, -impulse * a->inverse_mass);
    v3_add_scaled(&b->velocity, normal, impulse * b->inverse_mass);
    return 1;
}
