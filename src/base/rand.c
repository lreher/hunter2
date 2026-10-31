#include "base/base.h"

static u32 rand_state = 1;

void rand_seed(u32 seed) {
    rand_state = seed ? seed : 1; // xorshift gets stuck on 0
}

u32 rand_u32(void) {
    u32 x = rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rand_state = x;
    return x;
}

// [min, max)
f32 rand_f32(f32 min, f32 max) {
    f32 t = (f32)(rand_u32() >> 8) * (1.0f / 16777216.0f);
    return min + t * (max - min);
}

// random direction, every direction equally likely.
// picks points in the cube until one lands inside the unit sphere, so corners are not favored (cphiz normalized cube points)
V3 rand_unit_v3(void) {
    for (;;) {
        V3 v = v3_create(rand_f32(-1, 1), rand_f32(-1, 1), rand_f32(-1, 1));
        real square_length = v3_square_magnitude(v);
        if (square_length > 0.0001f && square_length <= 1) {
            v3_multiply(&v, 1 / real_sqrt(square_length));
            return v;
        }
    }
}
