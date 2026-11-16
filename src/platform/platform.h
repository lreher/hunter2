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

// web/ui.js
IMPORT("js_ui_title") void js_ui_title(const char *s, i32 len, u32 color);
IMPORT("js_ui_title_bottom") i32 js_ui_title_bottom(void);
IMPORT("js_ui_controls") void js_ui_controls(const char *s, i32 len);
IMPORT("js_ui_hint") void js_ui_hint(const char *s, i32 len);
IMPORT("js_ui_image") void js_ui_image(const char *name, i32 len, GpuHandle texture);
IMPORT("js_ui_slider") void js_ui_slider(const char *name, i32 len, f32 min, f32 max, f32 step, i32 log, f32 *value);
IMPORT("js_ui_choice") void js_ui_choice(const char *name, i32 len, const char *options, i32 options_len, i32 *value);
IMPORT("js_ui_clear") void js_ui_clear(void);
IMPORT("js_ui_group") void js_ui_group(const char *name, i32 len);
IMPORT("js_ui_fold") void js_ui_fold(const char *name, i32 len, const char *note, i32 note_len);
IMPORT("js_ui_fold_end") void js_ui_fold_end(void);
IMPORT("js_ui_row") void js_ui_row(const char *name, i32 len, const char *note, i32 note_len);
IMPORT("js_ui_block") void js_ui_block(const char *name, i32 len);
IMPORT("js_ui_block_end") void js_ui_block_end(void);
IMPORT("js_ui_color") void js_ui_color(const char *name, i32 len, f32 *rgb);
IMPORT("js_ui_text") void js_ui_text(const char *name, i32 len, char *text, i32 capacity, i32 *changed);
IMPORT("js_ui_button") void js_ui_button(const char *label, i32 len, const char *options, i32 options_len, i32 *target, i32 value);
IMPORT("js_ui_pick") void js_ui_pick(const char *name, i32 len, const char *options, i32 options_len, i32 picked, i32 *target, i32 value);
IMPORT("js_ui_fold_button") void js_ui_fold_button(const char *label, i32 len, i32 *target, i32 value);
IMPORT("js_ui_fold_rename") void js_ui_fold_rename(char *name, i32 capacity, i32 *target, i32 value);
IMPORT("js_ui_demo") void js_ui_demo(const char *id, i32 id_len, const char *chapter, i32 chapter_len, const char *name, i32 name_len);
IMPORT("js_ui_demo_bar") void js_ui_demo_bar(i32 *picked);
IMPORT("js_ui_demo_current") void js_ui_demo_current(i32 index);

// web/gpu.js. only platform/gpu.c should call these, everyone else uses plat_gpu_*
IMPORT("js_gpu_create_shader") GpuHandle js_gpu_create_shader(const char *wgsl, i32 len);
IMPORT("js_gpu_create_buffer") GpuHandle js_gpu_create_buffer(i32 size, i32 usage);
IMPORT("js_gpu_create_texture") GpuHandle js_gpu_create_texture(i32 w, i32 h, i32 format, i32 usage);
IMPORT("js_gpu_create_sampler") GpuHandle js_gpu_create_sampler(i32 filter);
IMPORT("js_gpu_create_pipeline") GpuHandle js_gpu_create_pipeline(const void *desc, i32 size);
IMPORT("js_gpu_create_compute_pipeline") GpuHandle js_gpu_create_compute_pipeline(GpuHandle shader);
IMPORT("js_gpu_create_bind_group") GpuHandle js_gpu_create_bind_group(const void *desc, i32 size);
IMPORT("js_gpu_destroy") void js_gpu_destroy(GpuHandle h);
IMPORT("js_gpu_write_buffer") void js_gpu_write_buffer(GpuHandle buffer, i32 offset, const void *data, i32 size);
IMPORT("js_gpu_write_texture") void js_gpu_write_texture(GpuHandle texture, const void *data, i32 w, i32 h, i32 bytes_per_row);
IMPORT("js_gpu_begin_frame") void js_gpu_begin_frame(void);
IMPORT("js_gpu_begin_compute_pass") void js_gpu_begin_compute_pass(void);
IMPORT("js_gpu_begin_depth_pass") void js_gpu_begin_depth_pass(GpuHandle depth_texture);
IMPORT("js_gpu_begin_screen_pass") void js_gpu_begin_screen_pass(f32 r, f32 g, f32 b);
IMPORT("js_gpu_set_pipeline") void js_gpu_set_pipeline(GpuHandle pipeline);
IMPORT("js_gpu_set_bind_group") void js_gpu_set_bind_group(i32 group, GpuHandle bind_group);
IMPORT("js_gpu_set_vertex_buffer") void js_gpu_set_vertex_buffer(i32 slot, GpuHandle buffer, i32 offset);
IMPORT("js_gpu_draw") void js_gpu_draw(i32 vertex_count, i32 instance_count);
IMPORT("js_gpu_dispatch") void js_gpu_dispatch(i32 groups_x, i32 groups_y, i32 groups_z);
IMPORT("js_gpu_end_frame") void js_gpu_end_frame(void);

