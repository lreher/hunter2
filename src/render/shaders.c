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
    "\n"
    RENDER_SHADER_INSTANCE
    "\n"
    "struct VOut {\n"
    "    @builtin(position) pos: vec4f,\n"
    "    @location(0) normal: vec3f,\n"
    "    @location(1) color: vec4f,\n"
    "    @location(2) world: vec3f,\n"
    "};\n"
    "\n"
    "@vertex fn vs(i: In) -> VOut {\n"
    "    let world = world_position(i);\n"
    "    var o: VOut;\n"
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
    "    let albedo = pow(i.color.rgb, vec3f(2.2));\n"
    "    let diffuse = max(dot(n, l), 0.0) * sun;\n"
    "    let h = normalize(l + normalize(scene.eye.xyz - p));\n"
    "    let specular = pow(max(dot(n, h), 0.0), 32.0) * 0.12 * sun * step(0.0, dot(n, l));\n"
    "    let sky = mix(vec3f(0.06, 0.05, 0.05), vec3f(0.20, 0.23, 0.30), 0.5 + 0.5 * n.y);\n" // more from above than below
    "    let lit = albedo * (vec3f(1.0, 0.96, 0.9) * diffuse + sky * ambient) + vec3f(specular);\n"
    "    return vec4f(pow(lit, vec3f(1.0 / 2.2)), i.color.a);\n"
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
