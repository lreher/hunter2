Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Engine written from scratch (src/phys/), built so far by working through Millington's "Game Physics Engine Development". The book is paused for now.

New stage: we are making a game out of the Gravity Cloud demo (src/demo/cloud.c). The game is 3D. I own the design: do not guess or pitch game ideas, and do not ask leading questions about what the game should be. I will tell you what to build, step by step.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/game.md: this task.
- focus/architecture.md, the Decisions section: code rules (layers, naming, unity build). Naming: no shorthand names.
- src/demo/cloud.c, then src/phys/phys.h, src/phys/force.c, src/phys/particle.c as needed.

Checks: ./build.sh builds, ./test.sh runs unit tests headless (tests/test.c). tests/headless/fake.mjs runs the real game with a fake WebGPU (SCENE=<id> picks a demo). I check every change in Chrome (npm start: http://localhost:8000). Stop any server you start when done. Deploys only when I ask (handover/deploy.md).

Keep focus/game.md up to date yourself. Then tell me you are ready and wait for my first instruction.
