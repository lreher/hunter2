// WGSL sources. Entry points must be named "vs" and "fs".

// One instance = one textured, tinted rectangle in screen pixels.
// Instance layout must match Quad in render.c.
static const char render_shader_quad[] =
    "struct Screen { size: vec2f };\n"
    "@group(0) @binding(0) var<uniform> screen: Screen;\n"
    "@group(0) @binding(1) var tex: texture_2d<f32>;\n"
    "@group(0) @binding(2) var samp: sampler;\n"
    "\n"
    "struct Inst {\n"
    "    @location(0) rect: vec4f,\n"
    "    @location(1) uvrect: vec4f,\n"
    "    @location(2) color: vec4f,\n"
    "    @location(3) params: vec4f,\n" // x = shape: 0 texture, 1 circle, 2 solid
    "};\n"
    "struct VOut {\n"
    "    @builtin(position) pos: vec4f,\n"
    "    @location(0) uv: vec2f,\n"
    "    @location(1) color: vec4f,\n"
    "    @location(2) local: vec2f,\n"
    "    @location(3) @interpolate(flat) shape: f32,\n"
    "};\n"
    "\n"
    "@vertex fn vs(@builtin(vertex_index) vi: u32, inst: Inst) -> VOut {\n"
    "    let corner = vec2f(f32(vi & 1u), f32((vi >> 1u) & 1u));\n"
    "    let px = inst.rect.xy + corner * inst.rect.zw;\n"
    "    var o: VOut;\n"
    "    o.pos = vec4f(px.x / screen.size.x * 2.0 - 1.0, 1.0 - px.y / screen.size.y * 2.0, 0.0, 1.0);\n"
    "    o.uv = inst.uvrect.xy + corner * inst.uvrect.zw;\n"
    "    o.color = inst.color;\n"
    "    o.local = corner;\n"
    "    o.shape = inst.params.x;\n"
    "    return o;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    let tex_coverage = textureSample(tex, samp, i.uv).r;\n"
    "    let d = length(i.local * 2.0 - 1.0);\n"
    "    let edge = max(fwidth(d), 0.0001);\n"
    "    let circle_coverage = 1.0 - smoothstep(1.0 - edge, 1.0, d);\n"
    "    var coverage = tex_coverage;\n"
    "    if (i.shape > 1.5) {\n"
    "        coverage = 1.0;\n"
    "    } else if (i.shape > 0.5) {\n"
    "        coverage = circle_coverage;\n"
    "    }\n"
    "    return vec4f(i.color.rgb, i.color.a * coverage);\n"
    "}\n";

// What the mesh shaders know about the frame. Layout must match MeshScene in mesh.c.
#define RENDER_SHADER_SCENE \
    "struct Scene {\n" \
    "    view_proj: mat4x4f,\n" \
    "    light: vec4f,\n"           /* direction toward the sun */ \
    "    eye: vec4f,\n"             /* camera position */ \
    "    light_view_proj: mat4x4f,\n" /* world -> shadow map: x, y in -1..1, depth 0..1 */ \
    "    shadow: vec4f,\n"          /* x texel size (uv), y depth bias, z normal offset (m) */ \
    "    grid: vec4f,\n"            /* contact grid: min x, min z, cells per meter, reach in radii */ \
    "    grid_size: vec4u,\n"       /* columns (x), rows (z) */ \
    "};\n" \
    "@group(0) @binding(0) var<uniform> scene: Scene;\n"

// One instance = one sphere or box: unit mesh scaled by half size, rotated, moved to center.
// Instance layout must match MeshInstance in mesh.c.
#define RENDER_SHADER_INSTANCE \
    "struct In {\n" \
    "    @location(0) pos: vec3f,\n" \
    "    @location(1) normal: vec3f,\n" \
    "    @location(2) center: vec4f,\n" \
    "    @location(3) half_size: vec4f,\n" \
    "    @location(4) rotation: vec4f,\n" /* quaternion x y z w */ \
    "    @location(5) color: vec4f,\n" \
    "};\n" \
    "\n" \
    "fn rotate(q: vec4f, v: vec3f) -> vec3f {\n" \
    "    let t = 2.0 * cross(q.xyz, v);\n" \
    "    return v + q.w * t + cross(q.xyz, t);\n" \
    "}\n" \
    "\n" \
    "fn world_position(i: In) -> vec3f {\n" \
    "    return i.center.xyz + rotate(i.rotation, i.pos * i.half_size.xyz);\n" \
    "}\n"

