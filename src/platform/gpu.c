#include "base/base.h"
#include "platform/platform.h"

// Each wrapper counts the crossing into JS, so we can see what a frame costs.

PlatGpuStats plat_gpu_stats;

GpuHandle plat_gpu_create_shader(const char *wgsl) {
    plat_gpu_stats.calls++;
    return js_gpu_create_shader(wgsl, str_len(wgsl));
}

GpuHandle plat_gpu_create_buffer(i32 size, i32 usage) {
    plat_gpu_stats.calls++;
    return js_gpu_create_buffer(size, usage);
}

GpuHandle plat_gpu_create_texture(i32 w, i32 h, i32 format, i32 usage) {
    plat_gpu_stats.calls++;
    return js_gpu_create_texture(w, h, format, usage);
}

GpuHandle plat_gpu_create_sampler(i32 filter) {
    plat_gpu_stats.calls++;
    return js_gpu_create_sampler(filter);
}

GpuHandle plat_gpu_create_pipeline(const GpuPipelineDesc *desc) {
    plat_gpu_stats.calls++;
    return js_gpu_create_pipeline(desc, sizeof(*desc));
}

GpuHandle plat_gpu_create_bind_group(const GpuBindGroupDesc *desc) {
    plat_gpu_stats.calls++;
    return js_gpu_create_bind_group(desc, sizeof(*desc));
}

void plat_gpu_destroy(GpuHandle h) {
    plat_gpu_stats.calls++;
    js_gpu_destroy(h);
}

void plat_gpu_write_buffer(GpuHandle buffer, i32 offset, const void *data, i32 size) {
    plat_gpu_stats.calls++;
    plat_gpu_stats.upload_bytes += size;
    js_gpu_write_buffer(buffer, offset, data, size);
}

void plat_gpu_write_texture(GpuHandle texture, const void *data, i32 w, i32 h, i32 bytes_per_row) {
    plat_gpu_stats.calls++;
    plat_gpu_stats.upload_bytes += bytes_per_row * h;
    js_gpu_write_texture(texture, data, w, h, bytes_per_row);
}

void plat_gpu_begin_frame(f32 r, f32 g, f32 b) {
    plat_gpu_stats = (PlatGpuStats){0};
    plat_gpu_stats.calls++;
    js_gpu_begin_frame(r, g, b);
}

void plat_gpu_set_pipeline(GpuHandle pipeline) {
    plat_gpu_stats.calls++;
    js_gpu_set_pipeline(pipeline);
}

void plat_gpu_set_bind_group(i32 group, GpuHandle bind_group) {
    plat_gpu_stats.calls++;
    js_gpu_set_bind_group(group, bind_group);
}

void plat_gpu_set_vertex_buffer(i32 slot, GpuHandle buffer, i32 offset) {
    plat_gpu_stats.calls++;
    js_gpu_set_vertex_buffer(slot, buffer, offset);
}

void plat_gpu_draw(i32 vertex_count, i32 instance_count) {
    plat_gpu_stats.calls++;
    plat_gpu_stats.draws++;
    js_gpu_draw(vertex_count, instance_count);
}

void plat_gpu_end_frame(void) {
    plat_gpu_stats.calls++;
    js_gpu_end_frame();
}
