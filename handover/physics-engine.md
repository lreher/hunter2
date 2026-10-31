Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Engine written from scratch. The game may end up 2D or 3D, not decided yet.

The main goal is learning: we work through Ian Millington's "Game Physics Engine Development" chapter by chapter and build its physics engine together in src/phys/. The physics comes first, the game second. Second goal: everything lightning quick, which is why we stay with C + wasm + our own WebGPU bridge. Rendering, camera and tooling only exist to see and check the physics: keep them cheap, do not let them grow into their own project.

Setup: run ./setup.sh first. It checks the tools (it test builds a tiny wasm), clones the book's source code, extracts the book text and sets the git identity, and only does what is missing. Tools come from PATH (override with CLANG, WASM_LD). If it reports MISSING, fix what you can (tools: ask before installing) and ask me for the rest. The book PDF is copyrighted and cannot be downloaded: I copy it into the repo root myself (any "Game Physics Engine Development*.pdf").

Then read:
- focus/physics-engine.md: this task. Goal, book map, physics state, decisions, machine notes, open questions, log.
- focus/architecture.md, the Decisions section: code rules (layers, naming, unity build). They apply to all code.
- src/phys/ and src/base/math.c: the physics code so far.

How we work: you read the book for me and explain what matters, short or in depth when I ask. We work in small steps in book order. Write code only when I direct it, small first. I review the code and tell you my preferences. Keep focus/physics-engine.md up to date yourself. Ask before changing brain.md.

The book: reference/millington.txt (pages split by form feed), reference/chapters.txt maps chapters to pages (book page = pdf page - 23). Equations and figures do not survive as text, so read those pages from the PDF directly. Explain in your own words, no long excerpts. The book's own C++ source is in reference/cyclone-physics. All of these are gitignored, ./setup.sh makes them.

Checks: ./build.sh builds the game, ./test.sh runs the unit tests headless. tests/headless/*.mjs run the real game with a fake WebGPU (FRAMES=60 node tests/headless/sim.mjs for a short run, keys.mjs for input and camera, jitter.mjs for pile movement). Run them after changes. Only tell me about them when it matters. I check every change in Chrome on Windows (node serve.js, http://localhost:8000).

Where we are (2026-10-31): physics covers book ch 2-4 (particles, integration, forces, damping) plus our own box walls and sphere collisions. The ball scene is now 3D: 80 balls in a 16 x 9 x 9 m box, lit sphere meshes, free fly camera (click locks the mouse, WASD, E/Q, Shift). The render kit is built for the whole book: instanced spheres and boxes (with a rotation slot for rigid bodies later), lines, camera. One physics thing seen: settled balls keep sliding on the floor, because friction only acts on impact (book ch 15).

Next step: back to the book. Ask me first: port the cphiz demos (explosion, fireworks, specs in the focus file) as scenes picked by ?scene=, or go straight to chapter 5 (force generators). Then work in small steps.
