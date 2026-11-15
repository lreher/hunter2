#include "engine/engine.h"
#include "platform/platform.h"

// Inputs (focus/engine.md, How listening works): each part has a Source and reads it when it runs.
// Mouse aim and tilt are worked out by the engine, once per frame (aim) and once per step (aim_pitch, aim_yaw, tilt).

InputTuning input_tuning = {
    .aim_sensitivity = 0.002f,
    .aim_full_turn_angle = 0.6f, // about 35 degrees
    .aim_limit = 1.0f,           // about 57 degrees
    .aim_invert_pitch = 0,
    .level_angle = 0.8f,
    .level_strength = 0.5f,
};

static real input_clamp_unit(real value) { return value > 1 ? 1 : value < -1 ? -1 : value; }

f32 source_read(Engine *engine, Source source, Object *object) {
    switch (source.kind) {
    case SOURCE_ALWAYS: return 1;
    case SOURCE_ACTUATOR:
        if (source.index < 0 || source.index >= engine->actuator_count) return 0;
        return actuator_strength(engine, &engine->actuators[source.index]);
    }
    if (!engine->inputs_on) return 0; // the rest is the pilot's
    switch (source.kind) {
    case SOURCE_KEY: return plat_key_down(source.index) ? 1 : 0;
    case SOURCE_KEY_PAIR: return (plat_key_down(source.index) ? 1 : 0) - (plat_key_down(source.negative) ? 1 : 0);
    case SOURCE_AIM_PITCH: return object ? object->aim_pitch : 0;
    case SOURCE_AIM_YAW: return object ? object->aim_yaw : 0;
    case SOURCE_TILT: return object ? object->tilt : 0;
    }
    return 0;
}

i32 source_describe(Engine *engine, Source source, char *text, i32 capacity) {
    switch (source.kind) {
    case SOURCE_ALWAYS: return str_append(text, capacity, "always");
    case SOURCE_KEY: return str_append(text, capacity, plat_key_name(source.index));
    case SOURCE_KEY_PAIR:
        str_append(text, capacity, plat_key_name(source.index));
        str_append(text, capacity, "/");
        return str_append(text, capacity, plat_key_name(source.negative));
    case SOURCE_AIM_PITCH: return str_append(text, capacity, "aim up/down");
    case SOURCE_AIM_YAW: return str_append(text, capacity, "aim left/right");
    case SOURCE_TILT: return str_append(text, capacity, "level");
    case SOURCE_ACTUATOR:
        if (source.index < 0 || source.index >= engine->actuator_count) return str_append(text, capacity, "?");
        return str_append(text, capacity, engine->actuators[source.index].name);
    }
    return str_len(text);
}

void object_update_aim(Object *object, Vector3 camera_up, i32 on) {
    Vector3 forward = object_axis(object, vector3_create(0, 0, -1));
    if (!on) {
        object->aim = forward;
        return;
    }
    Vector3 up = camera_up;
    Vector3 right = vector3_vector_product(object->aim, up);
    vector3_normalize(&right);
    real sensitivity = input_tuning.aim_sensitivity;
    real pitch_sign = input_tuning.aim_invert_pitch ? 1 : -1;
    Quaternion turn = quaternion_multiplied(quaternion_from_axis_angle(up, -plat_mouse_dx() * sensitivity),
                                            quaternion_from_axis_angle(right, pitch_sign * plat_mouse_dy() * sensitivity));
    object->aim = quaternion_rotated_vector(turn, object->aim);
    vector3_normalize(&object->aim);

    // keep it within limit of the nose: same side, pulled in to the edge
    real limit = input_tuning.aim_limit;
    real along = vector3_scalar_product(object->aim, forward);
    if (along < f32_cos(limit)) {
        Vector3 side = vector3_subtracted(object->aim, vector3_multiplied(forward, along));
        if (vector3_square_magnitude(side) < 1e-8f) side = up; // straight behind: pick a side
        vector3_normalize(&side);
        object->aim = vector3_added(vector3_multiplied(forward, f32_cos(limit)), vector3_multiplied(side, f32_sin(limit)));
    }
}

// Before its actuators run, every step: aim_pitch, aim_yaw and tilt from where the object points now.
// Aim: one turn straight at the aim. (aim_right, aim_up) is the way the nose has to go, the angle off sets how hard.
// The pitch and yaw actuators' speeds differ, so the top speed in that direction is on the ellipse between them:
// speed = 1 / sqrt((up / pitch_speed)^2 + (right / yaw_speed)^2) for the unit direction (right, up).
// Both inputs then get the same share of it, so the turn keeps its direction and the path stays straight.
// Tilt: the world's up as the object sees it, looking down its nose: how far it leans to the right of its own up.
// Pointing straight up or down there is no lean to fix, so it fades out by how level the nose is.
FORCE_INLINE void input_work_out(Engine *engine, i32 object_index) {
    Object *object = &engine->objects[object_index];
    real pitch_speed = 1, yaw_speed = 1; // of the turn actuators that read the aim
    for (i32 i = 0; i < engine->actuator_count; i++) {
        Actuator *actuator = &engine->actuators[i];
        if (actuator->object != object_index) continue;
        ActuatorType *type = &engine->actuator_types[actuator->type];
        if (actuator->input.kind == SOURCE_AIM_PITCH && type->speed > 0) pitch_speed = type->speed;
        if (actuator->input.kind == SOURCE_AIM_YAW && type->speed > 0) yaw_speed = type->speed;
    }
    Vector3 right = object_axis(object, vector3_create(1, 0, 0));
    Vector3 up = object_axis(object, vector3_create(0, 1, 0));
    Vector3 forward = object_axis(object, vector3_create(0, 0, -1));
    real aim_right = vector3_scalar_product(object->aim, right);
    real aim_up = vector3_scalar_product(object->aim, up);
    real aim_ahead = vector3_scalar_product(object->aim, forward);
    object->aim_pitch = object->aim_yaw = 0;
    real off_length = real_sqrt(aim_right * aim_right + aim_up * aim_up);
    if (off_length > 1e-6f) {
        real up_part = aim_up / off_length, right_part = aim_right / off_length;
        real share = f32_atan2(off_length, aim_ahead) / input_tuning.aim_full_turn_angle;
        if (share > 1) share = 1;
        real per_pitch = up_part / pitch_speed, per_yaw = right_part / yaw_speed;
        real speed = 1 / real_sqrt(per_pitch * per_pitch + per_yaw * per_yaw);
        object->aim_pitch = input_clamp_unit(per_pitch * speed * share);
        object->aim_yaw = input_clamp_unit(per_yaw * speed * share);
    }
    Vector3 world_up = vector3_create(0, 1, 0);
    Vector3 across = vector3_subtracted(world_up, vector3_multiplied(forward, vector3_scalar_product(world_up, forward)));
    real level = vector3_magnitude(across); // 1 = nose level, 0 = straight up or down
    real lean = f32_atan2(vector3_scalar_product(across, right), vector3_scalar_product(across, up));
    object->tilt = input_clamp_unit(lean / input_tuning.level_angle) * input_tuning.level_strength * level;
}
