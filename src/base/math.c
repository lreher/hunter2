#include "base/base.h"

V3 v3_create(real x, real y, real z) {
    V3 v = {x, y, z};
    return v;
}

V3 v3_zero(void) {
    return v3_create(0, 0, 0);
}

void v3_add(V3 *target, V3 v) {
    target->x += v.x;
    target->y += v.y;
    target->z += v.z;
}

V3 v3_added(V3 a, V3 b) {
    return v3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

void v3_subtract(V3 *target, V3 v) {
    target->x -= v.x;
    target->y -= v.y;
    target->z -= v.z;
}

V3 v3_subtracted(V3 a, V3 b) {
    return v3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

void v3_multiply(V3 *v, real scale) {
    v->x *= scale;
    v->y *= scale;
    v->z *= scale;
}

V3 v3_multiplied(V3 v, real scale) {
    return v3_create(v.x * scale, v.y * scale, v.z * scale);
}

void v3_add_scaled(V3 *target, V3 v, real scale) {
    target->x += v.x * scale;
    target->y += v.y * scale;
    target->z += v.z * scale;
}

void v3_invert(V3 *v) {
    v->x = -v->x;
    v->y = -v->y;
    v->z = -v->z;
}

real v3_scalar_product(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

V3 v3_vector_product(V3 a, V3 b) {
    return v3_create(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}

V3 v3_component_product(V3 a, V3 b) {
    return v3_create(a.x * b.x, a.y * b.y, a.z * b.z);
}

real v3_square_magnitude(V3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

real v3_magnitude(V3 v) {
    return real_sqrt(v3_square_magnitude(v));
}

void v3_normalize(V3 *v) {
    real length = v3_magnitude(*v);
    if (length > 0) v3_multiply(v, 1 / length);
}

V3 v3_lerp(V3 a, V3 b, real t) {
    return v3_create(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
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

M4 m4_identity(void) {
    M4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

M4 m4_multiplied(M4 a, M4 b) {
    M4 r;
    for (i32 col = 0; col < 4; col++) {
        for (i32 row = 0; row < 4; row++) {
            f32 sum = 0;
            for (i32 k = 0; k < 4; k++) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

M4 m4_perspective(f32 fov_y, f32 aspect, f32 near, f32 far) {
    f32 f = 1 / f32_tan(fov_y / 2);
    M4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = far / (near - far);
    r.m[11] = -1; // w = -z, the perspective divide
    r.m[14] = near * far / (near - far);
    return r;
}

M4 m4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    M4 r = m4_identity();
    r.m[0] = 2 / (right - left);
    r.m[5] = 2 / (top - bottom);
    r.m[10] = 1 / (near - far);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = near / (near - far);
    return r;
}

V3 m4_transform_point(M4 m, V3 p) {
    f32 out[4];
    for (i32 row = 0; row < 4; row++) out[row] = m.m[row] * p.x + m.m[4 + row] * p.y + m.m[8 + row] * p.z + m.m[12 + row];
    return v3_create(out[0] / out[3], out[1] / out[3], out[2] / out[3]);
}

M4 m4_look_at(V3 eye, V3 target, V3 up) {
    V3 forward = v3_subtracted(target, eye);
    v3_normalize(&forward);
    V3 right = v3_vector_product(forward, up);
    v3_normalize(&right);
    V3 camera_up = v3_vector_product(right, forward);

    // rows are the camera axes, so this turns world directions into camera directions
    M4 r = m4_identity();
    r.m[0] = right.x;
    r.m[4] = right.y;
    r.m[8] = right.z;
    r.m[1] = camera_up.x;
    r.m[5] = camera_up.y;
    r.m[9] = camera_up.z;
    r.m[2] = -forward.x;
    r.m[6] = -forward.y;
    r.m[10] = -forward.z;
    r.m[12] = -v3_scalar_product(right, eye);
    r.m[13] = -v3_scalar_product(camera_up, eye);
    r.m[14] = v3_scalar_product(forward, eye);
    return r;
}