// Lighting is done in linear color: colors come in as sRGB, go linear, get lit, go back to sRGB.
// Sun shadow: the shadow map holds the depth of the first thing the sun hits, per texel (drawn by
// render_shader_shadow). A pixel deeper than that is in shadow. 9 filtered taps soften the edge.
// Contact shadow: spheres close to the pixel each block part of the sky. The contact grid lists,
// per cell of the floor plane, the spheres close enough to matter, so a pixel only checks those.
static const char render_shader_mesh[] =
    RENDER_SHADER_SCENE
    "struct Sphere { center: vec4f, half_size: vec4f, rotation: vec4f, color: vec4f };\n" // a MeshInstance
    "@group(0) @binding(1) var<storage, read> spheres: array<Sphere>;\n"
    "@group(0) @binding(2) var<storage, read> grid: array<u32>;\n" // cell c lists grid[grid[c]] .. grid[grid[c + 1] - 1]
    "@group(0) @binding(3) var shadow_map: texture_depth_2d;\n"
    "@group(0) @binding(4) var shadow_sampler: sampler_comparison;\n"
    "@group(0) @binding(5) var photo: texture_2d<f32>;\n"
    "@group(0) @binding(6) var photo_sampler: sampler;\n"
    "\n"
    RENDER_SHADER_INSTANCE
    "\n"
    "struct VOut {\n"
    "    @builtin(position) pos: vec4f,\n"
    "    @location(0) normal: vec3f,\n"
    "    @location(1) color: vec4f,\n"
    "    @location(2) world: vec3f,\n"
    "    @location(3) center: vec4f,\n" // w = 1: show the photo
    "    @location(4) glow: f32,\n"       // 1 = full color, no shading
    "};\n"
    "\n"
    "@vertex fn vs(i: In) -> VOut {\n"
    "    let world = world_position(i);\n"
    "    var o: VOut;\n"
    "    o.center = i.center;\n"
    "    o.glow = i.half_size.w;\n"
    "    o.pos = scene.view_proj * vec4f(world, 1.0);\n"
    "    o.normal = rotate(i.rotation, i.normal / i.half_size.xyz);\n" // stretched shapes bend their normals the other way
    "    o.color = i.color;\n"
    "    o.world = world;\n"
    "    return o;\n"
    "}\n"
    "\n"
    // 1 = the sun reaches p. p is nudged off the surface first, so a surface does not shadow itself
    "fn sun_visible(p: vec3f, n: vec3f) -> f32 {\n"
    "    let s = scene.light_view_proj * vec4f(p + n * scene.shadow.z, 1.0);\n"
    "    let uv = vec2f(0.5 + 0.5 * s.x, 0.5 - 0.5 * s.y);\n"
    "    let depth = min(s.z - scene.shadow.y, 1.0);\n"
    "    var sum = 0.0;\n"
    "    for (var y = -1; y <= 1; y++) {\n"
    "        for (var x = -1; x <= 1; x++) {\n"
    "            let tap = uv + vec2f(f32(x), f32(y)) * scene.shadow.x;\n"
    "            sum += textureSampleCompareLevel(shadow_map, shadow_sampler, tap, depth);\n"
    "        }\n"
    "    }\n"
    "    return sum / 9.0;\n"
    "}\n"
    "\n"
    // how much of the sky nearby spheres leave open, 1 = all of it
    "fn sky_open(p: vec3f, n: vec3f) -> f32 {\n"
    "    let g = (p.xz - scene.grid.xy) * scene.grid.z;\n"
    "    let size = scene.grid_size.xy;\n"
    "    if (g.x < 0.0 || g.y < 0.0 || g.x >= f32(size.x) || g.y >= f32(size.y)) {\n"
    "        return 1.0;\n" // outside the grid: no sphere is close enough
    "    }\n"
    "    let cell = u32(g.y) * size.x + u32(g.x);\n"
    "    let reach = scene.grid.w;\n"
    "    var open = 1.0;\n"
    "    for (var k = grid[cell]; k < grid[cell + 1u]; k++) {\n"
    "        let sphere = spheres[grid[k]];\n"
    "        let v = sphere.center.xyz - p;\n"
    "        let d2 = dot(v, v);\n"
    "        let r2 = sphere.half_size.x * sphere.half_size.x;\n"
    "        let reach2 = reach * reach * r2;\n"
    "        if (d2 < reach2) {\n"
    "            let facing = dot(n, v) * inverseSqrt(d2);\n" // our own sphere faces away (< 0), so it does not count
    "            let fade = 1.0 - smoothstep(0.45 * reach2, reach2, d2);\n" // fades out before the reach, no visible edge
    "            open *= 1.0 - clamp(facing * r2 / d2, 0.0, 1.0) * fade;\n"
    "        }\n"
    "    }\n"
    "    return open;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    let n = normalize(i.normal);\n"
    "    let l = scene.light.xyz;\n"
    "    let p = i.world;\n"
    "    let sun = sun_visible(p, n);\n"
    "    let ambient = sky_open(p, n);\n"
    "\n"
    "    var color = i.color.rgb;\n"
    // photo facing the camera: look at the ball from the camera, the photo's center is the ball's center.
    // the visible half maps to a disc on the photo (its inscribed circle). shading still uses the real normal
    "    if (i.center.w > 0.5) {\n"
    "        let toward = normalize(scene.eye.xyz - i.center.xyz);\n"
    "        let side = normalize(cross(vec3f(0.0, 1.0, 0.0), toward));\n" // breaks only looking straight down
    "        let up = cross(toward, side);\n"
    "        let d = normalize(p - i.center.xyz);\n"
    "        let uv = vec2f(0.5 + 0.5 * dot(d, side), 0.5 - 0.5 * dot(d, up));\n"
    "        let texel = textureSampleLevel(photo, photo_sampler, uv, 0.0);\n"
    "        color = mix(color, texel.rgb, texel.a * step(0.0, dot(d, toward)));\n" // empty texture (alpha 0): plain color
    "    }\n"
    "    let albedo = pow(color, vec3f(2.2));\n"
    "    let diffuse = max(dot(n, l), 0.0) * sun;\n"
    "    let h = normalize(l + normalize(scene.eye.xyz - p));\n"
    "    let specular = pow(max(dot(n, h), 0.0), 32.0) * 0.12 * sun * step(0.0, dot(n, l));\n"
    "    let sky = mix(vec3f(0.06, 0.05, 0.05), vec3f(0.20, 0.23, 0.30), 0.5 + 0.5 * n.y);\n" // more from above than below
    "    let lit = albedo * (vec3f(1.0, 0.96, 0.9) * diffuse + sky * ambient) + vec3f(specular);\n"
    "    return vec4f(mix(pow(lit, vec3f(1.0 / 2.2)), color, clamp(i.glow, 0.0, 1.0)), i.color.a);\n"
    "}\n";

