#include "engine/engine.h"

// Placings: part of the universe (focus/engine.md). placing_place puts a placing's copies down: engine_start does it
// for every placing, the panel's "again" for one. Each copy gets a random size, a random spot inside the ball (tried
// again where it would overlap a copy already down or come within PLACING_CLEAR of an object in the world, given up
// after PLACING_TRIES), and a random speed in a random direction. mostly_small and small_faster bend the sizes and
// speeds: many small fast ones, a few big slow ones.

#define PLACING_TRIES 100
#define PLACING_CLEAR 50.0f // m between a copy's edge and an object in the world (the ship starts in the middle)

static i32 placing_spot_free(Engine *engine, Vector3 spot, real radius) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->copies_only) continue;
        real reach = radius + PLACING_CLEAR;
        if (vector3_square_magnitude(vector3_subtracted(spot, object->particle.position)) < reach * reach) return 0;
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) {
        ObjectCopy *copy = &engine->copies[i];
        if (!copy->used || copy->placing < 0) continue;
        real reach = radius + copy->size / 2;
        if (vector3_square_magnitude(vector3_subtracted(spot, copy->particle.position)) < reach * reach) return 0;
    }
    return 1;
}

static void placing_numbers(Engine *engine, Placing *placing, f32 *numbers) {
    numbers[0] = placing->count;
    numbers[1] = engine->spread;
    numbers[2] = placing->size_min;
    numbers[3] = placing->size_max;
    numbers[4] = placing->speed_min;
    numbers[5] = placing->speed_max;
    numbers[6] = placing->mostly_small;
    numbers[7] = placing->small_faster;
}

void placing_update(Engine *engine, i32 index) {
    f32 numbers[8];
    placing_numbers(engine, &engine->placings[index], numbers);
    for (i32 i = 0; i < 8; i++)
        if (numbers[i] != engine->placings[index].placed_with[i]) {
            placing_place(engine, index);
            return;
        }
}

void placing_place(Engine *engine, i32 index) {
    Placing *placing = &engine->placings[index];
    placing_numbers(engine, placing, placing->placed_with);
    for (i32 i = 0; i < engine->copy_capacity; i++)
        if (engine->copies[i].used && engine->copies[i].placing == index) engine->copies[i].used = 0;
    if (placing->object < 0 || placing->object >= engine->object_count) return;
    real half = engine->spread / 2;
    for (i32 n = 0; n < (i32)placing->count; n++) {
        // mostly small bends the pick toward min: from random (even) to random^5 (at 1 half of them in the lowest 3%)
        real pick = rand_f32(0, 1);
        real pick_fifth = pick * pick * pick * pick * pick;
        real bigness = pick + (pick_fifth - pick) * placing->mostly_small; // 0 = min, 1 = max
        real size = placing->size_min + (placing->size_max - placing->size_min) * bigness;
        Vector3 spot = placing->center;
        i32 found = 0;
        for (i32 attempt = 0; attempt < PLACING_TRIES && !found; attempt++) {
            Vector3 offset; // a random spot inside the ball: inside the cube, kept when inside the ball
            do offset = vector3_create(rand_f32(-1, 1), rand_f32(-1, 1), rand_f32(-1, 1));
            while (vector3_square_magnitude(offset) > 1);
            spot = vector3_added(placing->center, vector3_multiplied(offset, half));
            found = placing_spot_free(engine, spot, size / 2);
        }
        if (!found) continue;
        ObjectCopy *copy = engine_add_copy(engine, placing->object);
        if (!copy) return;
        copy->placing = index;
        copy->size = size;
        copy->particle.position = copy->previous_position = spot;
        // small faster: the top of its speed range comes down toward min the bigger it is
        real top = placing->speed_max + (placing->speed_min - placing->speed_max) * placing->small_faster * bigness;
        copy->particle.velocity = vector3_multiplied(rand_unit_vector3(), rand_f32(placing->speed_min, top));
        particle_set_mass(&copy->particle, 1); // engine_step sets the real one
    }
}
