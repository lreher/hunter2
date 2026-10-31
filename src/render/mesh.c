#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"

// 3D drawing: instanced meshes (sphere, box) and lines, in world meters, y up.
// Every draw call adds to a batch. render_end sends each batch in one upload and one draw.

typedef struct {
    f32 x, y, z;
    f32 nx, ny, nz;
} MeshVertex;

// One drawn object. Layout must match the pipeline attributes below (64 bytes).
typedef struct {
    f32 x, y, z, pad0;
    f32 hx, hy, hz, pad1; // half size per axis (radius for spheres)
    f32 qx, qy, qz, qw;   // rotation quaternion. identity until rigid bodies need it
    f32 r, g, b, a;
} MeshInstance;

typedef struct {
    f32 x, y, z;
    f32 r, g, b;
} LineVertex;

typedef struct {
    MeshVertex *vertices;
    i32 vertex_count;
    MeshInstance *instances;
    i32 instance_count;
    GpuHandle vertex_buffer;
    GpuHandle instance_buffer;
} Mesh;

#define MESH_SPHERE_SUBDIVISIONS 4 // 20 triangles, x4 each time: 5120
#define MESH_SPHERE_TRIANGLES (20 << (2 * MESH_SPHERE_SUBDIVISIONS))
#define MESH_MAX_INSTANCES 16384
#define MESH_MAX_LINES 16384

static MeshVertex mesh_sphere_vertices[MESH_SPHERE_TRIANGLES * 3];
static MeshVertex mesh_box_vertices[36];
static MeshInstance mesh_sphere_instances[MESH_MAX_INSTANCES];
static MeshInstance mesh_box_instances[MESH_MAX_INSTANCES];
static LineVertex mesh_lines[MESH_MAX_LINES * 2];
static i32 mesh_line_count;

static Mesh mesh_sphere = {mesh_sphere_vertices, 0, mesh_sphere_instances, 0, 0, 0};
static Mesh mesh_box = {mesh_box_vertices, 0, mesh_box_instances, 0, 0, 0};

// what the mesh shader knows about the frame. layout must match Scene in shaders.c
#define MESH_MAX_SHADOW_SPHERES 256 // spheres past this still draw, but cast no shadow
typedef struct {
    f32 view_proj[16];
    f32 light[4]; // direction toward the sun
    f32 eye[4];
    u32 sphere_count, pad[3];
    f32 spheres[MESH_MAX_SHADOW_SPHERES][4]; // center, radius
} MeshScene;

static MeshScene mesh_scene;
static GpuHandle mesh_scene_buffer;
static GpuHandle mesh_pipeline;
static GpuHandle mesh_bind_group;
static GpuHandle mesh_line_pipeline;
static GpuHandle mesh_line_bind_group;
static GpuHandle mesh_line_buffer;

// adds a triangle facing away from the mesh center (counter clockwise seen from outside, so culling keeps it)
static void mesh_push_triangle(Mesh *mesh, V3 a, V3 b, V3 c, V3 outward, i32 smooth) {
    V3 face = v3_vector_product(v3_subtracted(b, a), v3_subtracted(c, a));
    if (v3_scalar_product(face, outward) < 0) {
        V3 t = b;
        b = c;
        c = t;
    }
    V3 corners[3] = {a, b, c};
    for (i32 i = 0; i < 3; i++) {
        V3 n = smooth ? corners[i] : outward; // a unit sphere's normal is its position
        mesh->vertices[mesh->vertex_count++] = (MeshVertex){corners[i].x, corners[i].y, corners[i].z, n.x, n.y, n.z};
    }
}

static V3 mesh_midpoint_on_sphere(V3 a, V3 b) {
    V3 m = v3_added(a, b);
    v3_normalize(&m);
    return m;
}

