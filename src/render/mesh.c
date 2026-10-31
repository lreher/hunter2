#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"

// 3D drawing: instanced meshes (sphere, box) and lines, in world meters, y up.
// Every draw call adds to a batch. render_end uploads each batch once, then draws it in two passes:
// the shadow pass draws the scene from the sun into a depth texture (the shadow map),
// the screen pass draws it from the camera and reads the shadow map to see where the sun reaches.
// Contact shadows (dark where things touch) use a grid on the floor plane that lists the spheres
// near each cell, so a pixel only checks those, not every sphere.

typedef struct {
    f32 x, y, z;
    f32 nx, ny, nz;
} MeshVertex;

// One drawn object. Layout must match the pipeline attributes below and Sphere in shaders.c (64 bytes).
typedef struct {
    f32 x, y, z, photo;   // photo: 1 = show the sphere photo facing the camera (spheres only)
    f32 hx, hy, hz, pad1; // half size per axis (radius for spheres)
    f32 qx, qy, qz, qw;   // rotation quaternion. identity until rigid bodies need it
    f32 r, g, b, a;
} MeshInstance;

typedef struct {
    f32 x, y, z;
    f32 r, g, b;
} LineVertex;

// a unit shape (sphere radius 1, box -1..1), uploaded once
typedef struct {
    MeshVertex *vertices;
    i32 vertex_count;
    GpuHandle vertex_buffer;
} MeshShape;

// Spheres come in levels of detail: icospheres with 1 to 4 subdivisions (80 to 5120 triangles).
// Each sphere gets the cheapest level that still looks round at its size on screen.
// Too many triangles is slow, not just wasteful: the GPU shades in 2x2 pixel blocks, so triangles
// only a pixel or two big get the same pixels shaded several times.
#define MESH_SPHERE_LODS 4
#define MESH_SPHERE_TRIANGLES(level) (20 << (2 * ((level) + 1))) // level 0 = 1 subdivision
#define MESH_SPHERE_ALL_TRIANGLES \
    (MESH_SPHERE_TRIANGLES(0) + MESH_SPHERE_TRIANGLES(1) + MESH_SPHERE_TRIANGLES(2) + MESH_SPHERE_TRIANGLES(3))
// on screen radius (pixels) up to which each level looks round: an edge spans about 63 / 2^subdivisions
// degrees, so the outline dips r * (1 - cos(edge / 2)) inside the circle. that is half a pixel at these sizes
static const f32 mesh_sphere_lod_max_pixels[MESH_SPHERE_LODS - 1] = {13, 52, 208};

#define MESH_MAX_INSTANCES 16384
#define MESH_MAX_LINES 16384

static MeshVertex mesh_sphere_vertices[MESH_SPHERE_ALL_TRIANGLES * 3];
static MeshVertex mesh_box_vertices[36];
static MeshShape mesh_spheres[MESH_SPHERE_LODS];
static MeshShape mesh_box = {mesh_box_vertices, 0, 0};

// spheres in draw order with their level, then grouped by level for the upload: one buffer, one range per level
static MeshInstance mesh_sphere_instances[MESH_MAX_INSTANCES];
static u8 mesh_sphere_levels[MESH_MAX_INSTANCES];
static MeshInstance mesh_sphere_sorted[MESH_MAX_INSTANCES];
static i32 mesh_sphere_count;
static i32 mesh_sphere_level_start[MESH_SPHERE_LODS + 1]; // level k is sorted[start[k] .. start[k + 1] - 1]
static GpuHandle mesh_sphere_buffer; // the sorted spheres. instance data, and read by the shader for contact shadows
static f32 mesh_lod_scale;           // on screen radius in pixels = radius * mesh_lod_scale / distance

static MeshInstance mesh_box_instances[MESH_MAX_INSTANCES];
static i32 mesh_box_count;
static GpuHandle mesh_box_buffer;

static LineVertex mesh_lines[MESH_MAX_LINES * 2];
static i32 mesh_line_count;
static GpuHandle mesh_line_buffer;

