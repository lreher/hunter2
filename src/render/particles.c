// Particles: look only, they live on the GPU (focus/game.md, thruster particles). Gameplay never reads them.
// The CPU only sends orders: each render_emit call adds an emitter for this frame (where, which way, how many).
// render_end then runs, on the GPU:
//   1. move pass: every live particle ages and moves (one thread each),
//   2. spawn pass: this frame's new particles are written into the ring (one thread each),
//   3. draw: one instanced draw reads the same buffer.
// So the CPU cost does not grow with the count of particles: a few small uploads and calls per frame.
// Local emitters get an anchor: their space (position and rotation), sent every frame. Their particles keep
// position and velocity in that space and are turned into the world only when drawn, so they go with the emitter.
//
// The buffer is a ring of slots: new particles go in at the head, which moves on and wraps around. No free list:
// a slot is reused when the head comes round again, so the ring must be bigger than rate * lifetime, or young
// particles overwrite old ones that are still alive (they vanish early).

#define PARTICLE_MAX_SLOTS (1 << 20)  // the buffer, 48 MB. the ring in use (render_particle_tuning.capacity) is up to this
#define PARTICLE_MAX_EMITTERS 16      // render_emit calls per frame
#define PARTICLE_MAX_ANCHORS 64       // local emitters in all, anchors are never given back
#define PARTICLE_MAX_TYPES 16         // emitter types. a spark keeps its type next to its anchor (anchor_and_type in shaders.c)
#define PARTICLE_WORKGROUP 64         // threads per workgroup, matches @workgroup_size in the shaders

// One emitter this frame, as the shaders read it (Emitter in shaders.c, 96 bytes)
typedef struct {
    f32 position[4];  // w = cone
    f32 previous[4];  // w = size
    f32 direction[4]; // w = speed
    f32 velocity[4];  // w = speed spread
    f32 lifetime, lifetime_spread;
    u32 anchor, type; // anchor: 1 + its index, 0 = world. type: 0..PARTICLE_MAX_TYPES - 1
    u32 first, count, color_start, color_end;
} ParticleEmitterGpu;

// A local emitter's space this frame (Anchor in shaders.c)
typedef struct {
    f32 position[4];
    f32 rotation[4]; // quaternion x, y, z, w
} ParticleAnchorGpu;

// One emitter type, as the shaders read it (EmitterType in shaders.c): x = share of velocity kept this frame (drag),
// y = stretch, seconds of movement a streak shows
typedef struct {
    f32 keep, stretch, pad0, pad1;
} ParticleTypeGpu;

// The uniform for all three shaders (Frame in shaders.c)
typedef struct {
    f32 view_proj[16];
    f32 eye[4];          // w = frame seconds
    f32 camera_right[4];
    f32 camera_up[4];
    u32 capacity, head, spawn_total, emitter_count;
    u32 seed, slot_count, pad0, pad1;
    ParticleTypeGpu types[PARTICLE_MAX_TYPES];
    ParticleAnchorGpu anchors[PARTICLE_MAX_ANCHORS];
    ParticleEmitterGpu emitters[PARTICLE_MAX_EMITTERS];
} ParticleFrame;

RenderParticleTuning render_particle_tuning = {
    .capacity = 262144,
};
static f32 particle_type_drag[PARTICLE_MAX_TYPES]; // per second, the last one each type was emitted with

static ParticleFrame particle_frame;
static GpuHandle particle_frame_buffer;
static GpuHandle particle_buffer;
static GpuHandle particle_move_pipeline, particle_move_bind_group;
static GpuHandle particle_spawn_pipeline, particle_spawn_bind_group;
static GpuHandle particle_draw_pipeline, particle_draw_bind_group;
static f32 particle_seconds;       // this frame's time, from render_begin
static u32 particle_head;          // next slot to write, 0..capacity-1
static u32 particle_frame_count;   // seeds the random numbers
static f32 particle_awake_seconds; // until every particle is surely dead. 0 = skip the passes and the draw
static f32 particle_estimate;      // particles alive, about: sum of rate * lifetime over this frame's emitters
static i32 particle_anchor_count;  // anchors handed out

static GpuHandle particle_create_bind_group(GpuHandle pipeline) {
    GpuBindGroupDesc bind_group = {0};
    bind_group.pipeline = pipeline;
    bind_group.entry_count = 2;
    bind_group.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, particle_frame_buffer};
    bind_group.entries[1] = (GpuBindEntry){1, GPU_BIND_STORAGE, particle_buffer};
    return plat_gpu_create_bind_group(&bind_group);
}

