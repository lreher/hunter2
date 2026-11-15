#include "engine/engine.h"
#include "platform/platform.h"

// Emitters (focus/engine.md, The model). Each drawn frame (engine_draw) an emitter reads its input, takes its type's
// settings scaled by it, puts itself where its object is now, and asks render for sparks:
//   rate = the type's rate * input
//   spark speed = base speed + (the type's speed - base speed) * input
//   cone = the type's cone / (1 + cone squeeze * (input - 1)): more input, a little narrower
// Sparks are world sparks (user): once out they fly free with their shoot speed + the object's velocity at that moment.

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
        .memory = emitter->memory,
    };
    render_emit(&render_emitter);
    emitter->memory = render_emitter.memory;
}

void emitter_reset(Emitter *emitter) { emitter->memory.started = 0; }

// rate is per emitter now (it used to be the whole main thruster, split over its nozzles), so its label changed:
// an old save's "rate /s" does not load into it
void emitter_type_add_sliders(EmitterType *type) {
    plat_ui_group(type->name);
    plat_ui_slider_log("rate /s per emitter", 100, 1000000, 100, &type->rate); // log (user), so no 0
    plat_ui_slider("life s", 0.05f, 10, 0.05f, &type->lifetime);
    plat_ui_slider("life spread s", 0, 5, 0.05f, &type->lifetime_spread);
    plat_ui_slider("speed m/s", 0, 200, 0.5f, &type->speed);
    plat_ui_slider("base speed m/s", 0, 200, 0.5f, &type->base_speed);
    plat_ui_slider("speed spread m/s", 0, 100, 0.5f, &type->speed_spread);
    plat_ui_slider("cone rad", 0, 3.14f, 0.01f, &type->cone);
    plat_ui_slider("cone squeeze", 0, 1, 0.01f, &type->cone_squeeze);
    plat_ui_slider("size m", 0.005f, 1, 0.005f, &type->size);
}