// what the mesh shaders know about the frame. layout must match Scene in shaders.c
typedef struct {
    f32 view_proj[16];
    f32 light[4]; // direction toward the sun
    f32 eye[4];
    f32 light_view_proj[16]; // world -> shadow map: x, y in -1..1, depth 0..1
    f32 shadow[4];           // texel size (uv), depth bias, normal offset (m), unused
    f32 grid[4];             // contact grid: min x, min z, cells per meter, reach in radii
    u32 grid_size[4];        // columns (x), rows (z), unused, unused
} MeshScene;

static MeshScene mesh_scene;
static GpuHandle mesh_scene_buffer;
static GpuHandle mesh_pipeline;
static GpuHandle mesh_bind_group;

// One photo for all spheres (render_draw_sphere_photo). Empty until the user picks one (render_add_photo_picker).
#define MESH_PHOTO_SIZE 512
static GpuHandle mesh_photo;
static GpuHandle mesh_line_pipeline;
static GpuHandle mesh_line_bind_group;

// Shadow map: one depth texture over the shadow area, seen straight down the sun's rays.
// The shadow pass draws back faces (front faces culled): the depth stored is then the far side
// of each object, well behind its lit side, so lit surfaces do not shadow themselves.
#define MESH_SHADOW_SIZE 2048   // texels per side
#define MESH_SHADOW_BIAS 0.005f // m, pushes the depth test a little toward the sun
static V3 mesh_shadow_min = {-10, -1, -10}; // area that gets sun shadows, see render_set_shadow_area
static V3 mesh_shadow_max = {10, 10, 10};
static GpuHandle mesh_shadow_map;
static GpuHandle mesh_shadow_pipeline;
static GpuHandle mesh_shadow_bind_group;

// Contact grid over the floor plane (x, z). A sphere darkens what is within MESH_CONTACT_REACH radii
// of its center, so it is listed in every cell that reach touches. One array holds both: grid[c] and
// grid[c + 1] are where cell c's sphere indices start and end, the indices follow the cell table.
#define MESH_CONTACT_REACH 3.0f
#define MESH_GRID_MAX_SIDE 64
#define MESH_GRID_MAX_CELLS (MESH_GRID_MAX_SIDE * MESH_GRID_MAX_SIDE)
#define MESH_GRID_MAX_ENTRIES (1 << 17) // spheres past this get no contact shadow
static u32 mesh_grid[MESH_GRID_MAX_CELLS + 1 + MESH_GRID_MAX_ENTRIES];
static u32 mesh_grid_cursor[MESH_GRID_MAX_CELLS];
static i32 mesh_grid_used; // u32s of mesh_grid filled this frame
static GpuHandle mesh_grid_buffer;

// adds a triangle facing away from the mesh center (counter clockwise seen from outside, so culling keeps it)
static void mesh_push_triangle(MeshShape *shape, V3 a, V3 b, V3 c, V3 outward, i32 smooth) {
    V3 face = v3_vector_product(v3_subtracted(b, a), v3_subtracted(c, a));
    if (v3_scalar_product(face, outward) < 0) {
        V3 t = b;
        b = c;
        c = t;
    }
    V3 corners[3] = {a, b, c};
    for (i32 i = 0; i < 3; i++) {
        V3 n = smooth ? corners[i] : outward; // a unit sphere's normal is its position
        shape->vertices[shape->vertex_count++] = (MeshVertex){corners[i].x, corners[i].y, corners[i].z, n.x, n.y, n.z};
    }
}

static V3 mesh_midpoint_on_sphere(V3 a, V3 b) {
    V3 m = v3_added(a, b);
    v3_normalize(&m);
    return m;
}

