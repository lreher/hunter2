#include "base/base.h"

Vector3 vector3_create(real x, real y, real z) {
    Vector3 v = {x, y, z};
    return v;
}

Vector3 vector3_zero(void) {
    return vector3_create(0, 0, 0);
}

void vector3_add(Vector3 *target, Vector3 v) {
    target->x += v.x;
    target->y += v.y;
    target->z += v.z;
}

Vector3 vector3_added(Vector3 a, Vector3 b) {
    return vector3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

void vector3_subtract(Vector3 *target, Vector3 v) {
    target->x -= v.x;
    target->y -= v.y;
    target->z -= v.z;
}

Vector3 vector3_subtracted(Vector3 a, Vector3 b) {
    return vector3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

void vector3_multiply(Vector3 *v, real scale) {
    v->x *= scale;
    v->y *= scale;
    v->z *= scale;
}

Vector3 vector3_multiplied(Vector3 v, real scale) {
    return vector3_create(v.x * scale, v.y * scale, v.z * scale);
}

void vector3_add_scaled(Vector3 *target, Vector3 v, real scale) {
    target->x += v.x * scale;
    target->y += v.y * scale;
    target->z += v.z * scale;
}

void vector3_invert(Vector3 *v) {
    v->x = -v->x;
    v->y = -v->y;
    v->z = -v->z;
}

real vector3_scalar_product(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 vector3_vector_product(Vector3 a, Vector3 b) {
    return vector3_create(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}

Vector3 vector3_component_product(Vector3 a, Vector3 b) {
    return vector3_create(a.x * b.x, a.y * b.y, a.z * b.z);
}

real vector3_square_magnitude(Vector3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

real vector3_magnitude(Vector3 v) {
    return real_sqrt(vector3_square_magnitude(v));
}

void vector3_normalize(Vector3 *v) {
    real length = vector3_magnitude(*v);
    if (length > 0) vector3_multiply(v, 1 / length);
}

Vector3 vector3_lerp(Vector3 a, Vector3 b, real t) {
    return vector3_create(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

// Taylor series up to x^11, after folding x into [-pi/2, pi/2]. Error there is under 1e-7.
f32 f32_sin(f32 x) {
    x -= 2 * F32_PI * __builtin_floorf(x / (2 * F32_PI) + 0.5f); // now in [-pi, pi]
    if (x > F32_PI / 2) x = F32_PI - x;                          // sin(pi - x) = sin(x)
    if (x < -F32_PI / 2) x = -F32_PI - x;
    f32 x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}

f32 f32_cos(f32 x) {
    return f32_sin(x + F32_PI / 2);
}

f32 f32_tan(f32 x) {
    return f32_sin(x) / f32_cos(x);
}

Matrix4 matrix4_identity(void) {
    Matrix4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

Matrix4 matrix4_multiplied(Matrix4 a, Matrix4 b) {
    Matrix4 r;
    for (i32 col = 0; col < 4; col++) {
        for (i32 row = 0; row < 4; row++) {
            f32 sum = 0;
            for (i32 k = 0; k < 4; k++) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

Matrix4 matrix4_perspective(f32 fov_y, f32 aspect, f32 near, f32 far) {
    f32 f = 1 / f32_tan(fov_y / 2);
    Matrix4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = far / (near - far);
    r.m[11] = -1; // w = -z, the perspective divide
    r.m[14] = near * far / (near - far);
    return r;
}

Matrix4 matrix4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    Matrix4 r = matrix4_identity();
    r.m[0] = 2 / (right - left);
    r.m[5] = 2 / (top - bottom);
    r.m[10] = 1 / (near - far);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = near / (near - far);
    return r;
}

Vector3 matrix4_transform_point(Matrix4 m, Vector3 p) {
    f32 out[4];
    for (i32 row = 0; row < 4; row++) out[row] = m.m[row] * p.x + m.m[4 + row] * p.y + m.m[8 + row] * p.z + m.m[12 + row];
    return vector3_create(out[0] / out[3], out[1] / out[3], out[2] / out[3]);
}

Matrix4 matrix4_look_at(Vector3 eye, Vector3 target, Vector3 up) {
    Vector3 forward = vector3_subtracted(target, eye);
    vector3_normalize(&forward);
    Vector3 right = vector3_vector_product(forward, up);
    vector3_normalize(&right);
    Vector3 camera_up = vector3_vector_product(right, forward);

    // rows are the camera axes, so this turns world directions into camera directions
    Matrix4 r = matrix4_identity();
    r.m[0] = right.x;
    r.m[4] = right.y;
    r.m[8] = right.z;
    r.m[1] = camera_up.x;
    r.m[5] = camera_up.y;
    r.m[9] = camera_up.z;
    r.m[2] = -forward.x;
    r.m[6] = -forward.y;
    r.m[10] = -forward.z;
    r.m[12] = -vector3_scalar_product(right, eye);
    r.m[13] = -vector3_scalar_product(camera_up, eye);
    r.m[14] = vector3_scalar_product(forward, eye);
    return r;
}

Quaternion quaternion_identity(void) {
    Quaternion result = {1, 0, 0, 0};
    return result;
}

Quaternion quaternion_from_axis_angle(Vector3 axis, real angle) {
    real sine = f32_sin(angle / 2);
    Quaternion result = {f32_cos(angle / 2), axis.x * sine, axis.y * sine, axis.z * sine};
    return result;
}

// Hamilton product (book 9.2.3, eq 9.8)
Quaternion quaternion_multiplied(Quaternion a, Quaternion b) {
    Quaternion result;
    result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    result.y = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z;
    result.z = a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x;
    return result;
}

void quaternion_normalize(Quaternion *q) {
    real square_length = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
    if (square_length == 0) { // no rotation left to keep, fall back to none (book does the same)
        *q = quaternion_identity();
        return;
    }
    real inverse_length = 1 / real_sqrt(square_length);
    q->w *= inverse_length;
    q->x *= inverse_length;
    q->y *= inverse_length;
    q->z *= inverse_length;
}

// book 9.4.3: q' = q + (time_step / 2) * w * q, with w = (0, angular_velocity). good for the small turns of one step
void quaternion_add_rotation(Quaternion *q, Vector3 angular_velocity, real time_step) {
    Quaternion spin = {0, angular_velocity.x, angular_velocity.y, angular_velocity.z};
    Quaternion change = quaternion_multiplied(spin, *q);
    real half_step = time_step / 2;
    q->w += change.w * half_step;
    q->x += change.x * half_step;
    q->y += change.y * half_step;
    q->z += change.z * half_step;
    quaternion_normalize(q);
}

// q v q* written out: t = 2 (axis part x v), v' = v + w t + (axis part x t)
Vector3 quaternion_rotated_vector(Quaternion q, Vector3 v) {
    Vector3 axis_part = vector3_create(q.x, q.y, q.z);
    Vector3 t = vector3_multiplied(vector3_vector_product(axis_part, v), 2);
    Vector3 result = vector3_added(v, vector3_multiplied(t, q.w));
    vector3_add(&result, vector3_vector_product(axis_part, t));
    return result;
}

// q and -q are the same rotation. flip b when it points the other way, or the blend goes the long way round
Quaternion quaternion_lerp(Quaternion a, Quaternion b, real t) {
    real dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    real sign = dot < 0 ? -1 : 1;
    Quaternion result = {a.w + (sign * b.w - a.w) * t, a.x + (sign * b.x - a.x) * t,
                         a.y + (sign * b.y - a.y) * t, a.z + (sign * b.z - a.z) * t};
    quaternion_normalize(&result);
    return result;
}

// columns are where the x, y, z axes end up after the turn (book 9.4.4), the last column is the position
Matrix4 matrix4_from_rotation_position(Quaternion rotation, Vector3 position) {
    Vector3 x_axis = quaternion_rotated_vector(rotation, vector3_create(1, 0, 0));
    Vector3 y_axis = quaternion_rotated_vector(rotation, vector3_create(0, 1, 0));
    Vector3 z_axis = quaternion_rotated_vector(rotation, vector3_create(0, 0, 1));
    Matrix4 result = matrix4_identity();
    result.m[0] = x_axis.x;
    result.m[1] = x_axis.y;
    result.m[2] = x_axis.z;
    result.m[4] = y_axis.x;
    result.m[5] = y_axis.y;
    result.m[6] = y_axis.z;
    result.m[8] = z_axis.x;
    result.m[9] = z_axis.y;
    result.m[10] = z_axis.z;
    result.m[12] = position.x;
    result.m[13] = position.y;
    result.m[14] = position.z;
    return result;
}
