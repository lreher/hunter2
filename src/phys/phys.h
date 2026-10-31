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

// Force generators (ch 5): a force that is asked every step to add itself to a particle.
// A kind and a switch, not function pointers, so the force math can be inlined into the loop.
// No per particle data, so one generator can serve any number of particles.
typedef enum { FORCE_GRAVITY, FORCE_DRAG } ForceKind;
typedef struct {
    ForceKind kind;
    union {
        V3 gravity;                   // FORCE_GRAVITY: acceleration, scaled by mass into a force
        struct { real k1, k2; } drag; // FORCE_DRAG: k1 * speed + k2 * speed^2, against the velocity
    };
} ForceGen;

ForceGen force_gravity(V3 gravity);
ForceGen force_drag(real k1, real k2);
FORCE_INLINE void force_update(ForceGen *g, Particle *p, real dt); // adds g's force to p for this step

// Which generator acts on which particle. Run force_registry_update before integrating.
// The caller owns the array, sized for its needs: {regs, 0, cap}. The registry never owns particles or generators.
typedef struct { Particle *particle; ForceGen *gen; } ForceReg;
typedef struct { ForceReg *regs; i32 count, cap; } ForceRegistry;

void force_registry_add(ForceRegistry *r, Particle *p, ForceGen *g);    // traps when full
void force_registry_remove(ForceRegistry *r, Particle *p, ForceGen *g); // no effect if the pair is not there
void force_registry_clear(ForceRegistry *r);
FORCE_INLINE void force_registry_update(ForceRegistry *r, real dt);

#endif
