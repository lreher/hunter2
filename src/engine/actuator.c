#include "engine/engine.h"

// Actuators (focus/engine.md, The model). Every physics step each one reads its input and eases toward it:
//   turn: its speed now eases toward input * the type's speed
//   push: its force now eases toward input * the type's force (plus boost while its boost input is on), but never
//         runs weaker than the type's cutoff: when asked for more it lights straight at it, when asked for less
//         it goes out once it drops below it
// engine_step then turns and pushes its object with what it has now.

f32 actuator_strength(Engine *engine, Actuator *actuator) {
    ActuatorType *type = &engine->actuator_types[actuator->type];
    real full = type->kind == ACTUATOR_PUSH ? type->force : type->speed;
    return full > 0 ? actuator->now / full : 0;
}

// block first, then input, then fallback when input reads 0 (only while fallback_on)
FORCE_INLINE real actuator_read_input(Engine *engine, Actuator *actuator, Object *object) {
    if (source_read(engine, actuator->block, object) != 0) return 0;
    real input = source_read(engine, actuator->input, object);
    if (input == 0 && actuator->fallback_on) input = source_read(engine, actuator->fallback, object);
    return input;
}

FORCE_INLINE void actuator_ease(Engine *engine, Actuator *actuator, real time_step) {
    ActuatorType *type = &engine->actuator_types[actuator->type];
    Object *object = &engine->objects[actuator->object];
    real input = actuator_read_input(engine, actuator, object);
    real catch_up = type->response * time_step; // share of the gap closed this step
    if (catch_up > 1) catch_up = 1;
    if (type->kind == ACTUATOR_TURN) {
        actuator->now += (input * type->speed - actuator->now) * catch_up;
        return;
    }
    real wanted = input * type->force;
    if (input > 0 && source_read(engine, actuator->boost, object) != 0) wanted *= 1 + type->boost / 100;
    real cutoff = type->force * type->cutoff / 100;
    actuator->now += (wanted - actuator->now) * catch_up;
    if (actuator->now < cutoff) actuator->now = wanted >= cutoff ? cutoff : 0;
}
