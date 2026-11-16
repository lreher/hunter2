Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Physics from scratch (src/phys/). The game (src/demo/hunter2.c) is a flyable ship built from engine parts (src/engine/: objects, actuators, emitters, placings, inputs as data, parts as plain tables in hunter2.c), with an in-game editor panel (web/ui.js, src/engine/panel.c) on the right, hidden while flying and shown on Esc.

Stage: everything is committed and pushed (main). Deployed up to 1d7859f (stars, explosion aim); the rest is not deployed yet. Last session, all checked by me in Chrome:
- Stars on the sky (universe / stars): count, size px, brightness, twinkle, base color tinted toward tint 1 or 2. render/stars.c, no vertex buffer, a hash per star.
- Explosions on hit: cone aimed between ricochet and straight out of the surface ("straight out" on the bullet's on hit block), "denser middle" on every emitter type's cone, "speed from hit" (share of the bullet's speed into the surface added to the sparks).
- Planet looks: ball objects have a surface (pattern patches | bands | both, detail, roughness), air (rim color, thickness), colors "these three" | "10 sets" (built in sets in the shader) and variation. Each copy gets its own seed.
- Asteroids: a second placing and object. Placings have "mostly small" and "small ones faster". One spread for the universe (universe / spread). Objects have "pulls" yes | no (asteroids and bullets no: pulled but do not pull). Hits use sort and sweep along x, so 5000 asteroids run fine.
- My final save is the code defaults (gravity 2000, spread 15 km, 30 planets 282..1144 m, 5000 asteroids 5..30 m).

Task now: none set. Wait for my first instruction. Picked up, not a priority: bullets are hard to see (offered bigger, streaks along their way, slower).

How we work on this: build it up together, do not jump the gun. Talk it through first, editor UI first, then code. Small sketches (ASCII mockups are fine) that only show choices that make sense in the game, I decide. Keep parts generic game engine elements, not tied to this game (no "guns" on the ship: an emitter that emits objects). Use my words for things, keep concept talk simple and short, no invented names presented as settled. Do not offer an option you know will not give what I asked for. Say plainly when a control only acts on a button. No classes, mostly functional. I own the game design: do not pitch game ideas. Before removing or renaming any panel control or group, get my saved settings (I paste copy(localStorage.getItem("hunter2.settings.hunter2")) from the Chrome console) and put them into the code defaults first.

Finish each change in one go before replying: code, build, headless check, server restart. Do not stop halfway (I tested a half built change last session and it cost time). build.sh writes web/game.wasm, which the running server serves at once: a reload mid change picks up a half finished build, and a Save then can store wrong settings (that is why bullets "did not explode": my save held "object hit bullet=" with nothing). When something works headless but not for me, run the headless check with my save loaded before guessing.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/engine.md: "How we work here", "Universe and planets", "Explosions on hit", "Stars", "Explosion aim", "Planet looks and asteroids", "Many asteroids, shared spread", "Session 6 end", "Next", Machine notes, Decisions.
- focus/game.md: State and Decisions.
- focus/architecture.md, Decisions: layers (base <- platform <- render <- engine <- demo <- game), JS split by browser feature, naming (no shorthand).
- As needed: src/engine/engine.h, src/engine/engine.c (step order, gravity from pullers, sort and sweep hits, bursts), src/render/stars.c, src/render/mesh.c + shaders.c (surfaces), src/engine/placing.c, src/engine/emitter.c, src/engine/panel.c, src/demo/hunter2.c, web/ui.js.

Checks: ./build.sh builds, ./test.sh runs unit tests. tests/headless/fake.mjs runs the real game with a fake WebGPU and fake DOM (see Machine notes in focus/engine.md for how to pick out bullets and planets and press buttons; the fake has no localStorage, define one in the script, and it does not compile shaders, so looks can only be judged by me). To load my save headless, have the fake localStorage return it for "hunter2.settings.hunter2". I check every change in Chrome: after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends (pkill -f "^node serve.js", a plain "node serve.js" pattern also kills the shell running pkill). Commit only when I ask. Deploys only when I ask (handover/deploy.md).

Keep focus/engine.md (and focus/game.md for game changes) up to date yourself. Then tell me you are ready and wait for my first instruction.
