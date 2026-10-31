#include "base/base.h"
#include "platform/platform.h"

i32 plat_key_down(i32 key) {
    return js_key_down(key);
}

i32 plat_key_pressed(i32 key) {
    return js_key_pressed(key);
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
