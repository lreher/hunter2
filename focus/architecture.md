# focus: architecture

## Status
Mostly done. Built: layers, unity build, GPU bridge, fixed step loop, stats line, input, phys/ with particles and circle collisions, headless tests.
Not done: arenas and pools, scenes by ?scene=, on screen HUD (font only has Hello World letters).
Physics work continues in physics-engine.md. The decisions below still apply to all code.

Goal: split the code into a clear, extensible structure. Know where to look for things.

## Current state
- src/main.c: everything C (log, font, atlas, quads, game_init)
- web/platform.js: everything JS (webgpu setup, shader, pipeline, imports for C)

## Decisions
- Layers (C): base <- platform <- render <- demo <- game. Each layer only uses layers below. phys depends on base only. demo/ holds one file per demo (Demo data and DEMO_* ids in demo/demo.h, table and switch dispatch in demo/demo.c, no function pointers: hot step and draw are FORCE_INLINE, Chrome runs them much slower as separate functions), game/ runs the loop, camera and shared HUD (2026-10-31).
- Each folder has one public header (e.g. render/render.h). Others include only that.
- Game never touches JS or WebGPU directly.
- src/main.c only exports game_init, game_frame.
- JS split by browser feature: web/main.js (load, loop), gpu.js, input.js, audio.js, ui.js (debug panel and title, HTML over the canvas, 2026-10-31).
- Build: unity build (main.c includes each .c). Revisit if it grows too big.
- Naming: functions module_verb_noun, types PascalCase, macros UPPER_SNAKE, locals and fields snake_case (user: case does not matter, consistency does), JS imports js_, JS code camelCase. Prefixes: base topics (str_, mem_, log_, v3_), plat_, render_, demo_ (each demo file uses its own, e.g. balls_), game has none. Private statics prefixed too.
- Memory: arenas (perm, level, frame) + pools for come-and-go things (entities, particles). No malloc. Debug counters for arena usage.
- Rendering: B. Thin JS bridge mirrors WebGPU with integer handles. Shaders, pipelines, batching all in C (render/). Batching is what matters for perf. Can upgrade to a command buffer (one crossing per frame) later if crossings show up.
- Frame timer from day one: CPU ms per frame, draw calls, C->JS crossings.
- Frame loop: fixed timestep (1/60 s) for gameplay. Render every rAF, interpolating between last two states. Cap catch-up time per frame.

## Open questions
(none)

