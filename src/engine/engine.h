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
//   object        a physics object: mass, drag, model. Moved by physics (a point mass for now). Some are only made
//                 as copies (bullets by emitters, planets by placings): those are not in the world themselves
//   placing       part of the universe: puts copies of an object down at start, random spots, sizes and speeds
//   actuator type shared settings of an actuator: push (a force) or turn (turns its object around an axis)
//   actuator      one instance on an object: which way it pushes or turns, its input, its own state
//   emitter type  shared settings of what an emitter sends out: sparks (look only) or copies of an object
//   emitter       one instance: attached to an object or on its own, its input, its own memory
// Every part reads its input when it runs (no callbacks): actuators every physics step, emitters every drawn frame.

#define ENGINE_NAME_SIZE 32           // a type's name, typed in the panel, with its 0 at the end
#define ENGINE_MAX_ACTUATOR_TYPES 16  // a game's actuator type table must have this many places (new ones go in the free ones)
#define ENGINE_MAX_EMITTER_TYPES 16   // the same for emitter types (render keeps drag and streak for 16)
#define ENGINE_PARTS_TEXT_SIZE 2048   // the parts list as text, saved with the panel (engine/panel.c)

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
// Object.model: the renderer's sphere (render_draw_sphere) instead of a built model. Smooth, finer the bigger it is on
// screen, as in the other demos. 1 m across at size 1. No glow or halo (the sphere has neither)
#define OBJECT_MODEL_BALL ((RenderModel)-2)

typedef struct {
    const char *name;        // panel group title
    Vector3 start_position;  // m, world: where engine_start puts it
    f32 mass;                // kg
    f32 drag;                // share of speed lost per second. 0 = real space, it drifts forever
    i32 copies_only;         // 1 = not in the world itself, only copies of it (emitters, placings): bullets, planets
    f32 lifetime;            // s, copies_only: a copy is gone after this. 0 = never
    f32 density;             // kg per m^3. > 0: a copy's mass comes from its size (a ball that wide), not from mass
    f32 drag_cut;            // %, how much less drag while drag_cut_input is on. 100 = none at all
    Source drag_cut_input;
    RenderModel model;       // drawn with it. -1 = nothing to draw, OBJECT_MODEL_BALL = the renderer's smooth sphere
    f32 size;                // scale the model is drawn at
    f32 radius;              // m, the ball it is for gravity (and hits) when in the world. copies: half their size
    Vector3 color;           // 0..1
    i32 glow;                // 1 = drawn in its full color everywhere, no sun or shade (a bullet)
    f32 halo;                // m, glow on: a soft light in its color around it, gone at this distance. 0 = none
    // copies_only: what a copy does when it hits something (panel block "on hit")
    i32 hit_burst;           // 0 = nothing, k = emitter type k - 1 fires once where the copy was, and the copy is gone
    f32 hit_steps;           // physics steps after the hit before that (its push reaches what it hit first). at least 1
    i32 hit_moves_with;      // the burst's sparks start with: 0 = the velocity of what it hit, 1 = nothing (stay put)
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
    char name[ENGINE_NAME_SIZE]; // panel fold title, part of its sliders' save keys. renamed in the panel
    i32 kind;          // ACTUATOR_PUSH or ACTUATOR_TURN
    f32 force;         // push: N at full input
    f32 boost;         // push: %, extra force while an instance's boost input is on. 50 = 1.5 times force
    f32 cutoff;        // push: % of force, it never runs weaker: lights straight at it, goes out below it
    f32 speed;         // turn: rad/s at full input
    f32 response;      // per second, how fast it catches up with its input. high = snappy, low = heavy
} ActuatorType;

