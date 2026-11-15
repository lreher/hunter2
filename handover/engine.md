Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Physics from scratch (src/phys/). The game (src/demo/hunter2.c) is a flyable ship built from engine parts (src/engine/: objects, actuators, emitters, placings, inputs as data, parts as plain tables in hunter2.c), with an in-game editor panel (web/ui.js, src/engine/panel.c) on the right, hidden while flying and shown on Esc.

Stage: last session (all checked by me in Chrome, nothing committed since 9e50e02):
- Bullets: hold the left mouse button while flying. Two emitters under the wings ("under left wing" / "under right wing") use the emitter type "machine gun", which emits copies of the object "bullet" instead of sparks (an emitter type's "emits" pick: sparks or an object). Bullets glow (flat full color) with a soft halo (render_draw_glow), size, color, glow and halo in the object's model block.
- Objects heading in the panel: ship, bullet, planet, each with blocks (mass or density, drag, life, model).
- Universe heading: "gravity" (strength × real G, bounce) and the placing "planets" (count, spread, size min/max, speed min/max; changes place them again right away, "again" for a new layout). 150 planets 20..200 m across over 5 km, the camera draws to 20 km.
- Gravity on everything (ship, bullets, planets, all pairs) and hits for everything (Gravity Cloud's contact solver: planets bounce off each other, the ship and bullets hit planets with real force; bullets bounce, they are not taken away).
- My save from the session start is in the code defaults. My gravity strength since then is only in my save: ask me to paste it before you put it into the code.

Task now: quality of life improvements first (I will say which), then asteroids as well as planets, with a similar config. Wait for my first instruction.

How we work on this: build it up together, do not jump the gun. Talk it through first, editor UI first, then code. Small sketches (ASCII mockups are fine) that only show choices that make sense in the game, I decide. Keep parts generic game engine elements, not tied to this game (no "guns" on the ship: an emitter that emits objects). Use my words for things, keep concept talk simple and short, no invented names presented as settled. Do not offer an option you know will not give what I asked for (the flat "glow" was not a glow). Say plainly when a control only acts on a button. No classes, mostly functional. I own the game design: do not pitch game ideas. Before removing or renaming any panel control or group, get my saved settings (I paste copy(localStorage.getItem("hunter2.settings.hunter2")) from the Chrome console) and put them into the code defaults first.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/engine.md: "How we work here", "Bullets", "Universe and planets" (agreed sketches, what was built, code), "Next", Machine notes, Decisions.
- focus/game.md: State and Decisions.
- focus/architecture.md, Decisions: layers (base <- platform <- render <- engine <- demo <- game), JS split by browser feature, naming (no shorthand).
- As needed: src/engine/engine.h, src/engine/engine.c (step order, gravity, hits), src/engine/placing.c, src/engine/emitter.c, src/engine/panel.c, src/demo/hunter2.c, web/ui.js.

Checks: ./build.sh builds, ./test.sh runs unit tests. tests/headless/fake.mjs runs the real game with a fake WebGPU and fake DOM (see Machine notes in focus/engine.md for how to pick out bullets and planets and press buttons; the fake has no localStorage and does not compile shaders, so looks can only be judged by me). I check every change in Chrome: after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends (pkill -f "^node serve.js", a plain "node serve.js" pattern also kills the shell running pkill). Commit only when I ask. Deploys only when I ask (handover/deploy.md).

Keep focus/engine.md (and focus/game.md for game changes) up to date yourself. Then tell me you are ready and wait for my first instruction.
