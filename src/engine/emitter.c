#include "engine/engine.h"
#include "platform/platform.h"

// Emitters (focus/engine.md, The model). Each drawn frame (engine_draw) an emitter reads its input, takes its type's
// settings scaled by it, puts itself where its object is now, and asks render for sparks:
//   rate = the type's rate * input
//   spark speed = base speed + (the type's speed - base speed) * input
//   cone = the type's cone / (1 + cone squeeze * (input - 1)): more input, a little narrower
// Sparks are world sparks (user): once out they fly free with their shoot speed + the object's velocity at that moment.
// An emitter type can send out copies of an object instead (bullets, user): emitter_send_objects, every physics step.

void emitter_fire(Emitter *emitter, const EmitterType *type, Vector3 position, Quaternion rotation, Vector3 velocity,
                  f32 input) {
    real squeeze = 1 + type->cone_squeeze * (input - 1);
    RenderEmitter render_emitter = {
        .local = 0,
        .position = vector3_added(position, quaternion_rotated_vector(rotation, emitter->position)),
        .rotation = rotation,
        .direction = quaternion_rotated_vector(rotation, emitter->direction),
        .velocity = velocity,
        .rate = type->rate * input,
        .speed = type->base_speed + (type->speed - type->base_speed) * input,
        .speed_spread = type->speed_spread,
        .cone = type->cone / (squeeze > 0.1f ? squeeze : 0.1f),
        .lifetime = type->lifetime,
        .lifetime_spread = type->lifetime_spread,
        .size = type->size,
        .color_start = type->color_start,
        .color_end = type->color_end,
        .type = emitter->type, // its sparks keep the type, so drag and streak reach them all their life
        .drag = type->drag,
        .stretch = type->stretch,
        .memory = emitter->memory,
    };
    render_emit(&render_emitter);
    emitter->memory = render_emitter.memory;
}

void emitter_reset(Emitter *emitter) { emitter->memory.started = 0; }

// Copies go out at the type's rate * input. The first one leaves the moment the input comes on, then one each time a
// whole one is owed. Each starts at the emitter's spot with the object's velocity + speed (give or take speed spread)
// along the emitter's direction, turned up to cone off it. It starts out from where the emitter was a step ago, so it
// is drawn leaving the emitter, not ahead of it
void emitter_send_objects(Engine *engine, Emitter *emitter, real time_step) {
    EmitterType *type = &engine->emitter_types[emitter->type];
    if (type->object < 0 || type->object >= engine->object_count) return;
    Object *copied = &engine->objects[type->object];
    Object *object = 0;
    if (emitter->object >= 0 && emitter->object < engine->object_count) object = &engine->objects[emitter->object];
    real input = source_read(engine, emitter->input, object);
    if (input <= 0) {
        emitter->owed = 1;
        return;
    }
    Vector3 position = emitter->position, previous_position = emitter->position, direction = emitter->direction;
    Vector3 velocity = vector3_zero();
    Quaternion rotation = quaternion_identity();
    if (object) {
        rotation = object->rotation;
        position = vector3_added(object->particle.position, quaternion_rotated_vector(rotation, emitter->position));
        previous_position = vector3_added(object->previous_position,
                                          quaternion_rotated_vector(object->previous_rotation, emitter->position));
        direction = quaternion_rotated_vector(rotation, emitter->direction);
        velocity = object->particle.velocity;
    }
    while (emitter->owed >= 1) {
        ObjectCopy *copy = engine_add_copy(engine, type->object);
        if (!copy) break;
        emitter->owed -= 1;
        Vector3 way = direction; // turned off the direction by up to cone (about, fine for small cones)
        vector3_add_scaled(&way, rand_unit_vector3(), rand_f32(0, type->cone));
        vector3_normalize(&way);
        real speed = type->speed + rand_f32(-type->speed_spread, type->speed_spread);
        copy->rotation = rotation;
        copy->from = emitter->object;
        copy->particle.position = position;
        copy->previous_position = previous_position;
        copy->particle.velocity = vector3_added(velocity, vector3_multiplied(way, speed));
        particle_set_mass(&copy->particle, copied->mass > 0 ? copied->mass : 0.001f);
    }
    if (emitter->owed > 1) emitter->owed = 1; // no places left: it does not save them up
    emitter->owed += type->rate * input * time_step;
}

// rate is per emitter now (it used to be the whole main thruster, split over its nozzles), so its label changed:
// an old save's "rate /s" does not load into it
// a type that sends out objects: rate, speed, cone. the object has the rest (objects in the panel)
static void emitter_type_add_object_sliders(EmitterType *type) {
    plat_ui_block("rate");
    plat_ui_slider("rate /s per emitter", 0.5f, 100, 0.5f, &type->rate);
    plat_ui_block_end();
    plat_ui_block("speed");
    plat_ui_slider("speed m/s", 0, 2000, 5, &type->speed);
    plat_ui_slider("speed spread m/s", 0, 100, 0.5f, &type->speed_spread);
    plat_ui_block_end();
    plat_ui_block("cone");
    plat_ui_slider("cone rad", 0, 0.5f, 0.001f, &type->cone);
    plat_ui_block_end();
}

void emitter_type_add_sliders(EmitterType *type) { // in blocks (user): rate, life, speed, cone, size, color
    if (type->emits == EMITS_OBJECTS) {
        emitter_type_add_object_sliders(type);
        return;
    }
    plat_ui_block("rate");
    plat_ui_slider_log("rate /s per emitter", 100, 1000000, 100, &type->rate); // log (user), so no 0
    plat_ui_block_end();
    plat_ui_block("life");
    plat_ui_slider("life s", 0.05f, 10, 0.05f, &type->lifetime);
    plat_ui_slider("life spread s", 0, 5, 0.05f, &type->lifetime_spread);
    plat_ui_block_end();
    plat_ui_block("speed"); // drag here too: it slows the sparks down
    plat_ui_slider("speed m/s", 0, 200, 0.5f, &type->speed);
    plat_ui_slider("base speed m/s", 0, 200, 0.5f, &type->base_speed);
    plat_ui_slider("speed spread m/s", 0, 100, 0.5f, &type->speed_spread);
    plat_ui_slider("drag /s", 0, 10, 0.05f, &type->drag);
    plat_ui_block_end();
    plat_ui_block("cone");
    plat_ui_slider("cone rad", 0, 3.14f, 0.01f, &type->cone);
    plat_ui_slider("cone squeeze", 0, 1, 0.01f, &type->cone_squeeze);
    plat_ui_block_end();
    plat_ui_block("size");
    plat_ui_slider("size m", 0.005f, 1, 0.005f, &type->size);
    plat_ui_slider("streak s", 0, 0.2f, 0.002f, &type->stretch);
    plat_ui_block_end();
    plat_ui_block("color"); // a spark fades from one to the other over its life
    plat_ui_color("color at start", &type->color_start.x);
    plat_ui_color("color at end", &type->color_end.x);
    plat_ui_block_end();
}
