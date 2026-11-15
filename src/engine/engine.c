#include "engine/engine.h"
#include "platform/platform.h"

// The engine runs a game's parts (focus/engine.md). Per physics step (engine_step):
//   1. every object: inputs worked out (aim, tilt) from where it points now
//   2. every actuator: reads its input, eases toward it
//   3. gravity: every object in the world and every copy pulls on every other (engine_add_gravity)
//   4. every object: its turn actuators turn it, its push actuators push it (along its new axes), drag
//   5. forces into velocities, then hits: contacts, everything moves, bounces (engine_move_with_hits)
//   6. emitters of objects send out new copies (bullets)
// Copies whose life is up are taken away at the start of the step.
// Objects that are copies_only are not in the world themselves: they are not moved or drawn, only their copies are.
// Per drawn frame (engine_draw): objects and copies drawn blended between steps, then every spark emitter fires.

void engine_start(Engine *engine) {
    for (i32 i = 0; i < engine->object_count; i++) object_start(&engine->objects[i]);
    for (i32 i = 0; i < engine->actuator_count; i++) engine->actuators[i].now = 0;
    for (i32 i = 0; i < engine->emitter_count; i++) {
        emitter_reset(&engine->emitters[i]);
        engine->emitters[i].owed = 1;
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) engine->copies[i].used = 0;
    for (i32 i = 0; i < engine->placing_count; i++) placing_place(engine, i);
}

ObjectCopy *engine_add_copy(Engine *engine, i32 object) {
    for (i32 i = 0; i < engine->copy_capacity; i++) {
        ObjectCopy *copy = &engine->copies[i];
        if (copy->used) continue;
        Object *copied = &engine->objects[object];
        *copy = (ObjectCopy){.used = 1, .object = object, .placing = -1, .from = -1, .life_left = copied->lifetime, .size = copied->size};
        copy->rotation = quaternion_identity();
        copy->particle.damping = 1;
        return copy;
    }
    return 0;
}

// density > 0: a ball size across, else the object's mass
static real engine_copy_mass(Object *object, ObjectCopy *copy) {
    real mass = object->density > 0 ? object->density * 0.5236f * copy->size * copy->size * copy->size : object->mass; // pi / 6
    return mass > 0 ? mass : 0.001f;
}

// drag as damping: the share of velocity kept this step
static real engine_kept(real drag, real time_step) {
    real kept = 1 - drag * time_step;
    return kept > 0 ? kept : 0;
}

// Everything that has a body in the world this step: objects in the world and used copies, in id order (objects by
// place, then copies by place + object_count), so the contact list comes out sorted for the warm start
#define ENGINE_MAX_BODIES 4096
#define ENGINE_MAX_CONTACTS 8192   // a planet touches a few others, plus the ones it could reach this step
#define ENGINE_SOLVER_PASSES 4      // as Gravity Cloud
typedef struct {
    Particle *particle;
    real mass, radius;
    i32 id;
    i32 object;  // a world object: its place. -1 = a copy
    i32 from;    // a copy: the object whose emitter sent it out (it never hits that one). -1 = none
    real reach;  // m it can move this step, for the contact margin
} EngineBody;
static EngineBody engine_bodies[ENGINE_MAX_BODIES];
static i32 engine_body_count;
static ParticleContact engine_contact_lists[2][ENGINE_MAX_CONTACTS]; // this step's and last step's, swapped
static i32 engine_contact_counts[2], engine_contacts_now;

static void engine_gather_bodies(Engine *engine) {
    i32 count = 0;
    for (i32 i = 0; i < engine->object_count && count < ENGINE_MAX_BODIES; i++) {
        Object *object = &engine->objects[i];
        if (object->copies_only) continue;
        particle_set_mass(&object->particle, object->mass > 0 ? object->mass : 0.001f);
        engine_bodies[count++] = (EngineBody){&object->particle, object->mass, object->radius, i, i, -1, 0};
    }
    for (i32 i = 0; i < engine->copy_capacity && count < ENGINE_MAX_BODIES; i++) {
        ObjectCopy *copy = &engine->copies[i];
        if (!copy->used) continue;
        real mass = engine_copy_mass(&engine->objects[copy->object], copy);
        particle_set_mass(&copy->particle, mass);
        engine_bodies[count++] = (EngineBody){&copy->particle, mass, copy->size / 2, engine->object_count + i, -1, copy->from, 0};
    }
    engine_body_count = count;
}

