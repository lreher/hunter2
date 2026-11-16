#include "base/base.h"
#include "platform/platform.h"

void plat_log(const char *s) {
    js_log(s, str_len(s));
}

void plat_log_i32(const char *label, i32 value) {
    char line[96];
    line[0] = 0;
    str_append(line, sizeof(line), label);
    str_append(line, sizeof(line), ": ");
    str_append_i32(line, sizeof(line), value);
    plat_log(line);
}

i32 plat_screen_width(void) {
    return js_screen_width();
}

i32 plat_screen_height(void) {
    return js_screen_height();
}

f64 plat_now_ms(void) {
    return js_now_ms();
}

void plat_ui_slider(const char *name, f32 min, f32 max, f32 step, f32 *value) {
    js_ui_slider(name, str_len(name), min, max, step, 0, value);
}

void plat_ui_slider_log(const char *name, f32 min, f32 max, f32 step, f32 *value) {
    js_ui_slider(name, str_len(name), min, max, step, 1, value);
}

void plat_ui_group(const char *name) {
    js_ui_group(name, str_len(name));
}

void plat_ui_fold(const char *name, const char *note) {
    js_ui_fold(name, str_len(name), note, str_len(note));
}

void plat_ui_fold_end(void) {
    js_ui_fold_end();
}

void plat_ui_row(const char *name, const char *note) {
    js_ui_row(name, str_len(name), note, str_len(note));
}

void plat_ui_button(const char *label, const char *options, i32 *target, i32 value) {
    js_ui_button(label, str_len(label), options, str_len(options), target, value);
}

void plat_ui_pick(const char *name, const char *options, i32 picked, i32 *target, i32 value) {
    js_ui_pick(name, str_len(name), options, str_len(options), picked, target, value);
}

void plat_ui_fold_button(const char *label, i32 *target, i32 value) {
    js_ui_fold_button(label, str_len(label), target, value);
}

void plat_ui_fold_rename(char *name, i32 capacity, i32 *target, i32 value) {
    js_ui_fold_rename(name, capacity, target, value);
}

void plat_ui_text(const char *name, char *text, i32 capacity, i32 *changed) {
    js_ui_text(name, str_len(name), text, capacity, changed);
}

void plat_ui_block(const char *name) {
    js_ui_block(name, str_len(name));
}

void plat_ui_block_end(void) {
    js_ui_block_end();
}

void plat_ui_color(const char *name, f32 *rgb) {
    js_ui_color(name, str_len(name), rgb);
}

void plat_ui_choice(const char *name, const char *options, i32 *value) {
    js_ui_choice(name, str_len(name), options, str_len(options), value);
}

void plat_ui_image(const char *name, GpuHandle texture) {
    js_ui_image(name, str_len(name), texture);
}

void plat_ui_clear(void) {
    js_ui_clear();
}

void plat_ui_demo(const char *id, const char *chapter, const char *name) {
    js_ui_demo(id, str_len(id), chapter, str_len(chapter), name, str_len(name));
}

void plat_ui_demo_bar(i32 *picked) {
    js_ui_demo_bar(picked);
}

void plat_ui_demo_current(i32 index) {
    js_ui_demo_current(index);
}

void plat_ui_title(const char *s, u32 color) {
    js_ui_title(s, str_len(s), color);
}

i32 plat_ui_title_bottom(void) {
    return js_ui_title_bottom();
}

void plat_ui_controls(const char *lines) {
    js_ui_controls(lines, str_len(lines));
}

void plat_ui_hint(const char *s) {
    js_ui_hint(s, str_len(s));
}