// icosahedron, then split every triangle into 4 and push the new corners out to the sphere
static void mesh_build_sphere(MeshShape *shape, i32 subdivisions) {
    const f32 t = 1.618034f; // golden ratio
    V3 corners[12] = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
    };
    static const u8 faces[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
    };
    static V3 triangles[2][MESH_SPHERE_TRIANGLES(MESH_SPHERE_LODS - 1)][3]; // ping pong between two lists
    for (i32 i = 0; i < 12; i++) v3_normalize(&corners[i]);
    for (i32 f = 0; f < 20; f++) {
        for (i32 k = 0; k < 3; k++) triangles[0][f][k] = corners[faces[f][k]];
    }

    i32 count = 20, from = 0;
    for (i32 level = 0; level < subdivisions; level++) {
        i32 to = 1 - from;
        for (i32 i = 0; i < count; i++) {
            V3 a = triangles[from][i][0], b = triangles[from][i][1], c = triangles[from][i][2];
            V3 ab = mesh_midpoint_on_sphere(a, b), bc = mesh_midpoint_on_sphere(b, c), ca = mesh_midpoint_on_sphere(c, a);
            V3 *out = triangles[to][i * 4];
            out[0] = a, out[1] = ab, out[2] = ca;
            out[3] = ab, out[4] = b, out[5] = bc;
            out[6] = ca, out[7] = bc, out[8] = c;
            out[9] = ab, out[10] = bc, out[11] = ca;
        }
        count *= 4;
        from = to;
    }

    for (i32 i = 0; i < count; i++) {
        V3 *tri = triangles[from][i];
        mesh_push_triangle(shape, tri[0], tri[1], tri[2], v3_added(v3_added(tri[0], tri[1]), tri[2]), 1);
    }
}

// cube from -1 to 1, flat normals: 6 faces, 2 triangles each
static void mesh_build_box(void) {
    for (i32 axis = 0; axis < 3; axis++) {
        for (i32 sign = -1; sign <= 1; sign += 2) {
            f32 n[3] = {0, 0, 0}, u[3] = {0, 0, 0}, v[3] = {0, 0, 0};
            n[axis] = (f32)sign;
            u[(axis + 1) % 3] = 1;
            v[(axis + 2) % 3] = 1;
            V3 normal = {n[0], n[1], n[2]};
            V3 corner[4];
            for (i32 k = 0; k < 4; k++) {
                f32 su = (k == 1 || k == 2) ? 1.0f : -1.0f;
                f32 sv = (k >= 2) ? 1.0f : -1.0f;
                corner[k] = v3_create(n[0] + su * u[0] + sv * v[0], n[1] + su * u[1] + sv * v[1], n[2] + su * u[2] + sv * v[2]);
            }
            mesh_push_triangle(&mesh_box, corner[0], corner[1], corner[2], normal, 0);
            mesh_push_triangle(&mesh_box, corner[0], corner[2], corner[3], normal, 0);
        }
    }
}

static void mesh_upload_shape(MeshShape *shape) {
    i32 size = shape->vertex_count * (i32)sizeof(MeshVertex);
    shape->vertex_buffer = plat_gpu_create_buffer(size, GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);
    plat_gpu_write_buffer(shape->vertex_buffer, 0, shape->vertices, size);
}

static GpuHandle mesh_create_scene_bind_group(GpuHandle pipeline) {
    GpuBindGroupDesc desc = {0};
    desc.pipeline = pipeline;
    desc.entry_count = 1;
    desc.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, mesh_scene_buffer};
    return plat_gpu_create_bind_group(&desc);
}