// Shadow pass: the same instances seen from the sun, depth only (no fragment shader).
static const char render_shader_shadow[] =
    RENDER_SHADER_SCENE
    RENDER_SHADER_INSTANCE
    "\n"
    "@vertex fn vs(i: In) -> @builtin(position) vec4f {\n"
    "    return scene.light_view_proj * vec4f(world_position(i), 1.0);\n"
    "}\n";

// Lines in world space, one color per vertex. Layout must match LineVertex in mesh.c.
// Glows: a soft light around a point (a glowing bullet's halo). A square facing the camera, brightest in the middle,
// fading to nothing at its radius. Drawn added up (additive), depth tested but not written, like the sparks.
static const char render_shader_glow[] =
    "struct Scene { view_proj: mat4x4f, light: vec4f, eye: vec4f };\n" // only the start of the mesh Scene
    "@group(0) @binding(0) var<uniform> scene: Scene;\n"
    "\n"
    "struct VOut {\n"
    "    @builtin(position) position: vec4f,\n"
    "    @location(0) corner: vec2f,\n"
    "    @location(1) color: vec4f,\n"
    "};\n"
    "\n"
    "@vertex fn vs(@builtin(vertex_index) corner_index: u32, @location(0) center: vec4f, @location(1) color: vec4f) -> VOut {\n"
    "    let corner = vec2f(f32(corner_index & 1u), f32(corner_index >> 1u)) * 2.0 - 1.0;\n"
    "    let toward = normalize(scene.eye.xyz - center.xyz);\n"
    "    var side = cross(vec3f(0.0, 1.0, 0.0), toward);\n"
    "    if (dot(side, side) < 0.0001) { side = vec3f(1.0, 0.0, 0.0); }\n" // looking straight down at it
    "    side = normalize(side);\n"
    "    let up = cross(toward, side);\n"
    "    let world = center.xyz + (side * corner.x + up * corner.y) * center.w;\n"
    "    var o: VOut;\n"
    "    o.position = scene.view_proj * vec4f(world, 1.0);\n"
    "    o.corner = corner;\n"
    "    o.color = color;\n"
    "    return o;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    let fade = max(0.0, 1.0 - dot(i.corner, i.corner));\n"
    "    return vec4f(i.color.rgb, i.color.a * fade * fade);\n"
    "}\n";