// icosahedron, then split every triangle into 4 and push the new corners out to the sphere
static void mesh_build_sphere(void) {
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
    static V3 triangles[2][MESH_SPHERE_TRIANGLES][3]; // ping pong between two lists
    for (i32 i = 0; i < 12; i++) v3_normalize(&corners[i]);
    for (i32 f = 0; f < 20; f++) {
        for (i32 k = 0; k < 3; k++) triangles[0][f][k] = corners[faces[f][k]];
    }

    i32 count = 20, from = 0;
    for (i32 level = 0; level < MESH_SPHERE_SUBDIVISIONS; level++) {
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
        mesh_push_triangle(&mesh_sphere, tri[0], tri[1], tri[2], v3_added(v3_added(tri[0], tri[1]), tri[2]), 1);
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

static void mesh_upload(Mesh *mesh) {
    i32 size = mesh->vertex_count * (i32)sizeof(MeshVertex);
    mesh->vertex_buffer = plat_gpu_create_buffer(size, GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);
    plat_gpu_write_buffer(mesh->vertex_buffer, 0, mesh->vertices, size);
    mesh->instance_buffer = plat_gpu_create_buffer(MESH_MAX_INSTANCES * sizeof(MeshInstance), GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);
}

static GpuHandle mesh_create_bind_group(GpuHandle pipeline) {
    GpuBindGroupDesc desc = {0};
    desc.pipeline = pipeline;
    desc.entry_count = 1;
    desc.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, mesh_scene_buffer};
    return plat_gpu_create_bind_group(&desc);
}

static void render_mesh_init(void) {
    mesh_build_sphere();
    mesh_build_box();
    mesh_upload(&mesh_sphere);
    mesh_upload(&mesh_box);
    mesh_scene_buffer = plat_gpu_create_buffer(sizeof(MeshScene), GPU_BUFFER_UNIFORM | GPU_BUFFER_COPY_DST);
    mesh_line_buffer = plat_gpu_create_buffer(sizeof(mesh_lines), GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);

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
    mesh_bind_group = mesh_create_bind_group(mesh_pipeline);

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
    mesh_line_bind_group = mesh_create_bind_group(mesh_line_pipeline);
}

static void render_mesh_begin(void) {
    mesh_sphere.instance_count = 0;
    mesh_box.instance_count = 0;
    mesh_line_count = 0;
    mesh_scene.sphere_count = 0;
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
}

static void mesh_add(Mesh *mesh, V3 center, V3 half_size, f32 r, f32 g, f32 b) {
    if (mesh->instance_count >= MESH_MAX_INSTANCES) return;
    mesh->instances[mesh->instance_count++] = (MeshInstance){
        center.x, center.y, center.z, 0,
        half_size.x, half_size.y, half_size.z, 0,
        0, 0, 0, 1,
        r, g, b, 1,
    };
}

void render_draw_sphere(V3 center, f32 radius, f32 r, f32 g, f32 b) {
    if (mesh_scene.sphere_count < MESH_MAX_SHADOW_SPHERES) {
        f32 *s = mesh_scene.spheres[mesh_scene.sphere_count++];
        s[0] = center.x;
        s[1] = center.y;
        s[2] = center.z;
        s[3] = radius;
    }
    mesh_add(&mesh_sphere, center, v3_create(radius, radius, radius), r, g, b);
}

void render_draw_box(V3 center, V3 half_size, f32 r, f32 g, f32 b) {
    mesh_add(&mesh_box, center, half_size, r, g, b);
}

void render_draw_line(V3 from, V3 to, f32 r, f32 g, f32 b) {
    if (mesh_line_count >= MESH_MAX_LINES) return;
    mesh_lines[mesh_line_count * 2] = (LineVertex){from.x, from.y, from.z, r, g, b};
    mesh_lines[mesh_line_count * 2 + 1] = (LineVertex){to.x, to.y, to.z, r, g, b};
    mesh_line_count++;
}

static void mesh_draw(Mesh *mesh) {
    if (mesh->instance_count == 0) return;
    plat_gpu_write_buffer(mesh->instance_buffer, 0, mesh->instances, mesh->instance_count * sizeof(MeshInstance));
    plat_gpu_set_vertex_buffer(0, mesh->vertex_buffer, 0);
    plat_gpu_set_vertex_buffer(1, mesh->instance_buffer, 0);
    plat_gpu_draw(mesh->vertex_count, mesh->instance_count);
}

static void render_mesh_end(void) {
    // only the spheres used this frame are uploaded
    i32 scene_bytes = (i32)(sizeof(MeshScene) - sizeof(mesh_scene.spheres)) + (i32)mesh_scene.sphere_count * 16;
    plat_gpu_write_buffer(mesh_scene_buffer, 0, &mesh_scene, scene_bytes);
    if (mesh_sphere.instance_count + mesh_box.instance_count > 0) {
        plat_gpu_set_pipeline(mesh_pipeline);
        plat_gpu_set_bind_group(0, mesh_bind_group);
        mesh_draw(&mesh_sphere);
        mesh_draw(&mesh_box);
    }
    if (mesh_line_count > 0) {
        plat_gpu_write_buffer(mesh_line_buffer, 0, mesh_lines, mesh_line_count * 2 * sizeof(LineVertex));
        plat_gpu_set_pipeline(mesh_line_pipeline);
        plat_gpu_set_bind_group(0, mesh_line_bind_group);
        plat_gpu_set_vertex_buffer(0, mesh_line_buffer, 0);
        plat_gpu_draw(mesh_line_count * 2, 1);
    }
}
