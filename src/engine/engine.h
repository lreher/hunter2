#ifndef ENGINE_H
#define ENGINE_H

#include "base/base.h"
#include "phys/phys.h"
#include "render/render.h"

// The engine: the parts a game is built from, plain data and functions over it (focus/engine.md).
// A game writes its parts as plain tables and hands them to the engine in an Engine. Parts point at each other by
// their place in their table.
// Pattern: a shared definition (a "type") and separate instances made from it. Editing a type changes every
// instance made from it, each instance keeps its own place and state.
//   object        a physics object: mass, drag, model. Moved by physics (a point mass for now)
//   actuator type shared settings of an actuator: push (a force) or turn (turns its object around an axis)
//   actuator      one instance on an object: which way it pushes or turns, its input, its own state
//   emitter type  shared spark settings
//   emitter       one instance: attached to an object or on its own, its input, its own memory
// Every part reads its input when it runs (no callbacks): actuators every physics step, emitters every drawn frame.

// ---- Inputs (engine/input.c)
// Where a part's input comes from. Reading one gives how much it is on: 0..1 (a key pair or an aim -1..1).
enum {
    SOURCE_NONE,      // always 0
    SOURCE_ALWAYS,    // always 1
    SOURCE_KEY,       // index = KEY_*: 1 while held
    SOURCE_KEY_PAIR,  // index = KEY_* for 1, negative = KEY_* for -1, both or neither 0
    SOURCE_AIM_PITCH, // the object's mouse aim, up/down: how hard to turn the nose toward it (object_update_aim)
    SOURCE_AIM_YAW,   // the same, left/right
    SOURCE_TILT,      // how far the object leans off level, scaled to roll it back (InputTuning level_*)
    SOURCE_ACTUATOR,  // index = an actuator's place in the list: how hard it pushes or turns now
};
typedef struct {
    i32 kind;     // SOURCE_*
    i32 index;    // which key or actuator
    i32 negative; // SOURCE_KEY_PAIR: the key for -1
} Source;
// for writing parts as tables: .input = INPUT_KEY(KEY_W)
#define INPUT_ALWAYS {.kind = SOURCE_ALWAYS}
#define INPUT_KEY(key) {.kind = SOURCE_KEY, .index = (key)}
#define INPUT_KEY_PAIR(key, negative_key) {.kind = SOURCE_KEY_PAIR, .index = (key), .negative = (negative_key)}
#define INPUT_AIM_PITCH {.kind = SOURCE_AIM_PITCH}
#define INPUT_AIM_YAW {.kind = SOURCE_AIM_YAW}
#define INPUT_TILT {.kind = SOURCE_TILT}
#define INPUT_ACTUATOR(actuator) {.kind = SOURCE_ACTUATOR, .index = (actuator)}

// How mouse aim and tilt are worked out. Code only, not on the panel
typedef struct {
    f32 aim_sensitivity;     // radians the aim moves per pixel of mouse movement
    f32 aim_full_turn_angle; // radians off the aim that ask for full turn speed. less = sharper, more = gentler
    f32 aim_limit;           // radians: the aim stays at most this far from the nose
    i32 aim_invert_pitch;    // 0 = mouse up lifts the nose, 1 = mouse up dives (like a flight stick)
    f32 level_angle;         // radians of lean that read as full tilt. less lean, gentler
    f32 level_strength;      // tilt at full, 0..1: share of the roll speed used for leveling
} InputTuning;
extern InputTuning input_tuning;

// ---- Objects (engine/object.c)
// A physics object. Forward is -z in its own axes (the way an unturned camera looks), y up, x right.
typedef struct {
    const char *name;        // panel group title
    Vector3 start_position;  // m, world: where engine_start puts it
    f32 mass;                // kg
    f32 drag;                // share of speed lost per second. 0 = real space, it drifts forever
    f32 drag_cut;            // %, how much less drag while drag_cut_input is on. 100 = none at all
    Source drag_cut_input;
    RenderModel model;       // drawn with it. -1 = nothing to draw
    f32 size;                // scale the model is drawn at
    Vector3 color;           // 0..1
    // state
    Particle particle;       // position, velocity, mass. forces go through particle_add_force
    Vector3 previous_position;          // from the step before, for blending
    Quaternion rotation, previous_rotation;
    Vector3 aim;             // unit, world: where the pilot wants the nose (object_update_aim)
    f32 aim_pitch, aim_yaw;  // worked out every step from the aim (SOURCE_AIM_PITCH, SOURCE_AIM_YAW)
    f32 tilt;                // worked out every step from how it leans (SOURCE_TILT)
} Object;

