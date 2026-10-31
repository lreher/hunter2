# focus: physics-engine

Goal: learn how to write a physics engine by working through Millington's "Game Physics Engine Development" chapter by chapter, building it together in src/phys/. Learning the physics comes first, the game second. Rendering, cameras and tooling are only there to see and check the physics: keep them as cheap as possible (2026-10-31). Second goal: everything lightning quick. So we keep C + wasm + our own WebGPU bridge (raylib and three.js weighed and dropped 2026-10-31: both slower for many instances, raylib web is WebGL and DrawSphere builds vertices on the CPU every call). Render kit for the whole book, built once and kept minimal: instanced meshes (sphere, box), lines, a free camera.

## How we work here
- The user reads the book. Claude reads along: explains sections short or in depth, as asked.
- Claude writes code only when directed. Small first, or just what a step needs.
- The user reviews code, suggests changes, states preferences. Preferences that are about how we work go to brain (ask first). Everything else goes here.
- Explain the book in our own words. Short quotes are fine, no long excerpts.
- The book is 3D C++. We write C. Translate ideas, do not copy code shape.
- The engine is built in 3D, like the book, and stays generic. The game may end up 2D or 3D (not decided). If it goes 2D, we adapt then.

## Book
- Text: reference/millington.txt (pages split by form feed). Map: reference/chapters.txt.
- The PDF, millington.txt and contents.txt are gitignored (copyrighted). New machine: the user copies the PDF into the repo root, ./setup.sh makes the .txt files (pdftotext -layout; contents = pdf pages 8-16). chapters.txt is ours and is in git.
- pdf page = page in the PDF and in the text file. book page = pdf page - 23.
- Equations and figures are broken in the text. Read those pages from the PDF (Read tool, pages param).
- Parts: I particles (ch 2-4), II mass-aggregate (5-8), III rigid body (9-11), IV collision detection (12-13), V contact physics (14-17), VI other physics (18).
- Claude reads the book for the user. Work in small steps in book order.
- The book's own C++ source (Cyclone, MIT): reference/cyclone-physics, a gitignored local clone (local so grep and reads are fast). ./setup.sh clones it. Useful to check details, not to copy shape.

## State (2026-10-31)

