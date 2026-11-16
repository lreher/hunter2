#include "engine/engine.h"
#include "platform/platform.h"

// The engine runs a game's parts (focus/engine.md). Per physics step (engine_step):
//   1. every object: inputs worked out (aim, tilt) from where it points now
//   2. every actuator: reads its input, eases toward it
//   3. gravity: every object in the world and every copy pulls on every other (engine_add_gravity)
//   4. every object: its turn actuators turn it, its push actuators push it (along its new axes), drag
//   5. forces into velocities, then hits: contacts, everything moves, bounces (engine_move_with_hits)
//   6. emitters of objects send out new copies (bullets)
// Copies whose life is up are taken away at the start of the step, so are hit copies whose hit_steps are done: those
// leave a burst behind (their object's hit_burst), queued here and fired by engine_draw.
// Objects that are copies_only are not in the world themselves: they are not moved or drawn, only their copies are.
// Per drawn frame (engine_draw): objects and copies drawn blended between steps, then every spark emitter fires,
// then the queued bursts.

// bursts waiting for engine_draw (render takes 16 emitters a frame, the rest wait for the next). full: no new ones
#define ENGINE_MAX_BURSTS 256
typedef struct {
    i32 type;          // emitter type
    Vector3 position;  // world
    Vector3 direction; // world, unit: its cone points this way
    Vector3 velocity;  // world, its sparks start with it
    f32 extra_speed;   // m/s on top of the type's speed (from the hit)
} EngineBurst;
static EngineBurst engine_bursts[ENGINE_MAX_BURSTS];
static i32 engine_burst_count;