// Newton's gravity (book 3.2, eq 3.3, as Gravity Cloud): force = G * mass a * mass b / distance^2, along the line
// between them, on both. G = real G times the universe's gravity. Closer than their two radii (inside each other)
// counts as touching, so the pull never blows up. All pairs: the cost grows with the count squared
#define ENGINE_REAL_G 6.674e-11f // m^3 per kg per s^2
static void engine_add_gravity(Engine *engine) {
    if (engine->gravity <= 0) return;
    real g = ENGINE_REAL_G * engine->gravity;
    for (i32 a = 0; a < engine_body_count; a++) {
        EngineBody *first = &engine_bodies[a];
        for (i32 b = a + 1; b < engine_body_count; b++) {
            EngineBody *second = &engine_bodies[b];
            Vector3 between = vector3_subtracted(second->particle->position, first->particle->position);
            real distance_squared = vector3_square_magnitude(between);
            if (distance_squared <= 0) continue;
            real distance = __builtin_sqrtf(distance_squared);
            real touching = first->radius + second->radius;
            if (distance_squared < touching * touching) distance_squared = touching * touching;
            real pull = g * first->mass * second->mass / distance_squared; // N
            Vector3 force = vector3_multiplied(between, pull / distance);
            particle_add_force(first->particle, force);
            particle_add_force(second->particle, vector3_multiplied(force, -1));
        }
    }
}

// Hits (as Gravity Cloud): every pair of balls that could touch this step becomes a contact, the solver stops them
// closing more than their gap, everything moves, then the pairs that met bounce (the universe's bounce). Equal and
// opposite pushes by mass: a bullet barely moves a planet, the ship bounces off one. A copy never hits the object
// whose emitter sent it (bullets start inside the ship's ball)
static void engine_move_with_hits(Engine *engine, real time_step) {
    for (i32 i = 0; i < engine_body_count; i++)
        engine_bodies[i].reach = vector3_magnitude(engine_bodies[i].particle->velocity) * time_step;
    i32 before = engine_contacts_now;
    engine_contacts_now ^= 1;
    ParticleContact *list = engine_contact_lists[engine_contacts_now];
    i32 count = 0;
    for (i32 a = 0; a < engine_body_count && count < ENGINE_MAX_CONTACTS; a++) {
        EngineBody *first = &engine_bodies[a];
        if (first->radius <= 0) continue;
        for (i32 b = a + 1; b < engine_body_count && count < ENGINE_MAX_CONTACTS; b++) {
            EngineBody *second = &engine_bodies[b];
            if (second->radius <= 0) continue;
            if (first->object >= 0 && second->from == first->object) continue;
            if (particle_contact_find_spheres(&list[count], first->particle, first->radius, second->particle, second->radius,
                                              engine->bounce, time_step, first->reach + second->reach)) {
                list[count].id_a = first->id;
                list[count].id_b = second->id;
                count++;
            }
        }
    }
    engine_contact_counts[engine_contacts_now] = count;
    particle_contacts_warm(list, count, engine_contact_lists[before], engine_contact_counts[before]);
    particle_contacts_solve(list, count, ENGINE_SOLVER_PASSES);
    for (i32 i = 0; i < engine_body_count; i++) particle_move(engine_bodies[i].particle, time_step);
    particle_contacts_bounce(list, count, ENGINE_SOLVER_PASSES);
}

