#include "base/base.h"
#include "platform/platform.h"

i32 plat_key_down(i32 key) {
    return js_key_down(key);
}

i32 plat_key_pressed(i32 key) {
    return js_key_pressed(key);
}