void engine_start(Engine *engine) {
    for (i32 i = 0; i < engine->object_count; i++) object_start(&engine->objects[i]);
    for (i32 i = 0; i < engine->actuator_count; i++) engine->actuators[i].now = 0;
    for (i32 i = 0; i < engine->emitter_count; i++) {
        emitter_reset(&engine->emitters[i]);
        engine->emitters[i].owed = 1;
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) engine->copies[i].used = 0;
    engine_burst_count = 0;
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
static Vector3 engine_contact_closing[ENGINE_MAX_CONTACTS]; // this step's: a's velocity - b's before the solver, for hits
static i32 engine_contact_counts[2], engine_contacts_now;


// the velocity of a body by its id (engine_gather_bodies), zero when it is gone
static Vector3 engine_body_velocity(Engine *engine, i32 id) {
    if (id >= 0 && id < engine->object_count) return engine->objects[id].particle.velocity;
    i32 place = id - engine->object_count;
    if (place >= 0 && place < engine->copy_capacity && engine->copies[place].used) return engine->copies[place].particle.velocity;
    return vector3_zero();
}

// a copy whose object bursts on a hit, touching other (pushed by the solver this step): counts down from now on.
// out: unit, from the other's center toward the copy's. coming: the copy's velocity minus the other's before the hit.
// its burst points from the way it bounces off (coming mirrored off the surface) toward straight out, by hit_straight_out
static void engine_mark_hit(Engine *engine, i32 id, i32 other, Vector3 out, Vector3 coming) {
    i32 place = id - engine->object_count;
    if (place < 0 || place >= engine->copy_capacity) return;
    ObjectCopy *copy = &engine->copies[place];
    Object *object = &engine->objects[copy->object];
    if (!copy->used || copy->hit_left > 0 || object->hit_burst <= 0) return;
    i32 steps = (i32)(object->hit_steps + 0.5f);
    copy->hit_left = steps < 1 ? 1 : steps;
    copy->hit_body = other;
    real into = -vector3_scalar_product(coming, out); // toward the other's center
    copy->hit_speed = into > 0 ? into : 0;
    Vector3 mirrored = vector3_added(coming, vector3_multiplied(out, -2 * vector3_scalar_product(coming, out)));
    real length = vector3_magnitude(mirrored);
    mirrored = length > 0.0001f ? vector3_multiplied(mirrored, 1 / length) : out; // standing still: straight out
    Vector3 direction = vector3_lerp(mirrored, out, object->hit_straight_out);
    length = vector3_magnitude(direction);
    copy->hit_direction = length > 0.0001f ? vector3_multiplied(direction, 1 / length) : out;
}

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
    for (i32 i = 0; i < count; i++)
        engine_contact_closing[i] = vector3_subtracted(list[i].a->velocity, list[i].b ? list[i].b->velocity : vector3_zero());
    particle_contacts_solve(list, count, ENGINE_SOLVER_PASSES);
    for (i32 i = 0; i < engine_body_count; i++) particle_move(engine_bodies[i].particle, time_step);
    particle_contacts_bounce(list, count, ENGINE_SOLVER_PASSES);
    for (i32 i = 0; i < count; i++) { // a pair the solver pushed apart touched: hits
        if (list[i].impulse + list[i].fix_impulse <= 0) continue;
        Vector3 out_of_b = vector3_multiplied(list[i].normal, -1); // the normal goes from a toward b
        engine_mark_hit(engine, list[i].id_a, list[i].id_b, out_of_b, engine_contact_closing[i]);
        engine_mark_hit(engine, list[i].id_b, list[i].id_a, list[i].normal, vector3_multiplied(engine_contact_closing[i], -1));
    }
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
        if (copy->hit_left > 0 && --copy->hit_left == 0) { // its hit is done: gone, a burst where it was
            i32 type = object->hit_burst - 1;
            if (type >= 0 && type < engine->emitter_type_count && engine_burst_count < ENGINE_MAX_BURSTS) {
                Vector3 velocity = object->hit_moves_with == 0 ? engine_body_velocity(engine, copy->hit_body) : vector3_zero();
                engine_bursts[engine_burst_count++] = (EngineBurst){type, copy->particle.position, copy->hit_direction, velocity,
                                                                    (f32)(object->hit_speed_share * copy->hit_speed)};
            }
            copy->used = 0;
            continue;
        }
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

// an object or one of its copies at position, size across: its built model (with glow and halo) or the renderer's ball
static void engine_draw_model(Object *object, Vector3 position, Quaternion rotation, real size) {
    if (object->model == OBJECT_MODEL_BALL) {
        render_draw_sphere(position, size / 2, object->color.x, object->color.y, object->color.z);
        return;
    }
    if (object->model < 0) return;
    render_draw_model(object->model, position, rotation, vector3_create(size, size, size), object->color.x, object->color.y,
                      object->color.z, (f32)object->glow);
    if (object->glow) render_draw_glow(position, object->halo, object->color.x, object->color.y, object->color.z);
}

void engine_draw(Engine *engine, f32 alpha) {
    if (engine->stars.count > 0) render_draw_stars(&engine->stars);
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        if (object->copies_only) continue;
        engine_draw_model(object, object_drawn_position(object, alpha), object_drawn_rotation(object, alpha), object->size);
    }
    for (i32 i = 0; i < engine->copy_capacity; i++) {
        ObjectCopy *copy = &engine->copies[i];
        if (!copy->used) continue;
        Object *object = &engine->objects[copy->object];
        Vector3 position = vector3_lerp(copy->previous_position, copy->particle.position, alpha);
        engine_draw_model(object, position, copy->rotation, copy->size);
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
    i32 waiting = 0; // bursts last, in the order they came. the ones that did not fit wait for the next frame
    for (i32 i = 0; i < engine_burst_count; i++) {
        EngineBurst burst = engine_bursts[i];
        if (burst.type >= engine->emitter_type_count) continue;
        if (!emitter_burst(burst.type, &engine->emitter_types[burst.type], burst.position, burst.direction, burst.velocity,
                           burst.extra_speed))
            engine_bursts[waiting++] = burst;
    }
    engine_burst_count = waiting;
}
