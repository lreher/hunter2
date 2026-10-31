Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Engine written from scratch. The game may end up 2D or 3D, not decided yet.

Then read:
- focus/physics-engine.md: this task. Book map, physics state, decisions, open questions, log.
- focus/architecture.md, the Decisions section: code rules (layers, naming, unity build). They apply to all code.
- src/phys/ and src/base/math.c: the physics code so far.

The task: we build the game's physics engine in src/phys/ following Ian Millington's "Game Physics Engine Development". The engine is 3D and generic, like the book. You read the book for me and explain what matters, short or in depth when I ask. We work in small steps in book order. Write code only when I direct it, small first. I review the code and tell you my preferences. Keep focus/physics-engine.md up to date yourself. Ask before changing brain.md.

The book: reference/millington.txt (pages split by form feed), reference/chapters.txt maps chapters to pages. Equations and figures do not survive as text, so read those pages from the PDF directly. Explain in your own words, no long excerpts. The book's own C++ source is in reference/cyclone-physics. The PDF, the extracted text and cyclone-physics are gitignored: if they are missing, ask me (the clone command is in .gitignore).

Checks: ./build.sh builds the game, ./test.sh runs the phys tests headless. tests/headless/*.mjs run the real game with a fake WebGPU (FRAMES=60 node tests/headless/sim.mjs for a short run). Run them after changes. Only tell me about them when it matters.

Where we are (2026-10-31): base math and phys are 3D (V3) and cover everything from my old cphiz project (book ch 2-4, plus our own box walls and sphere collisions). The game is still flat: 80 balls at z = 0, drawn as 2D circles.

Next step: make the ball scene 3D. Balls move in a 3D box and are drawn in 3D. Start by settling the open questions in the focus file (how to draw spheres, camera, depth), then build it in small steps.
