#ifndef PHYS_H
#define PHYS_H

#include "base/base.h"

// Physics depends on base and nothing else. No rendering, no JS.
// 3D, like the book. The step size is fixed, so damping is applied once per step.

typedef struct {
    V3 position;
    V3 velocity;
    V3 acceleration;   // constant acceleration, like gravity
    V3 force_accum;    // forces added this step, cleared by integrate
    real damping;      // fraction of velocity kept each step (1 = no loss)
    real inverse_mass; // 0 = infinite mass, forces cannot move it
} Particle;

void particle_integrate(Particle *p, real dt);
void particle_set_mass(Particle *p, real mass); // mass must be > 0, traps otherwise. immovable: particle_set_inverse_mass(p, 0)
void particle_set_inverse_mass(Particle *p, real inverse_mass);
real particle_get_mass(Particle *p);            // REAL_MAX for infinite mass
i32 particle_has_finite_mass(Particle *p);
void particle_add_force(Particle *p, V3 force);
void particle_clear_force_accum(Particle *p);

// keeps a ball of the given radius inside the box. restitution = bounce, friction = sideways loss on impact
void particle_collide_box(Particle *p, real radius, V3 box_min, V3 box_max, real restitution, real friction);

// Slow impacts (under PHYS_REST_SPEED) do not bounce, so resting things settle instead of jittering.
#define PHYS_REST_SPEED 0.5f
real particle_bounce_factor(real impact_speed, real restitution);

// Pushes two overlapping spheres apart (the lighter one moves more) and bounces their velocities.
// Returns 1 if they were touching, 0 if not.
i32 particle_collide_spheres(Particle *a, real radius_a, Particle *b, real radius_b, real restitution);

#endif