static const char render_shader_line[] =
    "struct Scene { view_proj: mat4x4f };\n" // only the start of the mesh Scene
    "@group(0) @binding(0) var<uniform> scene: Scene;\n"
    "\n"
    "struct VOut {\n"
    "    @builtin(position) pos: vec4f,\n"
    "    @location(0) color: vec3f,\n"
    "};\n"
    "\n"
    "@vertex fn vs(@location(0) pos: vec3f, @location(1) color: vec3f) -> VOut {\n"
    "    var o: VOut;\n"
    "    o.pos = scene.view_proj * vec4f(pos, 1.0);\n"
    "    o.pos.z -= 0.000002 * o.pos.w;\n" // nudge toward the camera, so edges lying on a surface do not flicker against it
    "    o.color = color;\n"
    "    return o;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    return vec4f(i.color, 1.0);\n"
    "}\n";

// Particles (render/particles.c): look only, they live on the GPU. One storage buffer of slots, used as a ring.
// Each frame: the move pass ages and moves every live slot, then the spawn pass writes this frame's new ones,
// then the draw reads the same buffer. Layouts must match ParticleFrame, ParticleAnchorGpu and ParticleEmitterGpu in
// particles.c. A particle with an anchor keeps its position and velocity in that anchor's space (the anchor is moved
// and turned every frame, so the particle goes with it), anchor 0 means world space.
#define RENDER_SHADER_PARTICLE_COMMON \
    "struct Particle {\n" \
    "    position: vec3f, age: f32,\n"         /* age and lifetime in seconds. age >= lifetime: dead (0, 0 too) */ \
    "    velocity: vec3f, lifetime: f32,\n" \
    "    size: f32, color_start: u32, color_end: u32, anchor_and_type: u32,\n" /* colors packed 8 bits each, rgba. anchor in the low 8 bits, emitter type above */ \
    "};\n" \
    "struct Anchor {\n" \
    "    position: vec4f,\n" /* world */ \
    "    rotation: vec4f,\n" /* quaternion x, y, z, w */ \
    "};\n" \
    "fn rotate(q: vec4f, v: vec3f) -> vec3f {\n" /* v turned by the unit quaternion q */ \
    "    let t = 2.0 * cross(q.xyz, v);\n" \
    "    return v + q.w * t + cross(q.xyz, t);\n" \
    "}\n" \
    "struct Emitter {\n" \
    "    position: vec4f,\n"  /* w = cone, radians */ \
    "    previous: vec4f,\n"  /* where it was last frame. w = size */ \
    "    direction: vec4f,\n" /* w = speed */ \
    "    velocity: vec4f,\n"  /* w = speed spread */ \
    "    lifetime: f32, lifetime_spread: f32, anchor: u32, emitter_type: u32,\n" /* anchor: 1 + its index, 0 = world */ \
    "    first: u32, count: u32, color_start: u32, color_end: u32,\n" /* first: its first new particle this frame */ \
    "};\n" \
    "struct EmitterType {\n" \
    "    keep: f32, stretch: f32, pad0: f32, pad1: f32,\n" /* keep: share of velocity kept this frame (drag). stretch: seconds of movement a streak shows */ \
    "};\n" \
    "struct Frame {\n" \
    "    view_proj: mat4x4f,\n" \
    "    eye: vec4f,\n"          /* w = frame seconds */ \
    "    camera_right: vec4f,\n" \
    "    camera_up: vec4f,\n" \
    "    capacity: u32, head: u32, spawn_total: u32, emitter_count: u32,\n" \
    "    seed: u32, slot_count: u32, pad0: u32, pad1: u32,\n" \
    "    emitter_types: array<EmitterType, 16>,\n" \
    "    anchors: array<Anchor, 64>,\n" \
    "    emitters: array<Emitter, 16>,\n" \
    "};\n" \
    "@group(0) @binding(0) var<uniform> frame: Frame;\n"

