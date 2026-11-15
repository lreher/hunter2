Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Engine written from scratch (src/phys/), built so far by working through Millington's "Game Physics Engine Development". The book is paused for now.

Stage: we are making a 3D game, growing out of the Gravity Cloud demo (src/demo/cloud.c). I own the design: do not guess or pitch game ideas, and do not ask leading questions about what the game should be. I tell you what to build, step by step.

Where we are: the hunter2 demo (src/demo/hunter2.c, loads by default) has a flyable ship (src/demo/ship.c) with thrusters that ramp up and down (engine cutoff), Space boost, right mouse button drift, a chase camera, and thruster exhaust: our own GPU particle system (src/render/particles.c, compute shaders in src/render/shaders.c, one emitter per nozzle, sparks fly free with the ship's velocity at emission). The panel groups fold and remember it, save/load works, and the code defaults are my saved settings. focus/game.md has the full state, decisions, controls and code maps of ship.c and particles.c. Commit only when I ask.

Next: the code is being restructured into an engine of parts (focus/engine.md, handover/engine.md); ship and particle changes go through that model now. Done since the last handover: the camera pulls back with speed (boost raises the max), and the nozzles moved (twin main, twin forward, side thrusters on the wing tops). Still open: particle looks (I say what). Important: before removing any panel control, put my saved value into the code default first (removing one without that broke the flight feel once).

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/game.md: this task. focus/engine.md: the engine model the game is moving onto.
- focus/architecture.md, the Decisions section: code rules (layers, naming, unity build). Naming: no shorthand names.
- src/demo/ship.c and src/demo/hunter2.c, then src/render/particles.c, src/render/render.h and web/ui.js as needed.

Checks: ./build.sh builds, ./test.sh runs unit tests headless (tests/test.c). tests/headless/fake.mjs runs the real game with a fake WebGPU (SCENE=<id> picks a demo; hunter2 is the default; hooks.write sees buffer writes, dispatches lists compute dispatches). WGSL cannot be checked headless, Chrome is the real test. I check every change in Chrome: for now, after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends. Deploys only when I ask (handover/deploy.md).

Keep focus/game.md up to date yourself. Then tell me you are ready and wait for my first instruction.