static void render_mesh_init(void) {
    i32 used = 0;
    for (i32 level = 0; level < MESH_SPHERE_LODS; level++) {
        mesh_spheres[level].vertices = mesh_sphere_vertices + used;
        mesh_build_sphere(&mesh_spheres[level], level + 1);
        used += mesh_spheres[level].vertex_count;
        mesh_upload_shape(&mesh_spheres[level]);
    }
    mesh_build_box();
    mesh_upload_shape(&mesh_box);

    i32 instances_size = MESH_MAX_INSTANCES * (i32)sizeof(MeshInstance);
    mesh_sphere_buffer = plat_gpu_create_buffer(instances_size, GPU_BUFFER_VERTEX | GPU_BUFFER_STORAGE | GPU_BUFFER_COPY_DST);
    mesh_box_buffer = plat_gpu_create_buffer(instances_size, GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);
    mesh_line_buffer = plat_gpu_create_buffer(sizeof(mesh_lines), GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);
    mesh_grid_buffer = plat_gpu_create_buffer(sizeof(mesh_grid), GPU_BUFFER_STORAGE | GPU_BUFFER_COPY_DST);
    mesh_scene_buffer = plat_gpu_create_buffer(sizeof(MeshScene), GPU_BUFFER_UNIFORM | GPU_BUFFER_COPY_DST);
    mesh_shadow_map = plat_gpu_create_texture(MESH_SHADOW_SIZE, MESH_SHADOW_SIZE, GPU_FORMAT_DEPTH,
                                              GPU_TEXTURE_RENDER_ATTACHMENT | GPU_TEXTURE_BINDING);

    GpuPipelineDesc pipeline = {0};
    pipeline.shader = plat_gpu_create_shader(render_shader_mesh);
    pipeline.buffer_count = 2;
    pipeline.buffers[0] = (GpuVertexBufferLayout){sizeof(MeshVertex), GPU_STEP_VERTEX};
    pipeline.buffers[1] = (GpuVertexBufferLayout){sizeof(MeshInstance), GPU_STEP_INSTANCE};
    pipeline.attribute_count = 6;
    pipeline.attributes[0] = (GpuAttribute){0, 0, 0, GPU_VERTEX_FLOAT32X3};  // position
    pipeline.attributes[1] = (GpuAttribute){0, 1, 12, GPU_VERTEX_FLOAT32X3}; // normal
    pipeline.attributes[2] = (GpuAttribute){1, 2, 0, GPU_VERTEX_FLOAT32X4};  // center
    pipeline.attributes[3] = (GpuAttribute){1, 3, 16, GPU_VERTEX_FLOAT32X4}; // half size
    pipeline.attributes[4] = (GpuAttribute){1, 4, 32, GPU_VERTEX_FLOAT32X4}; // rotation
    pipeline.attributes[5] = (GpuAttribute){1, 5, 48, GPU_VERTEX_FLOAT32X4}; // color
    pipeline.target_format = GPU_FORMAT_SCREEN;
    pipeline.blend = GPU_BLEND_NONE;
    pipeline.topology = GPU_TOPOLOGY_TRIANGLE_LIST;
    pipeline.depth = GPU_DEPTH_ON;
    pipeline.cull = GPU_CULL_BACK;
    mesh_pipeline = plat_gpu_create_pipeline(&pipeline);

    mesh_photo = plat_gpu_create_texture(MESH_PHOTO_SIZE, MESH_PHOTO_SIZE, GPU_FORMAT_RGBA8,
                                         GPU_TEXTURE_BINDING | GPU_TEXTURE_COPY_DST | GPU_TEXTURE_RENDER_ATTACHMENT);
    GpuBindGroupDesc bind_group = {0};
    bind_group.pipeline = mesh_pipeline;
    bind_group.entry_count = 7;
    bind_group.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, mesh_scene_buffer};
    bind_group.entries[1] = (GpuBindEntry){1, GPU_BIND_STORAGE, mesh_sphere_buffer};
    bind_group.entries[2] = (GpuBindEntry){2, GPU_BIND_STORAGE, mesh_grid_buffer};
    bind_group.entries[3] = (GpuBindEntry){3, GPU_BIND_TEXTURE, mesh_shadow_map};
    bind_group.entries[4] = (GpuBindEntry){4, GPU_BIND_SAMPLER, plat_gpu_create_sampler(GPU_FILTER_SHADOW)};
    bind_group.entries[5] = (GpuBindEntry){5, GPU_BIND_TEXTURE, mesh_photo};
    bind_group.entries[6] = (GpuBindEntry){6, GPU_BIND_SAMPLER, plat_gpu_create_sampler(GPU_FILTER_LINEAR)};
    mesh_bind_group = plat_gpu_create_bind_group(&bind_group);

    // same instances and attributes, seen from the sun, depth only
    GpuPipelineDesc shadow = pipeline;
    shadow.shader = plat_gpu_create_shader(render_shader_shadow);
    shadow.target_format = GPU_FORMAT_DEPTH;
    shadow.cull = GPU_CULL_FRONT;
    mesh_shadow_pipeline = plat_gpu_create_pipeline(&shadow);
    mesh_shadow_bind_group = mesh_create_scene_bind_group(mesh_shadow_pipeline);

    GpuPipelineDesc lines = {0};
    lines.shader = plat_gpu_create_shader(render_shader_line);
    lines.buffer_count = 1;
    lines.buffers[0] = (GpuVertexBufferLayout){sizeof(LineVertex), GPU_STEP_VERTEX};
    lines.attribute_count = 2;
    lines.attributes[0] = (GpuAttribute){0, 0, 0, GPU_VERTEX_FLOAT32X3};  // position
    lines.attributes[1] = (GpuAttribute){0, 1, 12, GPU_VERTEX_FLOAT32X3}; // color
    lines.target_format = GPU_FORMAT_SCREEN;
    lines.blend = GPU_BLEND_NONE;
    lines.topology = GPU_TOPOLOGY_LINE_LIST;
    lines.depth = GPU_DEPTH_ON;
    lines.cull = GPU_CULL_NONE;
    mesh_line_pipeline = plat_gpu_create_pipeline(&lines);
    mesh_line_bind_group = mesh_create_scene_bind_group(mesh_line_pipeline);
}

