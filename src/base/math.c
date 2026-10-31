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
