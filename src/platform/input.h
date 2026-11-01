#ifndef INPUT_H
#define INPUT_H

#include "base/base.h"

// Keys the game can ask about. Order must match KEY_NAMES in web/input.js.
enum {
    KEY_SPACE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_Q,
    KEY_E,
    KEY_SHIFT, // either shift key
    KEY_O,
    KEY_BRACKET_LEFT,
    KEY_BRACKET_RIGHT,
    KEY_R,
};

// down = held right now. pressed = went down since the last frame (key repeat does not count)
i32 plat_key_down(i32 key);
i32 plat_key_pressed(i32 key);

// mouse movement in pixels since the last frame, only while the mouse is locked (click the canvas)
i32 plat_mouse_locked(void);
f32 plat_mouse_dx(void);
f32 plat_mouse_dy(void); // positive = down

#endif