typedef struct {
    const char *name;  // shown in the panel under its object
    i32 type;          // which actuator type
    i32 object;        // the object it is on
    Vector3 direction; // unit, the object's axes. push: the way it pushes. turn: the axis, input 1 turns it the
                       // right hand way around it (thumb along the axis, fingers curl the way it turns)
    Source input;      // how much it is asked for
    Source fallback;   // read when input reads 0 and fallback_on (the ship's roll: Q/E, else level itself)
    i32 fallback_on;   // 0 = input 0 asks for nothing, 1 = it reads fallback. panel: "when idle" nothing | level
    Source block;      // while this is on it is asked for nothing (the ship's drift)
    Source boost;      // push: while this is on (and input > 0) the type's boost is added
    f32 now;           // state. push: N now. turn: rad/s now
} Actuator;

// ---- Emitters (engine/emitter.c)
// Emitter type: the shared settings. Every emitter made from it fires with these, scaled by its input.
enum { EMITS_SPARKS, EMITS_OBJECTS }; // what an emitter type sends out
typedef struct {
    char name[ENGINE_NAME_SIZE];    // panel fold title, part of its sliders' save keys. renamed in the panel
    i32 emits;                      // EMITS_SPARKS or EMITS_OBJECTS
    i32 object;                     // EMITS_OBJECTS: the object it sends out copies of (an copies_only one).
                                    // they use rate, speed, speed_spread and cone, the object has the rest
    f32 rate;                       // sparks per second at input 1, for each emitter of this type
    f32 speed, speed_spread;        // m/s along the emitter's direction at input 1, give or take speed_spread
    f32 base_speed;                 // m/s at input 0: sparks never come out slower than this, however weak the input
    f32 cone;                       // radians, half the angle of the cone they spread in, at input 1
    f32 cone_squeeze;               // how much more input narrows the cone. 0 = never
    f32 lifetime, lifetime_spread;  // seconds, give or take lifetime_spread
    f32 size;                       // m, half the width of the glow
    Vector3 color_start, color_end; // 0..1, color at birth and at death
    f32 stretch;                    // seconds: a moving spark is drawn as a streak as long as the way it goes in this time
    f32 drag;                       // share of a spark's speed lost per second, for its whole life
    f32 burst;                      // > 0: fires once, this many sparks at once (an object's "on hit"), not at a rate
} EmitterType;

// Emitter: one placed instance of an emitter type
typedef struct {
    const char *name;           // shown in the panel under its object
    i32 type;                   // which emitter type
    i32 object;                 // the object it is attached to. -1 = on its own, its position is in the world
    Vector3 position;           // m, in the object's axes (on its own: the world)
    Vector3 direction;          // unit, in the same axes: the way the sparks leave
    Source input;               // how hard it fires
    RenderEmitterMemory memory; // render_emit's own, kept between frames
    f32 owed;                   // EMITS_OBJECTS: copies owed, one goes out at each whole one
} Emitter;

// A copy of an object: sent out by an emitter (a bullet) or put down by a placing (a planet), moved by physics until
// its life runs out (if its object has one)
typedef struct {
    i32 used;                    // 0 = a free place
    i32 object;                  // the object it is a copy of
    i32 placing;                 // the placing that put it down, -1 = an emitter sent it
    i32 from;                    // the object whose emitter sent it, it never hits that one. -1 = none
    f32 life_left;               // s, when its object has a lifetime
    f32 size;                    // m, drawn at this size (a placing picks one per copy, else the object's)
    Particle particle;           // position, velocity, mass
    Vector3 previous_position;   // from the step before, for blending
    Quaternion rotation;         // the emitter's when it went out
    i32 hit_left;                // steps left before it bursts after a hit (its object's hit_burst), 0 = not hit
    i32 hit_body;                // the id of what it hit (engine_gather_bodies): an object's place, or object_count + a copy's
} ObjectCopy;

// fires once (call every drawn frame, even at input 0, so it knows where it was when it fires again).
// position, rotation, velocity: of what it is attached to this frame
void emitter_fire(Emitter *emitter, const EmitterType *type, Vector3 position, Quaternion rotation, Vector3 velocity,
                  f32 input);
