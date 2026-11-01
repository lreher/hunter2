// Unity build: this is the only file the compiler is given.
// Include order follows the layers, lowest first.
#include "base/base.h"
#include "base/math.c"
#include "base/rand.c"
#include "base/str.c"

#include "phys/phys.h"
#include "phys/particle.c"
#include "phys/collide.c"
#include "phys/force.c"

#include "platform/platform.h"
#include "platform/platform.c"
#include "platform/gpu.c"
#include "platform/input.c"

#include "render/render.h"
#include "render/shaders.c"
#include "render/font.c"
#include "render/mesh.c"
#include "render/render.c"

#include "demo/demo.h"
#include "demo/balls.c"
#include "demo/cloud.c"
#include "demo/bridge.c"
#include "demo/pool.c"
#include "demo/demo.c"

#include "game/game.h"
#include "game/game.c"

// The only functions JS can call. Everything else stays inside C.
#define EXPORT(name) __attribute__((export_name(name)))

EXPORT("game_init") void export_game_init(void) {
    game_init();
}

// called by JS on every browser frame. now_ms comes from requestAnimationFrame
EXPORT("game_frame") void export_game_frame(f64 now_ms) {
    game_frame(now_ms);
}
