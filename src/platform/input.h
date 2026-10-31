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
};

// down = held right now. pressed = went down since the last frame (key repeat does not count)
i32 plat_key_down(i32 key);
i32 plat_key_pressed(i32 key);

#endif