// Ages and moves every slot in the buffer (not just the ring in use, so slots left out when the ring shrinks still die)
static const char render_shader_particle_move[] =
    RENDER_SHADER_PARTICLE_COMMON
    "@group(0) @binding(1) var<storage, read_write> particles: array<Particle>;\n"
    "\n"
    "@compute @workgroup_size(64) fn cs(@builtin(global_invocation_id) id: vec3u) {\n"
    "    let i = id.x;\n"
    "    if (i >= frame.slot_count) { return; }\n"
    "    let age = particles[i].age;\n"
    "    if (age >= particles[i].lifetime) { return; }\n" // dead: read 8 bytes, write nothing
    "    let seconds = frame.eye.w;\n"
    "    let emitter_type = frame.emitter_types[particles[i].anchor_and_type >> 8u];\n"
    "    let velocity = particles[i].velocity * emitter_type.keep;\n"
    "    particles[i].velocity = velocity;\n"
    "    particles[i].position = particles[i].position + velocity * seconds;\n"
    "    particles[i].age = age + seconds;\n"
    "}\n";

// One thread per new particle. Spawns are spread along the emitter's path since last frame: the first one leaves
// from where it was then and is a whole frame old, the last one leaves from where it is now, brand new.
static const char render_shader_particle_spawn[] =
    RENDER_SHADER_PARTICLE_COMMON
    "@group(0) @binding(1) var<storage, read_write> particles: array<Particle>;\n"
    "\n"
    "fn hash(v: u32) -> u32 {\n" // PCG hash: any number in, a well mixed number out
    "    let state = v * 747796405u + 2891336453u;\n"
    "    let word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;\n"
    "    return (word >> 22u) ^ word;\n"
    "}\n"
    "fn random(state: ptr<function, u32>) -> f32 {\n" // 0..1
    "    *state = hash(*state);\n"
    "    return f32(*state >> 8u) / 16777216.0;\n"
    "}\n"
    "\n"
    "@compute @workgroup_size(64) fn cs(@builtin(global_invocation_id) id: vec3u) {\n"
    "    let i = id.x;\n"
    "    if (i >= frame.spawn_total) { return; }\n"
    "    var which = 0u;\n" // the emitter this particle belongs to. their first indexes go up
    "    for (var k = 1u; k < frame.emitter_count; k++) { if (i >= frame.emitters[k].first) { which = k; } }\n"
    "    let emitter = frame.emitters[which];\n"
    "    var state = hash(i ^ frame.seed);\n"
    "\n"
    "    // a direction in the cone around the emitter's direction, spread evenly over the cone's cap\n"
    "    let forward = emitter.direction.xyz;\n"
    "    var helper = vec3f(0.0, 1.0, 0.0);\n"
    "    if (abs(forward.y) > 0.99) { helper = vec3f(1.0, 0.0, 0.0); }\n"
    "    let side = normalize(cross(helper, forward));\n"
    "    let up = cross(forward, side);\n"
    "    let cos_angle = mix(1.0, cos(emitter.position.w), random(&state));\n"
    "    let sin_angle = sqrt(max(0.0, 1.0 - cos_angle * cos_angle));\n"
    "    let around = random(&state) * 6.2831853;\n"
    "    let direction = forward * cos_angle + (side * cos(around) + up * sin(around)) * sin_angle;\n"
    "    let speed = emitter.direction.w + (random(&state) * 2.0 - 1.0) * emitter.velocity.w;\n"
    "    let velocity = emitter.velocity.xyz + direction * speed;\n"
    "\n"
    "    let along = (f32(i - emitter.first) + random(&state)) / f32(emitter.count);\n" // 0 = last frame, 1 = now
    "    let age = (1.0 - along) * frame.eye.w;\n"
    "    let lifetime = max(0.01, emitter.lifetime + (random(&state) * 2.0 - 1.0) * emitter.lifetime_spread);\n"
    "    let slot = (frame.head + i) % frame.capacity;\n"
    "    var particle: Particle;\n"
    "    if (emitter.anchor > 0u) {\n" // local: starts at the anchor (the nozzle), in its space, no velocity on top
    "        let anchor = frame.anchors[emitter.anchor - 1u];\n"
    "        let inverse = vec4f(-anchor.rotation.xyz, anchor.rotation.w);\n"
    "        particle.velocity = rotate(inverse, direction * speed);\n"
    "        particle.position = particle.velocity * age;\n"
    "    } else {\n"
    "        particle.velocity = velocity;\n"
    "        particle.position = mix(emitter.previous.xyz, emitter.position.xyz, along) + velocity * age;\n"
    "    }\n"
    "    particle.anchor_and_type = emitter.anchor | (emitter.emitter_type << 8u);\n"
    "    particle.age = age;\n"
    "    particle.lifetime = lifetime;\n"
    "    particle.size = emitter.previous.w;\n"
    "    particle.color_start = emitter.color_start;\n"
    "    particle.color_end = emitter.color_end;\n"
    "    particles[slot] = particle;\n"
    "}\n";