void object_start(Object *object); // at start_position, still, unturned, aim on the nose
Vector3 object_axis(Object *object, Vector3 local); // a direction in its own axes, turned into the world
Vector3 object_drawn_position(Object *object, f32 alpha); // blended between the last two steps
Quaternion object_drawn_rotation(Object *object, f32 alpha);
// once per browser frame: the mouse turns the aim around the camera's up and right, so it moves the way the mouse
// does on screen. on = 0 (mouse free, free camera): the aim sits on the nose, no turning
void object_update_aim(Object *object, Vector3 camera_up, i32 on);

// ---- Actuators (engine/actuator.c)
enum { ACTUATOR_PUSH, ACTUATOR_TURN }; // the kind (the user's "actuator type"; type in code is the shared definition)

typedef struct {
    const char *name;  // panel group title
    i32 kind;          // ACTUATOR_PUSH or ACTUATOR_TURN
    f32 force;         // push: N at full input
    f32 boost;         // push: %, extra force while an instance's boost input is on. 50 = 1.5 times force
    f32 cutoff;        // push: % of force, it never runs weaker: lights straight at it, goes out below it
    f32 speed;         // turn: rad/s at full input
    f32 response;      // per second, how fast it catches up with its input. high = snappy, low = heavy
} ActuatorType;

typedef struct {
    i32 type;          // which actuator type
    i32 object;        // the object it is on
    Vector3 direction; // unit, the object's axes. push: the way it pushes. turn: the axis, input 1 turns it the
                       // right hand way around it (thumb along the axis, fingers curl the way it turns)
    Source input;      // how much it is asked for
    Source fallback;   // read when input reads 0 (the ship's roll: Q/E, else level itself)
    Source block;      // while this is on it is asked for nothing (the ship's drift)
    Source boost;      // push: while this is on (and input > 0) the type's boost is added
    f32 now;           // state. push: N now. turn: rad/s now
} Actuator;

// ---- Emitters (engine/emitter.c)
// Emitter type: the shared spark settings. Every emitter made from it fires with these, scaled by its input.
typedef struct {
    const char *name;               // the panel group title (and part of the save key)
    f32 rate;                       // sparks per second at input 1, for each emitter of this type
    f32 speed, speed_spread;        // m/s along the emitter's direction at input 1, give or take speed_spread
    f32 base_speed;                 // m/s at input 0: sparks never come out slower than this, however weak the input
    f32 cone;                       // radians, half the angle of the cone they spread in, at input 1
    f32 cone_squeeze;               // how much more input narrows the cone. 0 = never
    f32 lifetime, lifetime_spread;  // seconds, give or take lifetime_spread
    f32 size;                       // m, half the width of the glow
    Vector3 color_start, color_end; // 0..1, color at birth and at death
} EmitterType;

// Emitter: one placed instance of an emitter type
typedef struct {
    i32 type;                   // which emitter type
    i32 object;                 // the object it is attached to. -1 = on its own, its position is in the world
    Vector3 position;           // m, in the object's axes (on its own: the world)
    Vector3 direction;          // unit, in the same axes: the way the sparks leave
    Source input;               // how hard it fires
    RenderEmitterMemory memory; // render_emit's own, kept between frames
} Emitter;

// fires once (call every drawn frame, even at input 0, so it knows where it was when it fires again).
// position, rotation, velocity: of what it is attached to this frame
void emitter_fire(Emitter *emitter, const EmitterType *type, Vector3 position, Quaternion rotation, Vector3 velocity,
                  f32 input);
void emitter_reset(Emitter *emitter); // what it is attached to jumped: no trail from the old spot
void emitter_type_add_sliders(EmitterType *type); // a panel group titled with its name

// ---- The engine (engine/engine.c): all of a game's parts
typedef struct {
    Object *objects;              i32 object_count;
    ActuatorType *actuator_types; i32 actuator_type_count;
    Actuator *actuators;          i32 actuator_count;
    EmitterType *emitter_types;   i32 emitter_type_count;
    Emitter *emitters;            i32 emitter_count;
    i32 inputs_on; // 0 = keys, aim and tilt read 0 (the free camera has the keys). actuators still ease down
} Engine;

f32 source_read(Engine *engine, Source source, Object *object); // object: the part's own, for aim and tilt
f32 actuator_strength(Engine *engine, Actuator *actuator);      // now / full: 0..1, past 1 with boost
void engine_start(Engine *engine); // every object at its start, actuators at rest, emitters without a trail
// one physics step: inputs are worked out, actuators ease toward them and push or turn their objects, physics moves them
FORCE_INLINE void engine_step(Engine *engine, real time_step);
// one drawn frame: objects draw their models, emitters fire
void engine_draw(Engine *engine, f32 alpha);
// panel groups from the parts' data: each object, actuator type, the shared particle settings, each emitter type
void engine_add_sliders(Engine *engine);

#endif
