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

typedef struct {
    real x, y, z;
} V3;

// math.c. functions that change their first argument are v3_verb, ones that return a new value are v3_verbed
V3 v3_create(real x, real y, real z);
V3 v3_zero(void);
void v3_add(V3 *target, V3 v);
V3 v3_added(V3 a, V3 b);
void v3_subtract(V3 *target, V3 v);
V3 v3_subtracted(V3 a, V3 b);
void v3_multiply(V3 *v, real scale);
V3 v3_multiplied(V3 v, real scale);
void v3_add_scaled(V3 *target, V3 v, real scale);
void v3_invert(V3 *v);
real v3_scalar_product(V3 a, V3 b);     // dot product
V3 v3_vector_product(V3 a, V3 b);       // cross product
V3 v3_component_product(V3 a, V3 b);    // a.x*b.x, a.y*b.y, a.z*b.z
real v3_square_magnitude(V3 v);
real v3_magnitude(V3 v);
void v3_normalize(V3 *v);
V3 v3_lerp(V3 a, V3 b, real t);

// rand.c. xorshift32, same seed = same numbers
void rand_seed(u32 seed);
u32 rand_u32(void);
f32 rand_f32(f32 min, f32 max);
V3 rand_unit_v3(void);

// str.c
i32 str_len(const char *s);
i32 str_append(char *dst, i32 cap, const char *src);  // returns new length, always NUL terminated
i32 str_append_i32(char *dst, i32 cap, i32 value);

#endif
