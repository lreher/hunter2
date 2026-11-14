#ifndef PHYS_H
#define PHYS_H

#include "base/base.h"

// Physics depends on base and nothing else. No rendering, no JS.
// 3D, like the book. The step size is fixed, so damping is applied once per step.

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;   // constant acceleration, like gravity
    Vector3 force_total;    // forces added this step, cleared by particle_apply_forces
    real damping;      // fraction of velocity kept each step (1 = no loss)
    real inverse_mass; // 0 = infinite mass, forces cannot move it
} Particle;

void particle_apply_forces(Particle *particle, real time_step); // forces and acceleration into velocity, damping, clears forces
void particle_move(Particle *particle, real time_step); // moves with the velocity
void particle_integrate(Particle *particle, real time_step);          // both halves, for particles without contacts
void particle_set_mass(Particle *particle, real mass); // mass must be > 0, traps otherwise. immovable: particle_set_inverse_mass(particle, 0)
void particle_set_inverse_mass(Particle *particle, real inverse_mass);
real particle_get_mass(Particle *particle);            // REAL_MAX for infinite mass
i32 particle_has_finite_mass(Particle *particle);
void particle_add_force(Particle *particle, Vector3 force);
void particle_clear_force_total(Particle *particle);

// Slow impacts (under PHYS_REST_SPEED) do not bounce, so resting things settle instead of jittering.
#define PHYS_REST_SPEED 0.5f
real particle_bounce_factor(real impact_speed, real restitution);

// Contacts (our take on ch 7). A step runs in this order:
//   forces, particle_apply_forces, find contacts, particle_contacts_warm, particle_contacts_solve,
//   particle_move, particle_contacts_bounce.
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
    Vector3 normal;               // from a toward b
    real total_inverse_mass;
    real target;             // the solver keeps the separating speed along the normal at least this
    real fix;                // extra separating speed that removes PHYS_OVERLAP_FIX of the overlap past the slop
    real bounce;             // separating speed wanted after an impact, 0 = no bounce
    real impulse;            // steady push, accumulated by the solver, >= 0. the warm start carries it over
    real fix_impulse;        // the fix on top, from zero every step. impulse + fix_impulse >= 0
} ParticleContact;

// Fill c and return 1 if the two could touch this step. Call after particle_apply_forces.
// margin: how far apart the surfaces can be and still touch this step, (speed_a + speed_b) * time_step covers it.
// Ids are left to the caller.
i32 particle_contact_find_spheres(ParticleContact *c, Particle *a, real radius_a, Particle *b, real radius_b,
                                  real restitution, real time_step, real margin);
// A sphere against a plane: the points x with x . normal = offset, normal (unit) pointing to the free side.
// friction: share of the sideways velocity lost each step it touches.
i32 particle_contact_find_plane(ParticleContact *c, Particle *particle, real radius, Vector3 normal, real offset,
                                real restitution, real friction, real time_step, real margin);
// Starts each contact from its impulse last step. Both lists sorted by (id_a, id_b).
void particle_contacts_warm(ParticleContact *now, i32 count, ParticleContact *before, i32 before_count);
// Before particle_move: stops pairs from closing more than their gap.
FORCE_INLINE void particle_contacts_solve(ParticleContact *list, i32 count, i32 passes);
// After particle_move, when the pairs that met are touching: their bounce.
void particle_contacts_bounce(ParticleContact *list, i32 count, i32 passes);

// Force generators (ch 5): a force that is asked every step to add itself to a particle.
// A kind and a switch, not function pointers, so the force math can be inlined into the loop.
// No per particle data, so one generator can serve any number of particles.
typedef enum {
    FORCE_GRAVITY, FORCE_DRAG, FORCE_ATTRACT, FORCE_EXPLODE,
    FORCE_SPRING, FORCE_ANCHORED_SPRING, FORCE_BUNGEE, FORCE_ANCHORED_BUNGEE, FORCE_BUOYANCY,
} ForceKind;
typedef struct {
    ForceKind kind;
    union {
        Vector3 gravity;                   // FORCE_GRAVITY: acceleration, scaled by mass into a force
        struct { real k1, k2; } drag; // FORCE_DRAG: k1 * speed + k2 * speed^2, against the velocity
        struct { Particle *other; real gravitational_constant; } attract; // FORCE_ATTRACT: pull toward other, G m1 m2 / r^2 (book 3.2, eq 3.3)
        struct { Vector3 center; real force; } explode;   // FORCE_EXPLODE: push straight away from center, the same force on every mass
        // springs (ch 6, Hooke's law): stiffness * (length - rest) toward the other end. pulls when stretched, pushes when
        // compressed. one generator per end: a spring between two bodies needs one on each
        struct { Particle *other; real stiffness, rest; } spring;          // FORCE_SPRING: other end on another body
        struct { Vector3 anchor; real stiffness, rest; } anchored_spring;       // FORCE_ANCHORED_SPRING: other end fixed. move it by writing anchor
        struct { Particle *other; real stiffness, rest; } bungee;          // FORCE_BUNGEE: a spring that only pulls
        struct { Vector3 anchor; real stiffness, rest; } anchored_bungee;       // FORCE_ANCHORED_BUNGEE
        // FORCE_BUOYANCY (book 6.2.4): water below the plane y = water_height. The share under water, d, goes from 0
        // (center depth above the surface) to 1 (depth below it), straight in between. Pushes up with
        // d * density * volume * gravity (the weight of the water pushed aside) and slows it with d * drag * velocity
        struct {
            real water_height; // m, y of the surface. move it by writing it
            real depth;        // m, > 0. for a ball, its radius
            real volume;       // m^3
            real density;      // kg/m^3 of the water, fresh water 1000
            real gravity;      // m/s^2, down. the book left it out, so its force was a mass
            real drag;         // N per m/s when fully under. 0 = none, like the book
        } buoyancy;
    };
} ForceGen;

ForceGen force_gravity(Vector3 gravity);
ForceGen force_drag(real k1, real k2);
ForceGen force_attract(Particle *other, real gravitational_constant); // no pull on other itself, or if either mass is infinite
ForceGen force_explode(Vector3 center, real force);   // a blast (simple version of book 17.2.3). nothing at the center itself
ForceGen force_spring(Particle *other, real stiffness, real rest); // stiffness: N/m, rest: m. nothing when both ends are on one spot
ForceGen force_anchored_spring(Vector3 anchor, real stiffness, real rest);
ForceGen force_bungee(Particle *other, real stiffness, real rest);
ForceGen force_anchored_bungee(Vector3 anchor, real stiffness, real rest);
ForceGen force_buoyancy(real water_height, real depth, real volume, real density, real gravity, real drag);
FORCE_INLINE void force_update(ForceGen *generator, Particle *particle, real time_step); // adds the generator's force to the particle for this step

// Which generator acts on which particle. Run force_registry_update before integrating.
// The caller owns the array, sized for its needs: {regs, 0, cap}. The registry never owns particles or generators.
typedef struct { Particle *particle; ForceGen *gen; } ForceReg;
typedef struct { ForceReg *regs; i32 count, cap; } ForceRegistry;

void force_registry_add(ForceRegistry *r, Particle *particle, ForceGen *generator);    // traps when full
void force_registry_remove(ForceRegistry *r, Particle *particle, ForceGen *generator); // no effect if the pair is not there
void force_registry_clear(ForceRegistry *r);
FORCE_INLINE void force_registry_update(ForceRegistry *r, real time_step);

#endif