## Log
- 2026-10-09: created focus.
- 2026-10-09: agreed on layers and folders.
- 2026-10-09: picked unity build for now.
- 2026-10-09: agreed on naming.
- 2026-10-09: picked arenas + pools.
- 2026-10-09: picked B (thin bridge, C in charge) + frame timer.
- 2026-10-09: picked fixed timestep.
- 2026-10-09: read ../cphiz (user's earlier C physics project, raylib, based on a Cyclone style particle engine). Findings below.

## cphiz review
Matches our plan:
- module_verb naming (v3_add, particle_integrate), PascalCase types (V3, Particle).
- fixed arrays + active flags for particles (basically pools, marked "// Bad" there).

User style to adopt:
- short math type names: V2 (like V3).
- paired vector funcs: in place v2_add(V2*, V2) and returning v2_added(V2, V2).
- `real` typedef (precision.h) so physics precision can switch float/double.
- one program per demo -> for us: scenes in one wasm, picked by URL (?scene=x).
- live tuning sliders (raygui) -> want our own small debug UI later.

Problems seen (verified):
- lib/particle.c does not compile (mid edit Feb 7, after last build Feb 2): C++ `&` ref in header, Particle** passed, missing `;`, Vector3 vs V3.
- v3_scalar_product returns 20 for (1,2,3).(4,5,6), expected 32. Uses + instead of * for y and z.
- integrate uses `force`, not `forceAccum`. rand_int returns float in .c, int in .h. header guard CORE_H in vector.h.
- vector.h includes raylib.h: physics coupled to renderer.
- fireworks/explosion create a sphere mesh + model every frame (GPU leak). Variable dt.

Proposed changes to plan:
- New layer phys/: depends only on base. Never on render/platform. Testable headless in node.
- No libc in wasm: sqrt via builtin, own PRNG (also deterministic). Fixed timestep means pow(damping, dt) is computed once.
- 2026-10-09: fields and locals: snake_case, be consistent.

## Build plan
1. Move code into the new tree, same behavior. DONE (browser check pending)
2. Thin GPU bridge (handles), replacing quad specific JS. Render pipelines/batching move to C. DONE (browser check pending)
3. Arenas, fixed timestep loop, frame timer. Loop + timer DONE. Arenas NOT done (balls use a static array, nothing needs arenas yet).
4. phys/ layer (port cphiz particles, fixed), scenes picked by ?scene=. phys/ DONE (2D particles), scenes NOT done.

## Step 1 log
- 2026-10-09: tree built. src/main.c (unity, only exports game_init), base/{base.h,str.c}, platform/{platform.h,platform.c}, render/{render.h,font.h,font.c,render.c}, game/{game.h,game.c}. web/{index.html,index.js,env.js,gpu.js}. Added font.h and str.c beyond the first tree. Logging lives in platform (plat_log), not base.
- Build needs -ffreestanding: clang turned str_len's loop into a strlen call (no libc).
- Checks: C stub gives the same 11 quads (x 448, y 418). index.js/env.js/gpu.js load against a fake WebGPU and log the same lines as before. Test scripts are in the session scratchpad, not the repo. Browser not checked yet.
- Temporary: render calls js_upload_texture/js_draw_quads etc. directly. Step 2 removes this.

## Step 2 log
- 2026-10-09: GPU bridge built. Contract is src/platform/gpu.h (handles, enums, descriptor structs of i32, mirrored in web/gpu.js). JS checks descriptor sizes and enum values and fails loudly. Usage flags use real WebGPU numbers.
- web/gpu.js is now generic: create_shader/buffer/texture/sampler/pipeline/bind_group, destroy, write_buffer/texture, begin_frame/set_*/draw/end_frame. It no longer knows what a quad is.
- C side: platform/gpu.c wrappers (plat_gpu_*) count crossings in plat_gpu_stats. render/shaders.c holds WGSL, render.c builds the pipeline and batches quads, font.c returns a texture handle. Only gpu.c calls js_gpu_*.
- New base helpers: str_append, str_append_i32. plat_log_i32 logs "label: value".
- Checks (fake WebGPU, real wasm): descriptors reaching WebGPU match the old hardcoded setup exactly. Per frame: 8 C->JS calls, 1 draw, 536 bytes uploaded. Init: 8 calls. Browser not checked yet.
- Limits: entry points must be "vs"/"fs". One vertex buffer slot (two since 2026-10-31). Handles are never reused.

## Step 3/4 log (loop, physics, balls)
- 2026-10-09: base/math.c (V2, cphiz names, paired v2_verb / v2_verbed, 2D), base/rand.c (xorshift32, seeded), phys/{phys.h,particle.c} (Particle, particle_integrate, particle_collide_box). phys depends on base only.
- cphiz fixes: dot product, force_accum is used, `force` field dropped, no raylib in the math. Damping is per step (no libc pow), fine because the step is fixed.
- Prefix note: phys uses topic prefixes like base does: v2_, rand_, particle_.
- tests/ + test.sh: headless wasm tests run in node (vectors, free fall, forces, damping, box bounce, rand, str). Mutation check: putting back the cphiz dot product bug makes a test fail.
- Render: Quad is now 64 bytes (added shape). Shapes: font texture, circle (shader, smooth edge), solid. render_draw_rect, render_draw_circle. web/gpu.js needed no change for this.
- Game: fixed step 1/60, catch-up capped at 0.25 s, draw blends previous and current positions. 16x9 m world fitted to screen. 80 balls, gravity, box bounce (restitution 0.85). JS: rAF loop calls game_frame(now_ms), canvas resizes with the window, js_now_ms.
- Stats line once per second in the console: fps, cpu us/frame, steps/s, gpu calls, draws, upload bytes.
- Headless sim (fake WebGPU, 200 frames): 81 quads, 1 draw, 8 gpu calls/frame, 5192 bytes/frame, cpu ~11 us/frame, balls stay inside the box. Browser not checked yet.
- Not done: ball vs ball collisions, arenas/pools, scenes, on screen HUD (font only has the letters of Hello World).

## Collisions log
- 2026-10-09: phys/collide.c: particle_collide_circles (push apart by inverse mass, impulse along the normal, returns 1 on contact). Walls use the same rest rule via particle_bounce_factor (impacts under PHYS_REST_SPEED 0.5 m/s do not bounce). Without it the pile jittered (0.17 px/frame mean), with it 0.015.
- Game: all pairs (no broadphase), SOLVER_PASSES 4 (pairs, then walls, each pass), spawn rejects overlaps (100 tries each), BALL_BOUNCE 0.9, WALL_BOUNCE 0.85. New stat: contacts/s.
- Tests: head on swap, no overlap, separating, momentum + energy conserved, inelastic, immovable wall, two walls, slow impact, same position (no NaN), slow floor hit.
- Headless sim (900 frames, 80 balls): no NaN, none outside box, pile settles. Worst overlap in the pile ~1.7 px at 100 px/m (~1.7 cm). cpu 45 us/frame (was 11 before collisions). Browser not checked yet.
- Known limits: O(n^2) pair checks (fine at 80, add a grid when stats show it matters). No rotation, so no rolling friction: balls slide. Fast small balls could tunnel through each other (no swept test).

## Input log
- 2026-10-09: web/input.js (key state, "pressed" lasts one frame, key repeat ignored, blur clears held keys, preventDefault on mapped keys). src/platform/input.h (KEY_ enum, order must match KEY_NAMES in input.js), input.c (plat_key_down, plat_key_pressed). index.js calls input.endFrame() after each game_frame. Polling means one C->JS call per query.
- Game: space respawns all balls. Seed is set once in game_init, so respawns give new layouts, first layout is always the same. Logs "balls spawned: N".
- Checked with simulated events on the real wasm: 1 press = 1 respawn, held key (repeat events) = 1 respawn, unknown key and arrows do nothing, second press gives a different layout, blur then press still works. Browser not checked yet.

## 3D render kit
- 2026-10-31: depth buffer, 2 vertex buffer slots, depth and cull modes, line topology in the GPU bridge. render/mesh.c: instanced spheres and boxes, lines, camera. Details in physics-engine.md log. Build uses -mbulk-memory.

## Reference book
- 2026-10-13: Millington "Game Physics Engine Development" (481 pages) extracted with pdftotext -layout to reference/millington.txt (pages split by form feed). reference/chapters.txt = chapter to pdf page map (book page = pdf page - 23). reference/contents.txt = raw table of contents (small caps are mangled). Equations and figures do not survive as text: read those pages from the PDF directly (Read tool, pages param).
- Book has six parts: I particles (ch 2-4), II mass-aggregate (5-8), III rigid body (9-11), IV collision detection (12-13), V contact physics (14-17), VI what comes next (18). Rigid body rotation is ch 9-11, collision detection and contacts ch 12-15, stability ch 16.