static void render_mesh_begin(void) {
    mesh_sphere_count = 0;
    mesh_box_count = 0;
    mesh_line_count = 0;
}

void render_set_camera(M4 view_projection, V3 eye) {
    for (i32 i = 0; i < 16; i++) mesh_scene.view_proj[i] = view_projection.m[i];
    V3 light = v3_create(0.4f, 1.0f, 0.3f); // up, a bit right and front
    v3_normalize(&light);
    mesh_scene.light[0] = light.x;
    mesh_scene.light[1] = light.y;
    mesh_scene.light[2] = light.z;
    mesh_scene.eye[0] = eye.x;
    mesh_scene.eye[1] = eye.y;
    mesh_scene.eye[2] = eye.z;

    // the projection's y scale (1 / tan(fov / 2)) is the length of the matrix's y row,
    // because the view part only rotates. times half the screen height gives pixels per meter at distance 1
    f32 *m = mesh_scene.view_proj;
    mesh_lod_scale = real_sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]) * (f32)plat_screen_height() / 2;
}

void render_set_shadow_area(V3 min, V3 max) {
    mesh_shadow_min = min;
    mesh_shadow_max = max;
}

static MeshInstance mesh_instance(V3 center, V3 half_size, f32 r, f32 g, f32 b) {
    return (MeshInstance){
        center.x, center.y, center.z, 0,
        half_size.x, half_size.y, half_size.z, 0,
        0, 0, 0, 1,
        r, g, b, 1,
    };
}

void render_add_photo_picker(const char *name) {
    plat_ui_image(name, mesh_photo);
}

static void mesh_add_sphere(V3 center, f32 radius, f32 r, f32 g, f32 b, f32 photo) {
    if (mesh_sphere_count >= MESH_MAX_INSTANCES) return;
    // distance in front of the camera (clip w). behind the camera only the shadow is seen, the same distance away
    f32 *m = mesh_scene.view_proj;
    f32 distance = m[3] * center.x + m[7] * center.y + m[11] * center.z + m[15];
    if (distance < 0) distance = -distance;
    i32 level = MESH_SPHERE_LODS - 1;
    if (distance > radius) {
        f32 pixels = radius * mesh_lod_scale / distance;
        level = 0;
        while (level < MESH_SPHERE_LODS - 1 && pixels > mesh_sphere_lod_max_pixels[level]) level++;
    }
    mesh_sphere_levels[mesh_sphere_count] = (u8)level;
    MeshInstance instance = mesh_instance(center, v3_create(radius, radius, radius), r, g, b);
    instance.photo = photo;
    mesh_sphere_instances[mesh_sphere_count++] = instance;
}