FORCE_INLINE void engine_step(Engine *engine, real time_step) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->copies_only) continue;
        object->previous_position = object->particle.position;
        object->previous_rotation = object->rotation;
        input_work_out(engine, i);
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) { // copies whose life is up go first
        ObjectCopy *copy = &engine->copies[i];
        if (!copy->used) continue;
        Object *object = &engine->objects[copy->object];
        if (object->lifetime > 0) {
            copy->life_left -= time_step;
            if (copy->life_left <= 0) {
                copy->used = 0;
                continue;
            }
        }
        copy->previous_position = copy->particle.position;
        copy->particle.damping = engine_kept(object->drag, time_step);
    }
    for (i32 i = 0; i < engine->actuator_count; i++) actuator_ease(engine, &engine->actuators[i], time_step);
    engine_gather_bodies(engine);
    engine_add_gravity(engine);
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->copies_only) continue;
        Vector3 turn = vector3_zero(); // rad/s in its own axes
        for (i32 j = 0; j < engine->actuator_count; j++) {
            Actuator *actuator = &engine->actuators[j];
            if (actuator->object == i && engine->actuator_types[actuator->type].kind == ACTUATOR_TURN)
                vector3_add_scaled(&turn, actuator->direction, actuator->now);
        }
        quaternion_add_rotation(&object->rotation, object_axis(object, turn), time_step);
        for (i32 j = 0; j < engine->actuator_count; j++) {
            Actuator *actuator = &engine->actuators[j];
            if (actuator->object == i && engine->actuator_types[actuator->type].kind == ACTUATOR_PUSH)
                particle_add_force(&object->particle, vector3_multiplied(object_axis(object, actuator->direction), actuator->now));
        }
        real drag = object->drag;
        if (source_read(engine, object->drag_cut_input, object) != 0) drag *= 1 - object->drag_cut / 100;
        object->particle.damping = engine_kept(drag, time_step);
    }
    for (i32 i = 0; i < engine_body_count; i++) particle_apply_forces(engine_bodies[i].particle, time_step);
    engine_move_with_hits(engine, time_step);
    for (i32 i = 0; i < engine->emitter_count; i++) {
        Emitter *emitter = &engine->emitters[i];
        if (engine->emitter_types[emitter->type].emits == EMITS_OBJECTS) emitter_send_objects(engine, emitter, time_step);
    }
}

void engine_draw(Engine *engine, f32 alpha) {
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->model < 0 || object->copies_only) continue;
        Vector3 scale = vector3_create(object->size, object->size, object->size);
        render_draw_model(object->model, object_drawn_position(object, alpha), object_drawn_rotation(object, alpha), scale,
                          object->color.x, object->color.y, object->color.z, (f32)object->glow);
        if (object->glow)
            render_draw_glow(object_drawn_position(object, alpha), object->halo, object->color.x, object->color.y, object->color.z);
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) {
        ObjectCopy *copy = &engine->copies[i];
        if (!copy->used) continue;
        Object *object = &engine->objects[copy->object];
        if (object->model < 0) continue;
        Vector3 scale = vector3_create(copy->size, copy->size, copy->size);
        Vector3 position = vector3_lerp(copy->previous_position, copy->particle.position, alpha);
        render_draw_model(object->model, position, copy->rotation, scale, object->color.x, object->color.y, object->color.z,
                          (f32)object->glow);
        if (object->glow) render_draw_glow(position, object->halo, object->color.x, object->color.y, object->color.z);
    }
    for (i32 i = 0; i < engine->emitter_count; i++) {
        Emitter *emitter = &engine->emitters[i];
        if (engine->emitter_types[emitter->type].emits == EMITS_OBJECTS) { // sent out in engine_step
            emitter_reset(emitter);
            continue;
        }
        Object *object = 0;
        Vector3 position = vector3_zero(), velocity = vector3_zero();
        Quaternion rotation = quaternion_identity();
        if (emitter->object >= 0 && emitter->object < engine->object_count) {
            object = &engine->objects[emitter->object];
            position = object_drawn_position(object, alpha);
            rotation = object_drawn_rotation(object, alpha);
            velocity = object->particle.velocity;
        }
        real input = source_read(engine, emitter->input, object);
        if (input < 0) input = 0;
        emitter_fire(emitter, &engine->emitter_types[emitter->type], position, rotation, velocity, input);
    }
}
