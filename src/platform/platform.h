#ifndef PLATFORM_H
#define PLATFORM_H

#include "base/base.h"
#include "platform/gpu.h"
#include "platform/input.h"

// Every function JS provides is declared here, and nowhere else.
#define IMPORT(name) __attribute__((import_module("env"), import_name(name)))

// web/env.js
IMPORT("js_log") void js_log(const char *s, i32 len);
IMPORT("js_screen_width") i32 js_screen_width(void);
IMPORT("js_screen_height") i32 js_screen_height(void);
IMPORT("js_now_ms") f64 js_now_ms(void);

// web/input.js
IMPORT("js_key_down") i32 js_key_down(i32 key);
IMPORT("js_key_pressed") i32 js_key_pressed(i32 key);
IMPORT("js_mouse_locked") i32 js_mouse_locked(void);
IMPORT("js_mouse_dx") f64 js_mouse_dx(void);
IMPORT("js_mouse_dy") f64 js_mouse_dy(void);

// web/gpu.js. only platform/gpu.c should call these, everyone else uses plat_gpu_*
IMPORT("js_gpu_create_shader") GpuHandle js_gpu_create_shader(const char *wgsl, i32 len);
IMPORT("js_gpu_create_buffer") GpuHandle js_gpu_create_buffer(i32 size, i32 usage);
IMPORT("js_gpu_create_texture") GpuHandle js_gpu_create_texture(i32 w, i32 h, i32 format, i32 usage);
IMPORT("js_gpu_create_sampler") GpuHandle js_gpu_create_sampler(i32 filter);
IMPORT("js_gpu_create_pipeline") GpuHandle js_gpu_create_pipeline(const void *desc, i32 size);
IMPORT("js_gpu_create_bind_group") GpuHandle js_gpu_create_bind_group(const void *desc, i32 size);
IMPORT("js_gpu_destroy") void js_gpu_destroy(GpuHandle h);
IMPORT("js_gpu_write_buffer") void js_gpu_write_buffer(GpuHandle buffer, i32 offset, const void *data, i32 size);
IMPORT("js_gpu_write_texture") void js_gpu_write_texture(GpuHandle texture, const void *data, i32 w, i32 h, i32 bytes_per_row);
IMPORT("js_gpu_begin_frame") void js_gpu_begin_frame(f32 r, f32 g, f32 b);
IMPORT("js_gpu_set_pipeline") void js_gpu_set_pipeline(GpuHandle pipeline);
IMPORT("js_gpu_set_bind_group") void js_gpu_set_bind_group(i32 group, GpuHandle bind_group);
IMPORT("js_gpu_set_vertex_buffer") void js_gpu_set_vertex_buffer(i32 slot, GpuHandle buffer, i32 offset);
IMPORT("js_gpu_draw") void js_gpu_draw(i32 vertex_count, i32 instance_count);
IMPORT("js_gpu_end_frame") void js_gpu_end_frame(void);

// platform.c
void plat_log(const char *s);
void plat_log_i32(const char *label, i32 value); // logs "label: value"
i32 plat_screen_width(void);
i32 plat_screen_height(void);
f64 plat_now_ms(void); // milliseconds, for measuring how long things take

#endif
