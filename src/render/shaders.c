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

// One instance = one sphere or box: unit mesh scaled by half size, rotated, moved to center.
// Instance layout must match MeshInstance in mesh.c, Scene must match MeshScene.
// Lighting is done in linear color: colors come in as sRGB, go linear, get lit, go back to sRGB.
// Every sphere drawn this frame is also in scene.spheres, so each pixel can check which spheres
// block the sun (soft shadow) and which sit close to it (contact shadow, dark where things touch).
static const char render_shader_mesh[] =
    "const MAX_SPHERES = 256;\n"
    "struct Scene {\n"
    "    view_proj: mat4x4f,\n"
    "    light: vec4f,\n"         // direction toward the sun
    "    eye: vec4f,\n"           // camera position
    "    sphere_count: vec4u,\n"  // x only
    "    spheres: array<vec4f, MAX_SPHERES>,\n" // center xyz, radius w
    "};\n"
    "@group(0) @binding(0) var<uniform> scene: Scene;\n"
    "\n"
    "struct In {\n"
    "    @location(0) pos: vec3f,\n"
    "    @location(1) normal: vec3f,\n"
    "    @location(2) center: vec4f,\n"
    "    @location(3) half_size: vec4f,\n"
    "    @location(4) rotation: vec4f,\n" // quaternion x y z w
    "    @location(5) color: vec4f,\n"
    "};\n"
    "struct VOut {\n"
    "    @builtin(position) pos: vec4f,\n"
    "    @location(0) normal: vec3f,\n"
    "    @location(1) color: vec4f,\n"
    "    @location(2) world: vec3f,\n"
    "};\n"
    "\n"
    "fn rotate(q: vec4f, v: vec3f) -> vec3f {\n"
    "    let t = 2.0 * cross(q.xyz, v);\n"
    "    return v + q.w * t + cross(q.xyz, t);\n"
    "}\n"
    "\n"
    "@vertex fn vs(i: In) -> VOut {\n"
    "    let world = i.center.xyz + rotate(i.rotation, i.pos * i.half_size.xyz);\n"
    "    var o: VOut;\n"
    "    o.pos = scene.view_proj * vec4f(world, 1.0);\n"
    "    o.normal = rotate(i.rotation, i.normal / i.half_size.xyz);\n" // stretched shapes bend their normals the other way
    "    o.color = i.color;\n"
    "    o.world = world;\n"
    "    return o;\n"
    "}\n"
    "\n"
    "@fragment fn fs(i: VOut) -> @location(0) vec4f {\n"
    "    let n = normalize(i.normal);\n"
    "    let l = scene.light.xyz;\n"
    "    let p = i.world;\n"
    "\n"
    "    var sun = 1.0;\n" // 1 = fully lit
    "    var ambient = 1.0;\n"
    "    for (var k = 0u; k < scene.sphere_count.x; k++) {\n"
    "        let c = scene.spheres[k].xyz;\n"
    "        let r = scene.spheres[k].w;\n"
    "        let v = c - p;\n"
    "        let along = dot(v, l);\n" // how far toward the sun the sphere is. behind us (< 0): no shadow, also skips our own sphere
    "        if (along > 0.0) {\n"
    "            let miss = length(v - l * along);\n" // closest the sun ray gets to the center
    "            let soft = 0.02 + 0.06 * along;\n"   // penumbra grows with distance
    "            sun = min(sun, smoothstep(r - soft, r + soft, miss));\n"
    "        }\n"
    "        let d2 = dot(v, v);\n" // contact shadow: how much of the sky this sphere covers
    "        let facing = dot(n, v) / sqrt(d2);\n"
    "        ambient *= 1.0 - clamp(facing * r * r / d2, 0.0, 1.0);\n"
    "    }\n"
    "\n"
    "    let albedo = pow(i.color.rgb, vec3f(2.2));\n"
    "    let diffuse = max(dot(n, l), 0.0) * sun;\n"
    "    let h = normalize(l + normalize(scene.eye.xyz - p));\n"
    "    let specular = pow(max(dot(n, h), 0.0), 32.0) * 0.12 * sun * step(0.0, dot(n, l));\n"
    "    let sky = mix(vec3f(0.06, 0.05, 0.05), vec3f(0.20, 0.23, 0.30), 0.5 + 0.5 * n.y);\n" // more from above than below
    "    let lit = albedo * (vec3f(1.0, 0.96, 0.9) * diffuse + sky * ambient) + vec3f(specular);\n"
    "    return vec4f(pow(lit, vec3f(1.0 / 2.2)), i.color.a);\n"
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