void emitter_reset(Emitter *emitter); // what it is attached to jumped: no trail from the old spot
i32 emitter_burst(i32 type_index, const EmitterType *type, Vector3 position, Vector3 velocity); // once. 0 = no room now
void emitter_type_add_sliders(EmitterType *type); // into the panel fold that is open. sparks or objects, by emits

// ---- Placings (engine/placing.c): the universe's. Puts count copies of an object down at random spots inside a ball
// spread across around center, none overlapping another copy or reaching an object in the world, each with a size
// between size_min and size_max and a speed between speed_min and speed_max in a random direction
typedef struct Placing {
    const char *name;           // panel fold title
    i32 object;                 // a copies_only object
    Vector3 center;             // m, world
    f32 count;                  // copies (a float: a panel slider)
    f32 spread;                 // m, across the ball they are put in
    f32 size_min, size_max;     // m, across
    f32 speed_min, speed_max;   // m/s
    f32 placed_with[6];         // count, spread, sizes, speeds when it last placed: a change places again
} Placing;

// ---- The engine (engine/engine.c): all of a game's parts
typedef struct {
    Object *objects;              i32 object_count;
    ActuatorType *actuator_types; i32 actuator_type_count;
    Actuator *actuators;          i32 actuator_count;
    EmitterType *emitter_types;   i32 emitter_type_count;
    Emitter *emitters;            i32 emitter_count;
    ObjectCopy *copies; i32 copy_capacity; // places for copies in flight. full: no new ones
    struct Placing *placings;     i32 placing_count;
    f32 gravity;   // the universe's: every object and copy pulls on every other, real G times this. 0 = none
    f32 bounce;    // the universe's: share of the speed two things meet with that they part with, 0..1
    i32 inputs_on; // 0 = keys, aim and tilt read 0 (the free camera has the keys). actuators still ease down
    // the panel's (engine/panel.c)
    i32 panel_action;                         // a click writes what it asks for here, engine_update_panel does it
    char panel_name[ENGINE_NAME_SIZE];        // a name typed in the panel, for a rename
    char panel_parts[ENGINE_PARTS_TEXT_SIZE]; // which types there are and which type each part uses, saved as "parts"
    i32 panel_parts_changed;                  // 1 = the panel put a saved parts list in, engine_update_panel applies it
} Engine;

f32 source_read(Engine *engine, Source source, Object *object); // object: the part's own, for aim and tilt
// adds what it reads in words to text ("W", "E/Q", "aim up/down", an actuator's name) for the panel. returns the length
i32 source_describe(Engine *engine, Source source, char *text, i32 capacity);
f32 actuator_strength(Engine *engine, Actuator *actuator);      // now / full: 0..1, past 1 with boost
// a free place for a new copy of object, used and filled with the object's size and life. 0 = none left
ObjectCopy *engine_add_copy(Engine *engine, i32 object);
void placing_place(Engine *engine, i32 placing); // takes its copies away and puts them down again
void placing_update(Engine *engine, i32 placing); // places again when count, spread, sizes or speeds changed
// one physics step of an emitter whose type emits objects: reads its input, sends out the copies it owes
void emitter_send_objects(Engine *engine, Emitter *emitter, real time_step);
void engine_start(Engine *engine); // every object at its start, actuators at rest, emitters without a trail
// one physics step: inputs are worked out, actuators ease toward them and push or turn their objects, physics moves them
FORCE_INLINE void engine_step(Engine *engine, real time_step);
// one drawn frame: objects draw their models, emitters fire
void engine_draw(Engine *engine, f32 alpha);
// ---- The panel (engine/panel.c)
// the parts' tree: each object with its actuators and emitters, then the actuator types and the emitter types
void engine_add_panel(Engine *engine);
// once per browser frame: does what a panel click asked for (new, copy, delete, rename, split, pick) or applies a
// loaded parts list. 1 = the parts changed, clear the panel and add it again (plat_ui_clear, then engine_add_panel)
i32 engine_update_panel(Engine *engine);

#endif