void render_draw_sphere(V3 center, f32 radius, f32 r, f32 g, f32 b) {
    mesh_add_sphere(center, radius, r, g, b, 0);
}

void render_draw_sphere_photo(V3 center, f32 radius, f32 r, f32 g, f32 b) {
    mesh_add_sphere(center, radius, r, g, b, 1);
}

void render_draw_box(V3 center, V3 half_size, f32 r, f32 g, f32 b) {
    if (mesh_box_count >= MESH_MAX_INSTANCES) return;
    mesh_box_instances[mesh_box_count++] = mesh_instance(center, half_size, r, g, b);
}

void render_draw_line(V3 from, V3 to, f32 r, f32 g, f32 b) {
    if (mesh_line_count >= MESH_MAX_LINES) return;
    mesh_lines[mesh_line_count * 2] = (LineVertex){from.x, from.y, from.z, r, g, b};
    mesh_lines[mesh_line_count * 2 + 1] = (LineVertex){to.x, to.y, to.z, r, g, b};
    mesh_line_count++;
}

// groups the spheres by level (counting sort), so each level is one range of the buffer
static void mesh_sort_spheres(void) {
    i32 next[MESH_SPHERE_LODS] = {0};
    for (i32 i = 0; i < mesh_sphere_count; i++) next[mesh_sphere_levels[i]]++;
    i32 start = 0;
    for (i32 level = 0; level < MESH_SPHERE_LODS; level++) {
        i32 count = next[level];
        mesh_sphere_level_start[level] = start;
        next[level] = start;
        start += count;
    }
    mesh_sphere_level_start[MESH_SPHERE_LODS] = start;
    for (i32 i = 0; i < mesh_sphere_count; i++) mesh_sphere_sorted[next[mesh_sphere_levels[i]]++] = mesh_sphere_instances[i];
}

static i32 mesh_grid_clamp(f32 v, u32 size) {
    i32 c = (i32)v;
    if (c < 0) return 0;
    if (c >= (i32)size) return (i32)size - 1;
    return c;
}

// the cells sorted sphere i reaches, clamped to the grid. returns how many
static i32 mesh_grid_cells_of(i32 i, i32 *x0, i32 *x1, i32 *z0, i32 *z1) {
    const MeshInstance *s = &mesh_sphere_sorted[i];
    f32 reach = s->hx * MESH_CONTACT_REACH, per_meter = mesh_scene.grid[2];
    *x0 = mesh_grid_clamp((s->x - reach - mesh_scene.grid[0]) * per_meter, mesh_scene.grid_size[0]);
    *x1 = mesh_grid_clamp((s->x + reach - mesh_scene.grid[0]) * per_meter, mesh_scene.grid_size[0]);
    *z0 = mesh_grid_clamp((s->z - reach - mesh_scene.grid[1]) * per_meter, mesh_scene.grid_size[1]);
    *z1 = mesh_grid_clamp((s->z + reach - mesh_scene.grid[1]) * per_meter, mesh_scene.grid_size[1]);
    return (*x1 - *x0 + 1) * (*z1 - *z0 + 1);
}

