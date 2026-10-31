# focus: physics-engine

Goal: build the game's physics engine in src/phys/, following Millington's "Game Physics Engine Development" while the user reads it.

## How we work here
- The user reads the book. Claude reads along: explains sections short or in depth, as asked.
- Claude writes code only when directed. Small first, or just what a step needs.
- The user reviews code, suggests changes, states preferences. Preferences that are about how we work go to brain (ask first). Everything else goes here.
- Explain the book in our own words. Short quotes are fine, no long excerpts.
- The book is 3D C++. We write C. Translate ideas, do not copy code shape.
- The engine is built in 3D, like the book, and stays generic. The game may end up 2D or 3D (not decided). If it goes 2D, we adapt then.

## Book
- Text: reference/millington.txt (pages split by form feed). Map: reference/chapters.txt.
- The PDF, millington.txt and contents.txt are gitignored (copyrighted). On a new machine copy them by hand. chapters.txt is ours and is in git.
- pdf page = page in the PDF and in the text file. book page = pdf page - 23.
- Equations and figures are broken in the text. Read those pages from the PDF (Read tool, pages param).
- Parts: I particles (ch 2-4), II mass-aggregate (5-8), III rigid body (9-11), IV collision detection (12-13), V contact physics (14-17), VI other physics (18).
- Claude reads the book for the user. Work in small steps in book order.
- The book's own C++ source (Cyclone, MIT): reference/cyclone-physics, a gitignored local clone (local so grep and reads are fast). New machine: git clone https://github.com/idmillington/cyclone-physics.git reference/cyclone-physics. Useful to check details, not to copy shape.

## State (2026-10-13)

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
- Particle: position, velocity, acceleration, force_accum, damping (per step), inverse_mass. particle_integrate, set_mass (traps on mass <= 0), set_inverse_mass, get_mass (REAL_MAX if infinite), has_finite_mass, add_force, clear_force_accum.
- particle_collide_box: 3D box, restitution, friction on impact (both other axes).
- particle_collide_spheres: detects and resolves in one call (push apart by inverse mass, impulse along normal). Not the book's separate contact + resolver design.
- particle_bounce_factor: impacts under PHYS_REST_SPEED (0.5 m/s) do not bounce.
- Game: still flat. Everything at z = 0, box depth WORLD_D 1 m. Scale: 1000 balls cost 4.5 ms/frame CPU (80 cost 0.04 ms), all pairs is O(n^2), so 10,000 needs a broadphase grid first (book ch 12). Tried 2026-10-31, reverted to 80. Fixed step 1/60, 4 solver passes, all pairs, 80 balls, space respawns. Same behavior as the 2D version (headless sim: same overlap, no NaN).
- Not ported from cphiz yet: the explosion and fireworks demos (next step, as scenes picked by ?scene=). Skipped: v3_print (no printf), log_map (raygui slider helper).

Not built (book order): force generators (ch 5), springs (ch 6), contact struct + contact resolver (ch 7), mass-aggregate engine (ch 8), rigid bodies and rotation (ch 9-11), broad phase and contact generation (ch 12-13), resolution with rotation, resting contacts, friction, stability (ch 14-16).

## Tests and instrumentation
- ./test.sh: tests/test.c (base + phys) built to wasm, run in node. Add tests for every phys change.
- tests/headless/sim.mjs: runs the real game with a fake WebGPU, ASCII view, overlap check.
- tests/headless/jitter.mjs: per frame movement of the balls (is the pile still?).
- tests/headless/keys.mjs: fake key events.
- Stats line in the browser console once per second (fps, cpu us/frame, contacts/s, gpu calls).

## Decisions
- V2 replaced by V3 everywhere. Game uses z = 0 while it is flat (2026-10-13).
- Vector products return a value, no cphiz get_ pairs (2026-10-13).
- Next: the ball scene in 3D (balls move in z, 3D drawing) (2026-10-31). cphiz demos after that, as scenes.
- 3D physics, like the book (2026-10-13). Game dimension not decided.
- phys/ depends on base only. Never render or platform.
- Damping is per step (no libc pow). Fine because the step is fixed.

## Open questions
- 3D ball scene: draw spheres as billboard impostors (quad + shaded sphere in the shader, cheap, fits the current quad batcher) or real meshes? Fixed camera or orbit? The GPU bridge has no depth buffer yet (impostors could sort back to front instead). Needs a 4x4 matrix in base for the camera.
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
