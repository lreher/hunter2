#ifndef GPU_H
#define GPU_H

#include "base/base.h"

// Thin mirror of WebGPU. C owns shaders, pipelines and batching.
// JS only keeps a table of real objects and hands out integer handles.
// Enum values and descriptor layouts here must match web/gpu.js.

typedef i32 GpuHandle; // 0 = invalid

// usage flags use the real WebGPU values, JS passes them straight through
#define GPU_BUFFER_COPY_DST 8
#define GPU_BUFFER_VERTEX 32
#define GPU_BUFFER_UNIFORM 64
#define GPU_TEXTURE_COPY_DST 2
#define GPU_TEXTURE_BINDING 4

enum { GPU_FORMAT_R8, GPU_FORMAT_RGBA8, GPU_FORMAT_SCREEN };
enum { GPU_VERTEX_FLOAT32X2, GPU_VERTEX_FLOAT32X3, GPU_VERTEX_FLOAT32X4 };
enum { GPU_STEP_VERTEX, GPU_STEP_INSTANCE };
enum { GPU_TOPOLOGY_TRIANGLE_LIST, GPU_TOPOLOGY_TRIANGLE_STRIP };
enum { GPU_BLEND_NONE, GPU_BLEND_ALPHA, GPU_BLEND_ADDITIVE };
enum { GPU_FILTER_NEAREST, GPU_FILTER_LINEAR };
enum { GPU_BIND_UNIFORM, GPU_BIND_TEXTURE, GPU_BIND_SAMPLER };

// Descriptors are plain structs of i32 that JS reads straight from wasm memory.
// JS checks the size, so a layout mismatch fails loudly.
// Shader entry points are always named "vs" and "fs".

#define GPU_MAX_ATTRIBUTES 8
#define GPU_MAX_BIND_ENTRIES 8

typedef struct {
    i32 location;
    i32 offset;
    i32 format; // GPU_VERTEX_*
} GpuAttribute;

typedef struct {
    GpuHandle shader;
    i32 vertex_stride; // 0 = no vertex buffer
    i32 vertex_step;   // GPU_STEP_*
    i32 attribute_count;
    i32 target_format; // GPU_FORMAT_*
    i32 blend;         // GPU_BLEND_*
    i32 topology;      // GPU_TOPOLOGY_*
    GpuAttribute attributes[GPU_MAX_ATTRIBUTES];
} GpuPipelineDesc;

typedef struct {
    i32 binding;
    i32 type; // GPU_BIND_*
    GpuHandle handle;
} GpuBindEntry;

typedef struct {
    GpuHandle pipeline; // layout comes from this pipeline
    i32 group;
    i32 entry_count;
    GpuBindEntry entries[GPU_MAX_BIND_ENTRIES];
} GpuBindGroupDesc;

// Counts what crosses from C to JS. Reset by plat_gpu_begin_frame.
typedef struct {
    i32 calls;
    i32 draws;
    i32 upload_bytes;
} PlatGpuStats;
extern PlatGpuStats plat_gpu_stats;

GpuHandle plat_gpu_create_shader(const char *wgsl);
GpuHandle plat_gpu_create_buffer(i32 size, i32 usage);
GpuHandle plat_gpu_create_texture(i32 w, i32 h, i32 format, i32 usage);
GpuHandle plat_gpu_create_sampler(i32 filter);
GpuHandle plat_gpu_create_pipeline(const GpuPipelineDesc *desc);
GpuHandle plat_gpu_create_bind_group(const GpuBindGroupDesc *desc);
void plat_gpu_destroy(GpuHandle h);

void plat_gpu_write_buffer(GpuHandle buffer, i32 offset, const void *data, i32 size);
void plat_gpu_write_texture(GpuHandle texture, const void *data, i32 w, i32 h, i32 bytes_per_row);

// one frame = clear the screen, then draw into it
void plat_gpu_begin_frame(f32 r, f32 g, f32 b);
void plat_gpu_set_pipeline(GpuHandle pipeline);
void plat_gpu_set_bind_group(i32 group, GpuHandle bind_group);
void plat_gpu_set_vertex_buffer(i32 slot, GpuHandle buffer, i32 offset);
void plat_gpu_draw(i32 vertex_count, i32 instance_count);
void plat_gpu_end_frame(void);

#endif