static void render_particles_init(void) {
    particle_frame_buffer = plat_gpu_create_buffer(sizeof(ParticleFrame), GPU_BUFFER_UNIFORM | GPU_BUFFER_COPY_DST);
    particle_buffer = plat_gpu_create_buffer(PARTICLE_MAX_SLOTS * 48, GPU_BUFFER_STORAGE); // starts zeroed: all dead

    particle_move_pipeline = plat_gpu_create_compute_pipeline(plat_gpu_create_shader(render_shader_particle_move));
    particle_move_bind_group = particle_create_bind_group(particle_move_pipeline);
    particle_spawn_pipeline = plat_gpu_create_compute_pipeline(plat_gpu_create_shader(render_shader_particle_spawn));
    particle_spawn_bind_group = particle_create_bind_group(particle_spawn_pipeline);

    GpuPipelineDesc pipeline = {0};
    pipeline.shader = plat_gpu_create_shader(render_shader_particle_draw);
    pipeline.buffer_count = 0; // the shader reads the particles by instance_index
    pipeline.target_format = GPU_FORMAT_SCREEN;
    pipeline.blend = GPU_BLEND_ADDITIVE;
    pipeline.topology = GPU_TOPOLOGY_TRIANGLE_STRIP;
    pipeline.depth = GPU_DEPTH_TEST; // hidden behind solid things, never hides another particle
    pipeline.cull = GPU_CULL_NONE;
    particle_draw_pipeline = plat_gpu_create_pipeline(&pipeline);
    particle_draw_bind_group = particle_create_bind_group(particle_draw_pipeline);
}

static void render_particles_begin(f32 seconds) {
    particle_seconds = seconds;
    particle_frame.spawn_total = 0;
    particle_frame.emitter_count = 0;
    particle_estimate = 0;
}

// camera right and up for squares that face it: the first two rows of the view projection, made unit length
// (the projection only scales them, see mesh_lod_scale in mesh.c)
static void render_particles_set_camera(Matrix4 view_projection, Vector3 eye) {
    for (i32 i = 0; i < 16; i++) particle_frame.view_proj[i] = view_projection.m[i];
    f32 *m = view_projection.m;
    Vector3 right = vector3_create(m[0], m[4], m[8]);
    Vector3 up = vector3_create(m[1], m[5], m[9]);
    vector3_normalize(&right);
    vector3_normalize(&up);
    particle_frame.eye[0] = eye.x;
    particle_frame.eye[1] = eye.y;
    particle_frame.eye[2] = eye.z;
    particle_frame.camera_right[0] = right.x;
    particle_frame.camera_right[1] = right.y;
    particle_frame.camera_right[2] = right.z;
    particle_frame.camera_up[0] = up.x;
    particle_frame.camera_up[1] = up.y;
    particle_frame.camera_up[2] = up.z;
}

static u32 particle_pack_color(Vector3 color, f32 alpha) {
    f32 channels[4] = {color.x, color.y, color.z, alpha};
    u32 packed = 0;
    for (i32 i = 0; i < 4; i++) {
        f32 value = channels[i] < 0 ? 0 : channels[i] > 1 ? 1 : channels[i];
        packed |= (u32)(value * 255 + 0.5f) << (8 * i); // unpack4x8unorm: first channel in the low byte
    }
    return packed;
}

static u32 particle_capacity(void) {
    f32 capacity = render_particle_tuning.capacity;
    if (capacity < 1) capacity = 1;
    if (capacity > PARTICLE_MAX_SLOTS) capacity = PARTICLE_MAX_SLOTS;
    return (u32)capacity;
}