// platform.c
void plat_log(const char *s);
void plat_log_i32(const char *label, i32 value); // logs "label: value"
i32 plat_screen_width(void);
i32 plat_screen_height(void);
f64 plat_now_ms(void); // milliseconds, for measuring how long things take

// debug panel. adds a slider that writes straight into *value whenever it moves. call once, at init
void plat_ui_slider(const char *name, f32 min, f32 max, f32 step, f32 *value);
void plat_ui_slider_log(const char *name, f32 min, f32 max, f32 step, f32 *value); // small values get more room. min > 0
void plat_ui_choice(const char *name, const char *options, i32 *value);
void plat_ui_color(const char *name, f32 *rgb); // a color picker writing 3 floats 0..1 (r, g, b). saved as "#rrggbb" // options split by |, "A|B|C". a click writes the index into *value
void plat_ui_image(const char *name, GpuHandle texture); // file picker, the image goes into the texture (needs COPY_DST + RENDER_ATTACHMENT)
// The panel is a tree of folds along the right edge, all folded at first. A slider's save key is the name of the
// fold it is in, then its own name ("engine/force N")
void plat_ui_group(const char *name);  // a fold at the top of the tree (ends any open ones). the controls after it go in it
void plat_ui_fold(const char *name, const char *note); // a fold inside the current one, note shown dim on the right
void plat_ui_fold_end(void);           // back out of the fold plat_ui_fold opened
void plat_ui_row(const char *name, const char *note);  // a line with nothing to open: a name and a dim note
// a small square group with a title, in the open fold. blocks one after another sit side by side when there is room
// and stack when not, light lines between them. not part of save keys (those stay fold name + control name)
void plat_ui_block(const char *name);
void plat_ui_block_end(void);
// Clicks that ask C for something write value into *target (C looks at it every frame and sets it back to 0)
void plat_ui_button(const char *label, const char *options, i32 *target, i32 value); // a clickable line. options
                                       // "a|b": a click shows them, picking option i writes value + i. "" = writes value
void plat_ui_pick(const char *name, const char *options, i32 picked, i32 *target, i32 value); // like plat_ui_choice,
                                       // picked one green, a click on option i writes value + i. not saved
void plat_ui_fold_button(const char *label, i32 *target, i32 value); // a small word on the open fold's line
// a double click on the open fold's name lets it be typed. Enter writes the name into name (cut to capacity, ends
// with 0), then value into *target. Esc puts it back
void plat_ui_fold_rename(char *name, i32 capacity, i32 *target, i32 value);
// a text that is saved and loaded with the panel's settings, not shown. a load (or a saved one at start, when it
// differs) writes it into text and sets *changed to 1
void plat_ui_text(const char *name, char *text, i32 capacity, i32 *changed);
void plat_ui_clear(void);            // removes every slider and picker, e.g. before another demo adds its own
void plat_ui_title(const char *s, u32 color); // big title in the top left corner. color 0xRRGGBB
i32 plat_ui_title_bottom(void);      // where the title ends, in screen pixels from the top. draw below it
void plat_ui_controls(const char *lines); // controls guide in the top right corner, monospace font. "key|action" lines
                                         // split by \n, the page adds "esc: dev menu". hidden while the panel is open. "" = none
void plat_ui_hint(const char *s);         // a line in the middle of the screen while the mouse is free. "" = none

// demo bar along the bottom, one tab per demo
void plat_ui_demo(const char *id, const char *chapter, const char *name); // adds a tab. id is what ?scene= in the URL matches
void plat_ui_demo_bar(i32 *picked);   // after the tabs: a click writes the tab's index into *picked. ?scene=<id> picks one right away
void plat_ui_demo_current(i32 index); // highlights the tab and puts ?scene=<id> in the URL

#endif
