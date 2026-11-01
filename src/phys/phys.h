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

void particle_integrate_velocity(Particle *p, real dt); // forces and acceleration into velocity, damping, clears forces
void particle_integrate_position(Particle *p, real dt); // moves with the velocity
void particle_integrate(Particle *p, real dt);          // both halves, for particles without contacts
void particle_set_mass(Particle *p, real mass); // mass must be > 0, traps otherwise. immovable: particle_set_inverse_mass(p, 0)
void particle_set_inverse_mass(Particle *p, real inverse_mass);
real particle_get_mass(Particle *p);            // REAL_MAX for infinite mass
i32 particle_has_finite_mass(Particle *p);
void particle_add_force(Particle *p, V3 force);
void particle_clear_force_accum(Particle *p);

// Slow impacts (under PHYS_REST_SPEED) do not bounce, so resting things settle instead of jittering.
#define PHYS_REST_SPEED 0.5f
real particle_bounce_factor(real impact_speed, real restitution);

// Contacts (our take on ch 7). A step runs in this order:
//   forces, particle_integrate_velocity, find contacts, particle_contacts_warm, particle_contacts_solve,
//   particle_integrate_position, particle_contacts_bounce.
// Contacts fix the velocities before anything moves, so bodies never move into each other first and get pushed
// back later. (The book integrates first and then pushes positions apart. A push is not an impulse, so it does not
// keep angular momentum: our gravity cloud spun faster and faster.) Contacts only add impulses along the normal,
// equal and opposite, so momentum and angular momentum stay.
// Speculative: a pair is a contact while it could touch this step. It may close its gap, not more.
// The solver makes passes with an accumulated impulse per contact (it can shrink again, never below 0) and starts
// from last step's impulse (warm start): a pile needs about the same pushes every step, so a few passes are enough.
// Leftover overlap (the passes never fully settle a pile) is fixed by a little extra speed apart, on top of the
// steady push and started from zero every step: carried over, it would push too hard once the overlap is gone,
// and piles shook.
#define PHYS_OVERLAP_FIX 0.2f    // fraction of a leftover overlap removed per step, by velocity
#define PHYS_OVERLAP_SLOP 0.005f // m. overlaps up to this are left alone: a resting pile always keeps a little, and
                                 // fixing it set the pile shaking (the fix speed fought the warm start)

typedef struct {
    Particle *a, *b;         // b = 0: immovable scenery, like a wall
    i32 id_a, id_b;          // the caller's ids for the pair, to find it again next step (particle_contacts_warm)
    V3 normal;               // from a toward b
    real total_inverse_mass;
    real target;             // the solver keeps the separating speed along the normal at least this
    real fix;                // extra separating speed that removes PHYS_OVERLAP_FIX of the overlap past the slop
    real bounce;             // separating speed wanted after an impact, 0 = no bounce
    real impulse;            // steady push, accumulated by the solver, >= 0. the warm start carries it over
    real fix_impulse;        // the fix on top, from zero every step. impulse + fix_impulse >= 0
} ParticleContact;

// Fill c and return 1 if the two could touch this step. Call after particle_integrate_velocity.
// margin: how far apart the surfaces can be and still touch this step, (speed_a + speed_b) * dt covers it.
// Ids are left to the caller.
i32 particle_contact_find_spheres(ParticleContact *c, Particle *a, real radius_a, Particle *b, real radius_b,
                                  real restitution, real dt, real margin);
// A sphere against a plane: the points x with x . normal = offset, normal (unit) pointing to the free side.
// friction: share of the sideways velocity lost each step it touches.
i32 particle_contact_find_plane(ParticleContact *c, Particle *p, real radius, V3 normal, real offset,
                                real restitution, real friction, real dt, real margin);
// Starts each contact from its impulse last step. Both lists sorted by (id_a, id_b).
void particle_contacts_warm(ParticleContact *now, i32 count, ParticleContact *before, i32 before_count);
// Before particle_integrate_position: stops pairs from closing more than their gap.
FORCE_INLINE void particle_contacts_solve(ParticleContact *list, i32 count, i32 passes);
// After particle_integrate_position, when the pairs that met are touching: their bounce.
void particle_contacts_bounce(ParticleContact *list, i32 count, i32 passes);

// Force generators (ch 5): a force that is asked every step to add itself to a particle.
// A kind and a switch, not function pointers, so the force math can be inlined into the loop.
// No per particle data, so one generator can serve any number of particles.
typedef enum { FORCE_GRAVITY, FORCE_DRAG, FORCE_ATTRACT, FORCE_EXPLODE } ForceKind;
typedef struct {
    ForceKind kind;
    union {
        V3 gravity;                   // FORCE_GRAVITY: acceleration, scaled by mass into a force
        struct { real k1, k2; } drag; // FORCE_DRAG: k1 * speed + k2 * speed^2, against the velocity
        struct { Particle *other; real g; } attract; // FORCE_ATTRACT: pull toward other, g m1 m2 / r^2 (book 3.2, eq 3.3)
        struct { V3 center; real force; } explode;   // FORCE_EXPLODE: push straight away from center, the same force on every mass
    };
} ForceGen;

ForceGen force_gravity(V3 gravity);
ForceGen force_drag(real k1, real k2);
ForceGen force_attract(Particle *other, real g); // no pull on other itself, or if either mass is infinite
ForceGen force_explode(V3 center, real force);   // a blast (simple version of book 17.2.3). nothing at the center itself
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
