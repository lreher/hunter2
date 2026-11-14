#ifndef BASE_H
#define BASE_H

typedef signed char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

// physics precision. switch to f64 here if we ever need it
typedef f32 real;
#define real_sqrt __builtin_sqrtf
#define REAL_MAX 3.402823466e+38f

// Forces a function into its caller. Chrome runs the hot demo loops much slower as their own functions (see focus/physics-engine.md)
#define FORCE_INLINE __attribute__((always_inline)) static inline

typedef struct {
    real x, y, z;
} Vector3;

// math.c. functions that change their first argument are vector3_verb, ones that return a new value are vector3_verbed
Vector3 vector3_create(real x, real y, real z);
Vector3 vector3_zero(void);
void vector3_add(Vector3 *target, Vector3 v);
Vector3 vector3_added(Vector3 a, Vector3 b);
void vector3_subtract(Vector3 *target, Vector3 v);
Vector3 vector3_subtracted(Vector3 a, Vector3 b);
void vector3_multiply(Vector3 *v, real scale);
Vector3 vector3_multiplied(Vector3 v, real scale);
void vector3_add_scaled(Vector3 *target, Vector3 v, real scale);
void vector3_invert(Vector3 *v);
real vector3_scalar_product(Vector3 a, Vector3 b);     // dot product
Vector3 vector3_vector_product(Vector3 a, Vector3 b);       // cross product
Vector3 vector3_component_product(Vector3 a, Vector3 b);    // a.x*b.x, a.y*b.y, a.z*b.z
real vector3_square_magnitude(Vector3 v);
real vector3_magnitude(Vector3 v);
void vector3_normalize(Vector3 *v);
Vector3 vector3_lerp(Vector3 a, Vector3 b, real t);

// math.c, for rendering (f32, not real). own trig because there is no libc
#define F32_PI 3.14159265f

f32 f32_sin(f32 x);
f32 f32_cos(f32 x);
f32 f32_tan(f32 x);

// 4x4 matrix, column major (m[column * 4 + row]), the layout WGSL's mat4x4f expects
typedef struct {
    f32 m[16];
} M4;

M4 m4_identity(void);
M4 m4_multiplied(M4 a, M4 b); // a * b: applies b first, then a
// right handed, camera looks down -z. depth goes 0 at near to 1 at far (WebGPU)
M4 m4_perspective(f32 fov_y, f32 aspect, f32 near, f32 far);
M4 m4_look_at(Vector3 eye, Vector3 target, Vector3 up);
// same axes and depth range as m4_perspective, without the perspective (parallel rays, for the sun)
M4 m4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
Vector3 m4_transform_point(M4 m, Vector3 p); // m * (p, 1), divided by w

// rand.c. xorshift32, same seed = same numbers
void rand_seed(u32 seed);
u32 rand_u32(void);
f32 rand_f32(f32 min, f32 max);
Vector3 rand_unit_vector3(void);

// str.c
i32 str_len(const char *s);
i32 str_append(char *dst, i32 cap, const char *src);  // returns new length, always NUL terminated
i32 str_append_i32(char *dst, i32 cap, i32 value);

#endif
