#include "base/base.h"
#include "platform/platform.h"

i32 plat_key_down(i32 key) {
    return js_key_down(key);
}

i32 plat_key_pressed(i32 key) {
    return js_key_pressed(key);
}

const char *plat_key_name(i32 key) {
    static const char *names[] = {
        [KEY_SPACE] = "Space", [KEY_LEFT] = "Left", [KEY_RIGHT] = "Right", [KEY_UP] = "Up", [KEY_DOWN] = "Down",
        [KEY_W] = "W", [KEY_A] = "A", [KEY_S] = "S", [KEY_D] = "D", [KEY_Q] = "Q", [KEY_E] = "E", [KEY_SHIFT] = "Shift",
        [KEY_O] = "O", [KEY_BRACKET_LEFT] = "[", [KEY_BRACKET_RIGHT] = "]", [KEY_R] = "R", [KEY_BACKSPACE] = "Backspace",
        [KEY_CONTROL] = "Control", [KEY_MOUSE_RIGHT] = "right mouse", [KEY_MOUSE_LEFT] = "left mouse",
    };
    if (key < 0 || key >= (i32)(sizeof names / sizeof names[0]) || !names[key]) return "?";
    return names[key];
}

i32 plat_mouse_locked(void) {
    return js_mouse_locked();
}

f32 plat_mouse_dx(void) {
    return (f32)js_mouse_dx();
}

f32 plat_mouse_dy(void) {
    return (f32)js_mouse_dy();
}