// One instance per slot in the ring, 4 corners each (a strip). A square facing the camera, stretched into a streak
// along the velocity, with a soft round glow. Dead slots collapse to a point off screen. Drawn added up (additive),
// depth tested but not written, so the order does not matter.
static const char render_shader_particle_draw[] =
    RENDER_SHADER_PARTICLE_COMMON
    "@group(0) @binding(1) var<storage, read> particles: array<Particle>;\n"
    "\n"
    "struct VOut {\n"
    "    @builtin(position) position: vec4f,\n"
    "    @location(0) corner: vec2f,\n"
    "    @location(1) color: vec4f,\n"
    "};\n"
    "\n"
    "@vertex fn vs(@builtin(vertex_index) corner_index: u32, @builtin(instance_index) slot: u32) -> VOut {\n"
    "    var o: VOut;\n"
    "    let particle = particles[slot];\n"
    "    if (particle.age >= particle.lifetime) {\n"
    "        o.position = vec4f(2.0, 2.0, 2.0, 1.0);\n"
    "        return o;\n"
    "    }\n"
    "    var position = particle.position;\n"
    "    var velocity = particle.velocity;\n"
    "    let anchor_index = particle.anchor_and_type & 255u;\n"
    "    if (anchor_index > 0u) {\n" // from the anchor's space into the world, where the anchor is this frame
    "        let anchor = frame.anchors[anchor_index - 1u];\n"
    "        position = anchor.position.xyz + rotate(anchor.rotation, position);\n"
    "        velocity = rotate(anchor.rotation, velocity);\n"
    "    }\n"
    "    let corner = vec2f(f32(corner_index & 1u), f32(corner_index >> 1u)) * 2.0 - 1.0;\n"
    "    var along = frame.camera_right.xyz;\n"
    "    var across = frame.camera_up.xyz;\n"
    "    var half_length = particle.size;\n"
    "    let speed = length(velocity);\n"
    "    let streak = speed * frame.emitter_types[particle.anchor_and_type >> 8u].stretch;\n"
    "    if (streak > particle.size * 0.1) {\n"
    "        let direction = velocity / speed;\n"
    "        let sideways = cross(direction, normalize(frame.eye.xyz - position));\n"
    "        if (dot(sideways, sideways) > 0.0001) {\n" // not flying straight at the camera
    "            along = direction;\n"
    "            across = normalize(sideways);\n"
    "            half_length = particle.size + streak * 0.5;\n"
    "        }\n"
    "    }\n"
    "    let world = position + along * corner.x * half_length + across * corner.y * particle.size;\n"
    "    o.position = frame.view_proj * vec4f(world, 1.0);\n"
    "    o.corner = corner;\n"
    "    let life = particle.age / particle.lifetime;\n"
    "    let color = mix(unpack4x8unorm(particle.color_start), unpack4x8unorm(particle.color_end), life);\n"
    "    o.color = vec4f(color.rgb, color.a * (1.0 - life));\n"
    "    return o;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    let fade = max(0.0, 1.0 - dot(i.corner, i.corner));\n"
    "    return vec4f(i.color.rgb, i.color.a * fade * fade);\n"
    "}\n";
