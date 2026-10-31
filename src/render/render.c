#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"
#include "render/font.h"

// Quad batching: every draw call adds a Quad, render_end sends them all in one draw.
// Layout must match the pipeline attributes below (64 bytes).
typedef struct {
    f32 x, y, w, h;     // screen pixels, y down
    f32 u, v, uw, vh;   // atlas rect, 0..1
    f32 r, g, b, a;
    f32 shape, p1, p2, p3; // shape: 0 = font texture, 1 = circle, 2 = solid. p1..p3 unused so far
} Quad;

enum { RENDER_SHAPE_TEXTURE, RENDER_SHAPE_CIRCLE, RENDER_SHAPE_SOLID };

#define RENDER_MAX_QUADS 4096
static Quad render_quads[RENDER_MAX_QUADS];
static i32 render_quad_count;

static GpuHandle render_pipeline;
static GpuHandle render_bind_group;
static GpuHandle render_uniforms;  // screen size
static GpuHandle render_instances; // quads for the frame

void render_init(void) {
    GpuHandle shader = plat_gpu_create_shader(render_shader_quad);
    GpuHandle sampler = plat_gpu_create_sampler(GPU_FILTER_NEAREST);
    GpuHandle font_texture = render_font_create_texture();

    render_uniforms = plat_gpu_create_buffer(16, GPU_BUFFER_UNIFORM | GPU_BUFFER_COPY_DST);
    render_instances = plat_gpu_create_buffer(sizeof(render_quads), GPU_BUFFER_VERTEX | GPU_BUFFER_COPY_DST);

    GpuPipelineDesc pipeline = {0};
    pipeline.shader = shader;
    pipeline.buffer_count = 1;
    pipeline.buffers[0] = (GpuVertexBufferLayout){sizeof(Quad), GPU_STEP_INSTANCE};
    pipeline.attribute_count = 4;
    pipeline.attributes[0] = (GpuAttribute){0, 0, 0, GPU_VERTEX_FLOAT32X4};  // x y w h
    pipeline.attributes[1] = (GpuAttribute){0, 1, 16, GPU_VERTEX_FLOAT32X4}; // u v uw vh
    pipeline.attributes[2] = (GpuAttribute){0, 2, 32, GPU_VERTEX_FLOAT32X4}; // r g b a
    pipeline.attributes[3] = (GpuAttribute){0, 3, 48, GPU_VERTEX_FLOAT32X4}; // shape p1 p2 p3
    pipeline.target_format = GPU_FORMAT_SCREEN;
    pipeline.blend = GPU_BLEND_ALPHA;
    pipeline.topology = GPU_TOPOLOGY_TRIANGLE_STRIP;
    pipeline.depth = GPU_DEPTH_OFF; // 2D overlay, drawn last, always on top
    pipeline.cull = GPU_CULL_NONE;
    render_pipeline = plat_gpu_create_pipeline(&pipeline);

    GpuBindGroupDesc bind_group = {0};
    bind_group.pipeline = render_pipeline;
    bind_group.group = 0;
    bind_group.entry_count = 3;
    bind_group.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, render_uniforms};
    bind_group.entries[1] = (GpuBindEntry){1, GPU_BIND_TEXTURE, font_texture};
    bind_group.entries[2] = (GpuBindEntry){2, GPU_BIND_SAMPLER, sampler};
    render_bind_group = plat_gpu_create_bind_group(&bind_group);

    render_mesh_init();
}

void render_begin(f32 r, f32 g, f32 b) {
    render_quad_count = 0;
    render_mesh_begin();
    plat_gpu_begin_frame(r, g, b);

    f32 screen[2] = {(f32)plat_screen_width(), (f32)plat_screen_height()};
    plat_gpu_write_buffer(render_uniforms, 0, screen, sizeof(screen));
}

// returns 0 when the batch is full
static Quad *render_next_quad(f32 x, f32 y, f32 w, f32 h, i32 shape, f32 r, f32 g, f32 b, f32 a) {
    if (render_quad_count >= RENDER_MAX_QUADS) return 0;
    Quad *q = &render_quads[render_quad_count++];
    q->x = x;
    q->y = y;
    q->w = w;
    q->h = h;
    q->u = 0;
    q->v = 0;
    q->uw = 0;
    q->vh = 0;
    q->r = r;
    q->g = g;
    q->b = b;
    q->a = a;
    q->shape = (f32)shape;
    q->p1 = 0;
    q->p2 = 0;
    q->p3 = 0;
    return q;
}

static void render_push_glyph(i32 c, f32 x, f32 y, f32 size, f32 r, f32 g, f32 b) {
    if (c < 0 || c >= 128) c = '?';
    Quad *q = render_next_quad(x, y, size, size, RENDER_SHAPE_TEXTURE, r, g, b, 1.0f);
    if (!q) return;
    q->u = (f32)(c % FONT_ATLAS_COLS) / FONT_ATLAS_COLS;
    q->v = (f32)(c / FONT_ATLAS_COLS) / FONT_ATLAS_ROWS;
    q->uw = 1.0f / FONT_ATLAS_COLS;
    q->vh = 1.0f / FONT_ATLAS_ROWS;
}

void render_draw_rect(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a) {
    render_next_quad(x, y, w, h, RENDER_SHAPE_SOLID, r, g, b, a);
}

void render_draw_circle(f32 cx, f32 cy, f32 radius, f32 r, f32 g, f32 b, f32 a) {
    render_next_quad(cx - radius, cy - radius, radius * 2, radius * 2, RENDER_SHAPE_CIRCLE, r, g, b, a);
}

void render_draw_text_centered(const char *s, f32 scale, f32 r, f32 g, f32 b) {
    f32 size = FONT_GLYPH_SIZE * scale;
    f32 width = size * str_len(s);
    f32 x = (i32)((plat_screen_width() - width) / 2);
    f32 y = (i32)((plat_screen_height() - size) / 2);
    for (; *s; s++) {
        render_push_glyph((u8)*s, x, y, size, r, g, b);
        x += size;
    }
}

void render_end(void) {
    render_mesh_end();
    if (render_quad_count > 0) {
        plat_gpu_write_buffer(render_instances, 0, render_quads, render_quad_count * sizeof(Quad));
        plat_gpu_set_pipeline(render_pipeline);
        plat_gpu_set_bind_group(0, render_bind_group);
        plat_gpu_set_vertex_buffer(0, render_instances, 0);
        plat_gpu_draw(4, render_quad_count);
    }
    plat_gpu_end_frame();
}