static void mesh_build_contact_grid(void) {
    mesh_scene.grid_size[0] = 0;
    mesh_scene.grid_size[1] = 0;
    mesh_grid_used = 0;
    if (mesh_sphere_count == 0) return;

    // the grid covers every sphere's reach. cells about one average reach wide keep both
    // the cells per sphere and the spheres per cell low
    f32 min_x = 1e30f, max_x = -1e30f, min_z = 1e30f, max_z = -1e30f, reach_sum = 0;
    for (i32 i = 0; i < mesh_sphere_count; i++) {
        const MeshInstance *s = &mesh_sphere_sorted[i];
        f32 reach = s->hx * MESH_CONTACT_REACH;
        if (s->x - reach < min_x) min_x = s->x - reach;
        if (s->x + reach > max_x) max_x = s->x + reach;
        if (s->z - reach < min_z) min_z = s->z - reach;
        if (s->z + reach > max_z) max_z = s->z + reach;
        reach_sum += reach;
    }
    f32 width = max_x - min_x, depth = max_z - min_z;
    f32 cell = reach_sum / (f32)mesh_sphere_count;
    f32 smallest = (width > depth ? width : depth) / MESH_GRID_MAX_SIDE;
    if (cell < smallest) cell = smallest;
    if (cell <= 0) cell = 1;
    i32 cols = (i32)(width / cell) + 1, rows = (i32)(depth / cell) + 1;
    if (cols > MESH_GRID_MAX_SIDE) cols = MESH_GRID_MAX_SIDE;
    if (rows > MESH_GRID_MAX_SIDE) rows = MESH_GRID_MAX_SIDE;
    mesh_scene.grid[0] = min_x;
    mesh_scene.grid[1] = min_z;
    mesh_scene.grid[2] = 1 / cell;
    mesh_scene.grid[3] = MESH_CONTACT_REACH;
    mesh_scene.grid_size[0] = (u32)cols;
    mesh_scene.grid_size[1] = (u32)rows;

    // count per cell into grid[c + 1], then add up: grid[c] = where cell c starts.
    // the same total check in both loops skips the same spheres if the entries run out
    i32 cells = cols * rows, total = 0, x0, x1, z0, z1;
    for (i32 c = 0; c <= cells; c++) mesh_grid[c] = 0;
    for (i32 i = 0; i < mesh_sphere_count; i++) {
        i32 n = mesh_grid_cells_of(i, &x0, &x1, &z0, &z1);
        if (total + n > MESH_GRID_MAX_ENTRIES) continue;
        total += n;
        for (i32 z = z0; z <= z1; z++) {
            for (i32 x = x0; x <= x1; x++) mesh_grid[z * cols + x + 1]++;
        }
    }
    mesh_grid[0] = (u32)(cells + 1); // indices start right after the cell table
    for (i32 c = 0; c < cells; c++) {
        mesh_grid[c + 1] += mesh_grid[c];
        mesh_grid_cursor[c] = mesh_grid[c];
    }

    total = 0;
    for (i32 i = 0; i < mesh_sphere_count; i++) {
        i32 n = mesh_grid_cells_of(i, &x0, &x1, &z0, &z1);
        if (total + n > MESH_GRID_MAX_ENTRIES) continue;
        total += n;
        for (i32 z = z0; z <= z1; z++) {
            for (i32 x = x0; x <= x1; x++) mesh_grid[mesh_grid_cursor[z * cols + x]++] = (u32)i;
        }
    }
    mesh_grid_used = cells + 1 + total;
}

// the sun's view: a box along the light, fitted around the shadow area. shadow map texels are spread over it
static void mesh_fit_shadow(void) {
    V3 min = mesh_shadow_min, max = mesh_shadow_max;
    V3 light = v3_create(mesh_scene.light[0], mesh_scene.light[1], mesh_scene.light[2]);
    V3 center = v3_multiplied(v3_added(min, max), 0.5f);
    V3 up = (light.y > 0.99f || light.y < -0.99f) ? v3_create(0, 0, 1) : v3_create(0, 1, 0);
    M4 view = m4_look_at(v3_added(center, light), center, up); // only the direction matters, any distance works

    V3 lo = v3_create(1e30f, 1e30f, 1e30f), hi = v3_create(-1e30f, -1e30f, -1e30f);
    for (i32 k = 0; k < 8; k++) {
        V3 corner = v3_create(k & 1 ? max.x : min.x, k & 2 ? max.y : min.y, k & 4 ? max.z : min.z);
        V3 p = m4_transform_point(view, corner);
        if (p.x < lo.x) lo.x = p.x;
        if (p.y < lo.y) lo.y = p.y;
        if (p.z < lo.z) lo.z = p.z;
        if (p.x > hi.x) hi.x = p.x;
        if (p.y > hi.y) hi.y = p.y;
        if (p.z > hi.z) hi.z = p.z;
    }
    // the view looks down -z, so the corner nearest the sun has the largest z
    M4 light_view_proj = m4_multiplied(m4_orthographic(lo.x, hi.x, lo.y, hi.y, -hi.z, -lo.z), view);
    for (i32 i = 0; i < 16; i++) mesh_scene.light_view_proj[i] = light_view_proj.m[i];

    f32 width = hi.x - lo.x, height = hi.y - lo.y;
    f32 texel = (width > height ? width : height) / MESH_SHADOW_SIZE; // meters
    mesh_scene.shadow[0] = 1.0f / MESH_SHADOW_SIZE;
    mesh_scene.shadow[1] = MESH_SHADOW_BIAS / (hi.z - lo.z);
    mesh_scene.shadow[2] = texel * 1.5f;
}

