#include "engine/engine.h"
#include "platform/platform.h"

// The engine runs a game's parts (focus/engine.md). Per physics step (engine_step):
//   1. every object: inputs worked out (aim, tilt) from where it points now
//   2. every actuator: reads its input, eases toward it
//   3. every object: its turn actuators turn it, its push actuators push it (along its new axes), drag, physics moves it
// Per drawn frame (engine_draw): objects drawn blended between steps, then every emitter fires.

void engine_start(Engine *engine) {
    for (i32 i = 0; i < engine->object_count; i++) object_start(&engine->objects[i]);
    for (i32 i = 0; i < engine->actuator_count; i++) engine->actuators[i].now = 0;
    for (i32 i = 0; i < engine->emitter_count; i++) emitter_reset(&engine->emitters[i]);
}

FORCE_INLINE void engine_step(Engine *engine, real time_step) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        object->previous_position = object->particle.position;
        object->previous_rotation = object->rotation;
        input_work_out(engine, i);
    }
    for (i32 i = 0; i < engine->actuator_count; i++) actuator_ease(engine, &engine->actuators[i], time_step);
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        Vector3 turn = vector3_zero(); // rad/s in its own axes
        for (i32 j = 0; j < engine->actuator_count; j++) {
            Actuator *actuator = &engine->actuators[j];
            if (actuator->object == i && engine->actuator_types[actuator->type].kind == ACTUATOR_TURN)
                vector3_add_scaled(&turn, actuator->direction, actuator->now);
        }
        quaternion_add_rotation(&object->rotation, object_axis(object, turn), time_step);
        for (i32 j = 0; j < engine->actuator_count; j++) {
            Actuator *actuator = &engine->actuators[j];
            if (actuator->object == i && engine->actuator_types[actuator->type].kind == ACTUATOR_PUSH)
                particle_add_force(&object->particle, vector3_multiplied(object_axis(object, actuator->direction), actuator->now));
        }
        // drag as damping: the share of velocity kept this step
        real drag = object->drag;
        if (source_read(engine, object->drag_cut_input, object) != 0) drag *= 1 - object->drag_cut / 100;
        real kept = 1 - drag * time_step;
        object->particle.damping = kept > 0 ? kept : 0;
        particle_set_mass(&object->particle, object->mass);
        particle_integrate(&object->particle, time_step);
    }
}

void engine_draw(Engine *engine, f32 alpha) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->model < 0) continue;
        Vector3 scale = vector3_create(object->size, object->size, object->size);
        render_draw_model(object->model, object_drawn_position(object, alpha), object_drawn_rotation(object, alpha), scale,
                          object->color.x, object->color.y, object->color.z);
    }
    for (i32 i = 0; i < engine->emitter_count; i++) {
        Emitter *emitter = &engine->emitters[i];
        Object *object = 0;
        Vector3 position = vector3_zero(), velocity = vector3_zero();
        Quaternion rotation = quaternion_identity();
        if (emitter->object >= 0 && emitter->object < engine->object_count) {
            object = &engine->objects[emitter->object];
            position = object_drawn_position(object, alpha);
            rotation = object_drawn_rotation(object, alpha);
            velocity = object->particle.velocity;
        }
        real input = source_read(engine, emitter->input, object);
        if (input < 0) input = 0;
        emitter_fire(emitter, &engine->emitter_types[emitter->type], position, rotation, velocity, input);
    }
}

// one group per part, titled with its name (the save key is group + slider name, web/ui.js)
void engine_add_sliders(Engine *engine) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        plat_ui_group(object->name);
        plat_ui_slider("drag /s", 0, 3, 0.01f, &object->drag);
        if (object->drag_cut_input.kind != SOURCE_NONE) plat_ui_slider("drag cut %", 0, 100, 1, &object->drag_cut);
    }
    for (i32 i = 0; i < engine->actuator_type_count; i++) {
        ActuatorType *type = &engine->actuator_types[i];
        plat_ui_group(type->name);
        if (type->kind == ACTUATOR_TURN) {
            plat_ui_slider("speed rad/s", 0.1f, 8, 0.05f, &type->speed);
            plat_ui_slider("response /s", 0.5f, 30, 0.1f, &type->response);
            continue;
        }
        plat_ui_slider("force N", 0, 100000, 500, &type->force);
        i32 boosted = 0; // only when an actuator of this type has a boost input
        for (i32 j = 0; j < engine->actuator_count; j++)
            if (engine->actuators[j].type == i && engine->actuators[j].boost.kind != SOURCE_NONE) boosted = 1;
        if (boosted) plat_ui_slider("boost %", 0, 300, 1, &type->boost);
        plat_ui_slider("response /s", 0.5f, 30, 0.1f, &type->response);
        plat_ui_slider("cutoff %", 0, 50, 1, &type->cutoff);
    }
    plat_ui_group("particles"); // shared by every spark
    plat_ui_slider_log("max particles", 1024, 1048576, 1024, &render_particle_tuning.capacity); // log (user)
    plat_ui_slider("streak s", 0, 0.2f, 0.002f, &render_particle_tuning.stretch);
    plat_ui_slider("particle drag /s", 0, 10, 0.05f, &render_particle_tuning.drag);
    for (i32 i = 0; i < engine->emitter_type_count; i++) emitter_type_add_sliders(&engine->emitter_types[i]);
}
