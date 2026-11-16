#include "base/base.h"
#include "platform/platform.h"
#include "render/render.h"

// Stars on the sky (render_draw_stars): each one is a direction, infinitely far away, so turning moves them across
// the screen and flying never brings them closer. Nothing is uploaded per star: the shader makes star k's direction,
// brightness, tint and twinkle timing from k alone (a hash), so the same stars come back every frame.
// Drawn first in the screen pass, added onto the clear color: everything drawn after covers them.

// layout must match Stars in render_shader_stars
typedef struct {
    f32 view_proj[16];
    f32 screen[4]; // width, height (pixels), seconds since start, unused
    f32 look[4];   // size (pixels), brightness, twinkle 0..1, unused
    f32 color[4];  // base color
    f32 tint_one[4];
    f32 tint_two[4];
} StarsFrame;

static StarsFrame stars_frame;
static GpuHandle stars_buffer, stars_pipeline, stars_bind_group;
static f32 stars_seconds;   // since start, for the twinkle
static i32 stars_count;     // to draw this frame, 0 = none (render_draw_stars not called)

static void render_stars_init(void) {
    stars_buffer = plat_gpu_create_buffer(sizeof(StarsFrame), GPU_BUFFER_UNIFORM | GPU_BUFFER_COPY_DST);
    GpuPipelineDesc pipeline = {0};
    pipeline.shader = plat_gpu_create_shader(render_shader_stars);
    pipeline.buffer_count = 0; // all made in the shader from the star's number
    pipeline.target_format = GPU_FORMAT_SCREEN;
    pipeline.blend = GPU_BLEND_ADDITIVE;
    pipeline.topology = GPU_TOPOLOGY_TRIANGLE_STRIP;
    pipeline.depth = GPU_DEPTH_OFF; // drawn first, everything after covers them
    pipeline.cull = GPU_CULL_NONE;
    stars_pipeline = plat_gpu_create_pipeline(&pipeline);
    GpuBindGroupDesc bind_group = {0};
    bind_group.pipeline = stars_pipeline;
    bind_group.entry_count = 1;
    bind_group.entries[0] = (GpuBindEntry){0, GPU_BIND_UNIFORM, stars_buffer};
    stars_bind_group = plat_gpu_create_bind_group(&bind_group);
}

static void render_stars_begin(f32 seconds) {
    stars_seconds += seconds;
    if (stars_seconds > 3600) stars_seconds -= 3600; // keeps sin() precise, the jump once an hour is a blink
    stars_count = 0;
}

static void render_stars_set_camera(Matrix4 view_projection) {
    for (i32 i = 0; i < 16; i++) stars_frame.view_proj[i] = view_projection.m[i];
}

void render_draw_stars(const RenderStars *stars) {
    stars_count = (i32)stars->count;
    if (stars_count > RENDER_MAX_STARS) stars_count = RENDER_MAX_STARS;
    stars_frame.look[0] = stars->size;
    stars_frame.look[1] = stars->brightness;
    stars_frame.look[2] = stars->twinkle;
    for (i32 i = 0; i < 3; i++) {
        stars_frame.color[i] = stars->color[i];
        stars_frame.tint_one[i] = stars->tint_one[i];
        stars_frame.tint_two[i] = stars->tint_two[i];
    }
}

// before the passes (and before the mesh scene upload, which the headless scripts read as the camera)
static void render_stars_upload(void) {
    if (stars_count <= 0) return;
    stars_frame.screen[0] = (f32)plat_screen_width();
    stars_frame.screen[1] = (f32)plat_screen_height();
    stars_frame.screen[2] = stars_seconds;
    plat_gpu_write_buffer(stars_buffer, 0, &stars_frame, sizeof(stars_frame));
}

static void render_stars_draw(void) {
    if (stars_count <= 0) return;
    plat_gpu_set_pipeline(stars_pipeline);
    plat_gpu_set_bind_group(0, stars_bind_group);
    plat_gpu_draw(4, stars_count);
}