cphiz (github.com/lreher/cphiz, user's earlier project, 3D, raylib). Fully covered by hunter2 now, no longer needed locally. The demo specs below are all we still needed from it.
- V3 vector library, Particle (position, velocity, acceleration, force, forceAccum, damping, inverseMass), particle_integrate.
- Demos: explosion and fireworks (specs below).
- Roughly book ch 2-4. Particle.c was mid edit (force accumulator) and does not compile. Dot product bug.

cphiz demo specs (to rebuild as scenes, values as cphiz had them):
- explosion: space resets N particles (slider 1-10000, default 100) at (0, 1, 0), velocity 0. Push: random force x,z in [-F, F], y in [F/2, F], F slider 10-1000 N (default 100). Gravity -9.8, damping 0.99, mass random 0.5-2 kg. Floor y = 0 inside x,z in [-50, 50]: y velocity *= -0.5, x,z *= 0.8. Draw radius = mass / 10, color red to yellow with speed (speed * 15, capped at 255).
- cphiz bug in explosion: the push was meant as a one off kick, but cphiz's integrate read the `force` field and never cleared it, so it acted as a constant thrust. In our engine: particle_add_force once at spawn (one step of push), or set the start velocity directly.
- fireworks: a rocket every 2 s (slider, log scale 0.01-3 s). Rocket: from (0, 1, 0), velocity x,z in [-5s, 5s], y in [10s, 15s] with speed slider s (default 1.5), gravity -9.8, damping 0.99, mass 1, life 2 s. When life runs out it explodes into 2000 particles: one random color per explosion, direction rand_unit_v3, speed 2-10, acceleration y -6, damping random 0.2-0.98, mass 0.1, life 1 s (slider), alpha fades with remaining life. Rockets drawn as tiny white spheres. Background drawn with alpha 50 (trails).
- Both used variable dt and leaked a sphere mesh per frame. Ours: fixed step, pools.

This app (src/phys/, src/base/math.c), 3D since 2026-10-13, caught up with cphiz:
- base: V3 (real = f32), paired v3_verb / v3_verbed. Products return a value: v3_scalar_product, v3_vector_product, v3_component_product (in place versions only when needed). REAL_MAX. rand_unit_v3 (uniform direction, rejection sampling. cphiz normalized cube points, which favors corners). V2 is gone.
- Particle: position, velocity, acceleration, force_accum, damping (per step), inverse_mass. particle_integrate (semi-implicit Euler, not the book's order, see Decisions), set_mass (traps on mass <= 0), set_inverse_mass, get_mass (REAL_MAX if infinite), has_finite_mass, add_force, clear_force_accum.
- particle_collide_box: 3D box, restitution, friction on impact (both other axes).
- particle_collide_spheres: detects and resolves in one call (push apart by inverse mass, impulse along normal). Not the book's separate contact + resolver design.
- particle_bounce_factor: impacts under PHYS_REST_SPEED (0.5 m/s) do not bounce.
- Game ("Erasmos Balls"): box 16 x 9 x 9 m (x 0..16, y 0..9, z -4.5..4.5). Fixed step 1/60, 4 solver passes, all pairs. Balls 80 by default, 1-1000 by slider, radius range by two sliders (0.05-0.5 and 0.05-1.0), ball bounce 0.6 and wall bounce 0.5 by slider. Space respawns. Camera moves only while the mouse is locked. 1000 balls cost ~7 ms per step (all pairs is O(n^2)): a broadphase grid (book ch 12) would fix it.
- Look: lit in linear color, soft highlight, soft sun shadows and contact shadows from the first 256 spheres (per pixel loop in the mesh shader), 5120 triangle spheres, 4x MSAA. HUD top left: title (HTML, Tiny5 from Google Fonts), then fps, overlap view (O), overlap frames/s, ball count. Debug panel top right (web/ui.js).
- Not ported from cphiz yet: the explosion and fireworks demos (next step, as scenes picked by ?scene=). Skipped: v3_print (no printf), log_map (raygui slider helper).

Not built (book order): force generators (ch 5), springs (ch 6), contact struct + contact resolver (ch 7), mass-aggregate engine (ch 8), rigid bodies and rotation (ch 9-11), broad phase and contact generation (ch 12-13), resolution with rotation, resting contacts, friction, stability (ch 14-16).

## Machine notes
- macOS machine (2026-10-31): tools from Homebrew (llvm, lld, poppler). llvm is keg only (not on PATH), find_clang.sh finds it. Apple's /usr/bin/clang has no wasm32 target.
- Linux/WSL machine (2026-10-31): tools from apt (clang, lld, poppler-utils). The game runs in Chrome on Windows at http://localhost:8000 (node serve.js in WSL). If Chrome says "no WebGPU adapter": chrome://gpu, check hardware acceleration, driver blocklist, or chrome://flags/#enable-unsafe-webgpu.

## Tests and instrumentation
- ./test.sh: tests/test.c (base + phys) built to wasm, run in node. Add tests for every phys change.
- tests/headless/fake.mjs: shared fake browser (real index.js + wasm, fake WebGPU, pointer lock). Reads the sphere instances C sends.
- tests/headless/sim.mjs: runs the real game, ASCII front and top views, inside the box, NaN and 3D overlap checks.
- tests/headless/jitter.mjs: per frame movement of the balls (is the pile still?).
- tests/headless/keys.mjs: fake key events.
- Stats line in the browser console once per second (fps, cpu us/frame, contacts/s, gpu calls).

## Decisions
- V2 replaced by V3 everywhere. Game uses z = 0 while it is flat (2026-10-13).
- Vector products return a value, no cphiz get_ pairs (2026-10-13).
- 3D ball scene done (2026-10-31). Next: cphiz demos as scenes (?scene=), or straight on to ch 5. Ask the user.
- 3D physics, like the book (2026-10-13). Game dimension not decided.
- phys/ depends on base only. Never render or platform.
- Damping is per step (no libc pow). Fine because the step is fixed.
- Integrator: semi-implicit Euler (velocity first, then position with the new velocity), not the book's order (2026-10-31). The book (ch 3, pdf 74 / book 51) drops 1/2 a t^2 as negligible and moves position with the old velocity, counting on damping < 1 to soak up the error. Under gravity that adds 1/2 g^2 dt^2 of energy per step, always the same sign: with damping 1 a 10 cm hop at restitution 0.85 never died out (37 of 80 balls still hopping at 60 s). Swapped order: all still by 10 s. Test: test_small_bounce_settles.
- Ball scene bounce: balls 0.6, walls 0.5 (was 0.9 / 0.85, looked rubbery and floaty) (2026-10-31).
- 3D balls drawn as real meshes (sphere mesh, per instance model matrix, depth buffer), not impostors. Chosen over impostors so rigid bodies (ch 9-11) reuse it (2026-10-31).
- Camera: free fly, perspective (2026-10-31). Click locks the mouse, click again releases (Esc also releases, browser rule). The camera only moves while locked: mouse look and keys (WASD, E/Q) do nothing when unlocked, so the slider panel can be used without drifting. W moves along the view direction (noclip). E up, Q down (world axis). Space stays respawn. ~5 m/s, Shift ~4x. Pitch clamped short of straight up/down, no roll. Start in front of the box, a bit above, looking at its center.
- 3D scene look: box 16 x 9 x 9 m. Solid floor plus 12 edge lines, no wall faces. One fixed angled light from above plus ambient, no shadows. Icosphere (~320 triangles), one mesh scaled per ball, all balls in one instanced draw (2026-10-31).

## Open questions
- Is the game itself going 3D now, or only this scene?
- Rebuild collisions on the book's contact + resolver design (ch 7), or keep the current one until rigid bodies?
- Next shape: non rotating boxes first, or straight to rotating rigid bodies?

## Log
- 2026-10-13: focus created. Book extracted. Headless scripts moved from session scratchpad into tests/headless/.
- 2026-10-13: handover prompt saved to handover/physics-engine.md. Brain rule 7 added (brain: ask first, focus: self managed).
- 2026-10-13: decided: physics engine is 3D and generic. Game may still go 2D. Brain rules 8 and 9 added.
- 2026-10-13: re-read cphiz and src/phys. State section confirmed, ch 2-4 gaps added. ./test.sh passes.
- 2026-10-13: step 1 done: base math and phys converted to 3D, cphiz parts caught up (products, mass helpers, random direction). Tests added: 3D vectors, mass, z wall, diagonal sphere hit, rand_unit_v3. Browser not checked yet.
- 2026-10-31: git prep. .gitignore added (build outputs, book files, .DS_Store). cphiz demo specs written here, so ../cphiz and ../cyclone-physics are no longer needed locally.
- 2026-10-31: tried 1000 small balls (4.5 ms/frame), back to 80. Cyclone cloned into reference/ (gitignored). Brain: handover workflow added. Handover rewritten. Next: 3D ball scene.
- 2026-10-31: setup.sh added (checks tools, clones Cyclone, extracts the book text, sets the git identity if missing). Tested in a fresh clone. contents.txt was cut off at 15.4.1 (pdf pages 8-14 only), now the full table of contents (pages 8-16).
- 2026-10-31: new machine (Linux/WSL). User's PDF is from a different source (re-saved by PDFium, file name "Game Physics Engine Development.pdf"). Checked: 481 pages, real text, equations and figures render, TOC on pdf 8-16, book page = pdf page - 23 holds. chapters.txt had every chapter start one page late (parts were right), fixed.
- 2026-10-31: scripts made generic. build.sh and test.sh take clang from PATH (override CLANG, WASM_LD). setup.sh test-builds a tiny wasm to check the toolchain and finds the book as "Game Physics Engine Development*.pdf". On macOS, brew llvm and lld must be on PATH (they are keg-only).
- 2026-10-31: render kit built (steps 1-5). base: f32_sin/cos/tan (Taylor to x^11 after folding into [-pi/2, pi/2]), M4 (column major), m4_identity, m4_multiplied, m4_perspective (WebGPU depth 0..1), m4_look_at. GPU bridge: depth buffer every frame (JS remakes it on resize), pipelines get 2 vertex buffer slots, depth mode, cull mode, line topology. render/mesh.c: icosphere (320 triangles) and box meshes, instanced (64 byte instance: center, half size, rotation quaternion, color), lines, camera uniform (view_proj + light). API: render_set_camera, render_draw_sphere, render_draw_box, render_draw_line. Input: click locks the mouse, click again releases, js_mouse_dx/dy per frame, Q, E, Shift. Build flag -mbulk-memory (clang emitted memset for big zeroed structs, this turns it into the wasm memory.fill instruction). Headless scripts share tests/headless/fake.mjs and read the sphere instances (3D overlap, front and top ASCII views). Headless: 3 draws and 19 gpu calls per frame, ~50 us cpu/frame, no NaN, none outside, camera W/Shift/mouse lock behave. Checked in the browser by the user: works.
- 2026-10-31: Windows Chrome said "no WebGPU adapter" (chrome://gpu: WebGPU disabled). Fixed on the user's side, see the Machine notes.
- Seen in 3D: settled balls keep sliding on the floor for a long time (8 mm/frame mean at frame 1500). Wall friction only acts on impact, so nothing slows a ball sliding along the floor. A physics topic (friction is ch 15), not a render bug.
- 2026-10-31: user: lighting looks bad, balls seem to sink into each other. Measured (scratch probe, 1200 frames): physics state worst overlap 3 mm. Drawn frames at 144 Hz (user's monitor): worst 2.5 cm, 2 frames over 2 cm (blending between steps cuts corners in bounces). Likely mostly a depth cue problem (no shadows, no highlight). Plan agreed: 1 overlap debug view, 2 sRGB, 3 specular, 4 blob shadows.
- 2026-10-31: step 1: O toggles the overlap view (balls overlapping > 1 cm in the drawn frame show white for 0.5 s). Stats line gets "overlap frames" (drawn frames per second with such an overlap), counted even with the view off. KEY_O added.
- 2026-10-31: console too noisy for the user, so on screen text: font now has A-Z, 0-9 and some punctuation (lowercase draws as uppercase), render_draw_text(s, x, y, scale). HUD top left: fps, overlap view on/off, overlap frames/s.
- 2026-10-31: user: no white balls in the overlap view, so the sinking was a depth illusion. Lighting redone in the mesh shader: sRGB in, linear lighting, sRGB out (lines and 2D untouched). Blinn-Phong highlight (needs the eye: render_set_camera(view_proj, eye)). Shadows: instead of blob discs, every drawn sphere goes into the scene uniform (center + radius, max 256 casters) and each pixel tests them: soft sun shadow (penumbra grows with distance) plus a contact shadow (dark where things touch). Cost: a loop over all spheres per pixel, fine at 80, too slow for thousands (would need shadow maps then). Camera uniform is now MeshScene (view_proj, light, eye, count, spheres). fake.mjs finds the scene uniform by its UNIFORM usage flag. WGSL is not compiled headless; the look was checked with a JS ray tracer using the same math (scratch).
- 2026-10-31: user: balls too shiny and too polygony, box edges look broken and flicker while moving. Changes: highlight 0.5 -> 0.12, power 64 -> 32. Icosphere 2 -> 4 subdivisions (5120 triangles). 4x MSAA in gpu.js (multisampled color + depth, resolved into the canvas, every pipeline count 4). Lines nudged toward the camera in clip space (z -= 2e-6 * w, ~1.6 cm at 20 m with depth24plus and near 0.05): the bottom edges lay exactly on the floor top and z-fought it. WebGPU depth bias does not work on lines, so it is done in the shader.
- 2026-10-31: user: balls bounce too much, never stop. Measured: energy fell, then ~37 balls hopped forever (one traced: same 10 cm hop every 18 frames). Cause: explicit Euler energy gain matched the bounce loss. Tried three integrators, picked semi-implicit Euler (see Decisions). Bounce lowered to 0.6 / 0.5. Floor sliding (seen earlier) also gone: all balls still by 10 s.
- 2026-10-31: debug panel. User picked an HTML panel over an own C UI. web/ui.js builds sliders C declares with plat_ui_slider(name, min, max, step, &f32), JS writes the value straight into wasm memory on input (no per frame calls). Arrow keys on a focused slider do not reach the game. Ball scene sliders: balls 1-1000, min and max radius 0.05-0.5 (crossing is fine, they swap), ball bounce, wall bounce. Bounce is live, count and sizes respawn on change. HUD shows the ball count. fake.mjs fakes the panel elements, sliders[name].set(v). Headless at 1000 balls: ~6.9 ms cpu per step frame (all pairs x4 passes plus the O(n^2) drawn overlap check), no NaN, worst overlap 2 mm. Only the first 256 balls cast shadows, and the shadow loop runs over all of them per pixel, so the GPU may struggle at high counts.
- 2026-10-31: max radius slider goes to 1.0 (min stays 0.05-0.5). 1000 balls of r 0.9-1.0 do not fit in the box (~4000 m3 of balls in 1296 m3): they stay packed and overlapping, but no NaN and none outside. Respawn takes ~12 ms.
- 2026-10-31: camera keys only work while the mouse is locked (were always on). keys.mjs checks unlocked, locked and unlocked again.
- 2026-10-31: title "Erasmos Balls" top left, green, in the Tiny5 pixel font. User chose the real font from Google Fonts (third party, OFL, needs network, falls back to monospace) over our own 5px font in C. It is an HTML div (web/ui.js, plat_ui_title), the C HUD draws below it (plat_ui_title_bottom gives its bottom edge in canvas pixels, read every frame).
- 2026-10-31: session end. State section refreshed, handover rewritten. Next: ask, ch 5 or cphiz demos.
- 2026-10-31: after a pull on macOS the page failed with "GpuPipelineDesc is 124 bytes in C, JS expects 176": game.wasm is gitignored, so it was a stale build. Rebuilt. find_clang.sh (sourced by build.sh, test.sh, setup.sh): CLANG if set, else clang on PATH if it builds wasm32, else Homebrew llvm (/opt/homebrew or /usr/local). package.json with npm start = ./build.sh && node serve.js (npm start -- 8080 for another port). Building on every start keeps the wasm in step with src/.
- 2026-10-31: M4 MacBook Air (8 core GPU, 2x Retina) ran 200 balls at ~20 fps, GPU bound (cpu < 1 ms/frame). Cause: every pixel looped over all spheres (up to 256) for shadows, made worse by 5120 triangle spheres (tiny triangles get pixels shaded several times). Fixes: sun shadows from a 2048 shadow map (depth pass from the sun, back faces, 3x3 PCF; fixed softness, no longer grows with distance), contact shadows from a floor plane grid built in C each frame (a pixel checks only spheres within 3 radii of its cell, faded from 2 radii), all balls cast shadows now (no 256 cap). Sphere LOD: 80/320/1280/5120 triangles picked by on screen radius (13/52/208 px). GPU bridge: frames are passes (begin_frame, begin_depth_pass, begin_screen_pass), GPU_FORMAT_DEPTH (depth only pipelines), GPU_FILTER_SHADOW sampler, GPU_BIND_STORAGE, GPU_CULL_FRONT. render_set_shadow_area. web/package.json marks web/ as ES modules (stops a node warning). Headless and unit tests pass. Browser check pending.