static void mesh_draw_batches(void) {
    for (i32 level = 0; level < MESH_SPHERE_LODS; level++) {
        i32 first = mesh_sphere_level_start[level];
        i32 count = mesh_sphere_level_start[level + 1] - first;
        if (count == 0) continue;
        plat_gpu_set_vertex_buffer(0, mesh_spheres[level].vertex_buffer, 0);
        plat_gpu_set_vertex_buffer(1, mesh_sphere_buffer, first * (i32)sizeof(MeshInstance));
        plat_gpu_draw(mesh_spheres[level].vertex_count, count);
    }
    if (mesh_box_count > 0) {
        plat_gpu_set_vertex_buffer(0, mesh_box.vertex_buffer, 0);
        plat_gpu_set_vertex_buffer(1, mesh_box_buffer, 0);
        plat_gpu_draw(mesh_box.vertex_count, mesh_box_count);
    }
}

// uploads the frame (scene, batches, contact grid), then draws the shadow map
static void render_mesh_shadow_pass(void) {
    mesh_sort_spheres();
    mesh_build_contact_grid();
    mesh_fit_shadow();
    plat_gpu_write_buffer(mesh_scene_buffer, 0, &mesh_scene, sizeof(mesh_scene));
    if (mesh_sphere_count > 0) {
        plat_gpu_write_buffer(mesh_sphere_buffer, 0, mesh_sphere_sorted, mesh_sphere_count * (i32)sizeof(MeshInstance));
    }
    if (mesh_box_count > 0) {
        plat_gpu_write_buffer(mesh_box_buffer, 0, mesh_box_instances, mesh_box_count * (i32)sizeof(MeshInstance));
    }
    if (mesh_grid_used > 0) plat_gpu_write_buffer(mesh_grid_buffer, 0, mesh_grid, mesh_grid_used * 4);
    if (mesh_line_count > 0) {
        plat_gpu_write_buffer(mesh_line_buffer, 0, mesh_lines, mesh_line_count * 2 * (i32)sizeof(LineVertex));
    }

    plat_gpu_begin_depth_pass(mesh_shadow_map); // even with nothing to draw, so the map is cleared
    if (mesh_sphere_count + mesh_box_count > 0) {
        plat_gpu_set_pipeline(mesh_shadow_pipeline);
        plat_gpu_set_bind_group(0, mesh_shadow_bind_group);
        mesh_draw_batches();
    }
}

// screen pass: the meshes lit and shadowed, then the lines
static void render_mesh_draw(void) {
    if (mesh_sphere_count + mesh_box_count > 0) {
        plat_gpu_set_pipeline(mesh_pipeline);
        plat_gpu_set_bind_group(0, mesh_bind_group);
        mesh_draw_batches();
    }
    if (mesh_line_count > 0) {
        plat_gpu_set_pipeline(mesh_line_pipeline);
        plat_gpu_set_bind_group(0, mesh_line_bind_group);
        plat_gpu_set_vertex_buffer(0, mesh_line_buffer, 0);
        plat_gpu_draw(mesh_line_count * 2, 1);
    }
}
