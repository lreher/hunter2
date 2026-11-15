Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Physics written from scratch (src/phys/) from Millington's "Game Physics Engine Development" (book paused). The game (src/demo/hunter2.c, src/demo/ship.c) has a flyable ship with thruster exhaust from our own GPU particle system (src/render/particles.c).

Stage: we are restructuring the code into a game engine made of parts, defined together step by step. focus/engine.md has the agreed model: objects (physics objects with a list of actuators), actuators (defined on their own with a name, type push or turn, each object gets its own instance, editing the shared one changes all), emitter types (shared particle settings) and emitters (placed instances, attached to an object or on their own, each reads its own input), inputs as data (each part reads its input when it runs, no callbacks). Long term: the game shows all its parts and lets me configure them like a real game engine.

How we work on this: build it up together, do not jump the gun. No classes, mostly functional. Keep concept talk simple and short, no formulas, numbers or invented names unless I ask, never present a placeholder name as settled. Think like a game developer building it in an editor first, then the code. I own the game design: do not pitch game ideas.

Next: code work starts with the emitter (focus/engine.md, Next): emitter types and emitters, the ship's nozzles become emitters, side and up/down thrusters get their own emitter type. Settle the open points listed there with me first (where the code lives, names, how types show on the panel without breaking my saved settings). Important: before removing or renaming any panel control, put my saved value into the code default first.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/engine.md: this task, all of it.
- focus/game.md: the game's state, controls, and the code maps of ship.c and particles.c.
- focus/architecture.md, the Decisions section: code rules (layers, naming, unity build, no function pointers in the hot path). Naming: no shorthand names.
- src/demo/ship.c, src/demo/hunter2.c, src/render/particles.c, src/render/render.h, web/ui.js as needed.

Checks: ./build.sh builds, ./test.sh runs unit tests headless (tests/test.c). tests/headless/fake.mjs runs the real game with a fake WebGPU (SCENE=<id> picks a demo, hunter2 is the default; hooks.write sees buffer writes; see Machine notes in focus/engine.md). WGSL cannot be checked headless, Chrome is the real test. I check every change in Chrome: after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends. Commit only when I ask. Deploys only when I ask (handover/deploy.md).

Keep focus/engine.md (and focus/game.md for game changes) up to date yourself. Then tell me you are ready and wait for my first instruction.
