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

void plat_ui_image(const char *name, GpuHandle texture) {
    js_ui_image(name, str_len(name), texture);
}

void plat_ui_title(const char *s) {
    js_ui_title(s, str_len(s));
}

i32 plat_ui_title_bottom(void) {
    return js_ui_title_bottom();
}