void render_emit(RenderEmitter *emitter) {
    RenderEmitterMemory *memory = &emitter->memory;
    if (!memory->started) {
        memory->previous_position = emitter->position;
        memory->carry = 0;
        memory->started = 1;
    }
    u32 type = emitter->type < 0 ? 0 : emitter->type >= PARTICLE_MAX_TYPES ? PARTICLE_MAX_TYPES - 1 : (u32)emitter->type;
    particle_type_drag[type] = emitter->drag; // turned into the share kept this frame in render_particles_compute
    particle_frame.types[type].stretch = emitter->stretch;
    u32 anchor = 0;
    if (emitter->local) {
        if (memory->anchor == 0 && particle_anchor_count < PARTICLE_MAX_ANCHORS) memory->anchor = ++particle_anchor_count;
        anchor = (u32)memory->anchor;
    }
    if (anchor > 0) { // every frame, even with nothing new: its older particles follow it
        Quaternion rotation = emitter->rotation;
        particle_frame.anchors[anchor - 1] = (ParticleAnchorGpu){
            .position = {emitter->position.x, emitter->position.y, emitter->position.z, 0},
            .rotation = {rotation.x, rotation.y, rotation.z, rotation.w},
        };
    }
    f32 wanted = emitter->rate * particle_seconds + memory->carry;
    u32 count = wanted > 0 ? (u32)wanted : 0;
    memory->carry = wanted - (f32)count; // the part of a particle left over, so low rates still come out right
    u32 capacity = particle_capacity();
    if (count > capacity - particle_frame.spawn_total) count = capacity - particle_frame.spawn_total;
    particle_estimate += emitter->rate * emitter->lifetime;
    if (count > 0 && particle_frame.emitter_count < PARTICLE_MAX_EMITTERS) {
        ParticleEmitterGpu *gpu = &particle_frame.emitters[particle_frame.emitter_count++];
        *gpu = (ParticleEmitterGpu){
            .position = {emitter->position.x, emitter->position.y, emitter->position.z, emitter->cone},
            .previous = {memory->previous_position.x, memory->previous_position.y, memory->previous_position.z, emitter->size},
            .direction = {emitter->direction.x, emitter->direction.y, emitter->direction.z, emitter->speed},
            .velocity = {emitter->velocity.x, emitter->velocity.y, emitter->velocity.z, emitter->speed_spread},
            .lifetime = emitter->lifetime,
            .lifetime_spread = emitter->lifetime_spread,
            .anchor = anchor,
            .type = type,
            .first = particle_frame.spawn_total,
            .count = count,
            .color_start = particle_pack_color(emitter->color_start, 1),
            .color_end = particle_pack_color(emitter->color_end, 1),
        };
        particle_frame.spawn_total += count;
        f32 longest = emitter->lifetime + emitter->lifetime_spread;
        if (longest > particle_awake_seconds) particle_awake_seconds = longest;
    }
    memory->previous_position = emitter->position;
}

i32 render_particle_estimate(void) {
    u32 capacity = particle_capacity();
    return particle_estimate > (f32)capacity ? (i32)capacity : (i32)particle_estimate;
}

// before the shadow pass: move, then spawn. passes run on the GPU in order, so the draw sees both
static void render_particles_compute(void) {
    if (particle_awake_seconds <= 0) return;
    u32 capacity = particle_capacity();
    if (particle_head >= capacity) particle_head = 0; // the ring shrank
    for (i32 i = 0; i < PARTICLE_MAX_TYPES; i++) {
        f32 keep = 1 - particle_type_drag[i] * particle_seconds;
        particle_frame.types[i].keep = keep < 0 ? 0 : keep;
    }
    particle_frame.eye[3] = particle_seconds;
    particle_frame.capacity = capacity;
    particle_frame.head = particle_head;
    particle_frame.seed = particle_frame_count++ * 0x9E3779B9u;
    particle_frame.slot_count = PARTICLE_MAX_SLOTS;
    i32 used = (i32)__builtin_offsetof(ParticleFrame, emitters) + // the header and every anchor, then the emitters in use
               (i32)(particle_frame.emitter_count * sizeof(ParticleEmitterGpu));
    plat_gpu_write_buffer(particle_frame_buffer, 0, &particle_frame, used);

    plat_gpu_begin_compute_pass();
    plat_gpu_set_pipeline(particle_move_pipeline);
    plat_gpu_set_bind_group(0, particle_move_bind_group);
    plat_gpu_dispatch((PARTICLE_MAX_SLOTS + PARTICLE_WORKGROUP - 1) / PARTICLE_WORKGROUP, 1, 1);
    if (particle_frame.spawn_total > 0) {
        plat_gpu_set_pipeline(particle_spawn_pipeline);
        plat_gpu_set_bind_group(0, particle_spawn_bind_group);
        plat_gpu_dispatch((particle_frame.spawn_total + PARTICLE_WORKGROUP - 1) / PARTICLE_WORKGROUP, 1, 1);
        particle_head = (particle_head + particle_frame.spawn_total) % capacity;
    }
}

// in the screen pass, after the solid things, so they can hide particles
static void render_particles_draw(void) {
    if (particle_awake_seconds <= 0) return;
    plat_gpu_set_pipeline(particle_draw_pipeline);
    plat_gpu_set_bind_group(0, particle_draw_bind_group);
    plat_gpu_draw(4, (i32)particle_capacity());
    particle_awake_seconds -= particle_seconds;
}
