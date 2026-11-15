#include "engine/engine.h"

// Objects (focus/engine.md, The model). Moved by physics as a point mass: forces from its push actuators,
// turning from its turn actuators (not physics yet, focus/engine.md open questions). engine_step runs them.

void object_start(Object *object) {
    object->particle.position = object->previous_position = object->start_position;
    object->particle.velocity = vector3_zero();
    object->particle.acceleration = vector3_zero();
    object->particle.force_total = vector3_zero();
    object->rotation = object->previous_rotation = quaternion_identity();
    object->aim = object_axis(object, vector3_create(0, 0, -1));
    object->aim_pitch = object->aim_yaw = object->tilt = 0;
}

Vector3 object_axis(Object *object, Vector3 local) { return quaternion_rotated_vector(object->rotation, local); }

Vector3 object_drawn_position(Object *object, f32 alpha) {
    return vector3_lerp(object->previous_position, object->particle.position, alpha);
}

Quaternion object_drawn_rotation(Object *object, f32 alpha) {
    return quaternion_lerp(object->previous_rotation, object->rotation, alpha);
}
