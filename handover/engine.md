Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Physics from scratch (src/phys/). The game (src/demo/hunter2.c) is a flyable ship built from engine parts (src/engine/: objects, actuators, emitters, placings, inputs as data, parts as plain tables in hunter2.c), with an in-game editor panel (web/ui.js, src/engine/panel.c) on the right, hidden while flying and shown on Esc.

Stage: everything is committed (085bf35), pushed and deployed (https://hunter2.lucasreher.com). Last session, all checked by me in Chrome:
- Roll: "when idle" nothing | level on the roll actuator, starts on nothing (space has no up). A try at letting Q/E set the level broke the camera and controls and was taken out again.
- Up/down thrusters: two emitters on top and two under the middle of the wings (left/right wing top/bottom), in place of keel and top ridge.
- Planets are drawn with the renderer's smooth sphere (render_draw_sphere, as in the other demos), not a built model.
- Bullets explode on hit: an object's "on hit" block (burst pick, gone after steps, moves with what it hits | nothing) fires an emitter type once where the copy was, one step after its push lands. "explosion" is a "sparks once" emitter type (the emits pick: sparks | sparks once | an object). Render takes 16 emitters a frame, extra bursts wait a frame.
- Loading screen naming each step, step times in the console ("[js] load ...").
- My save is the code defaults (gravity 11858 x real G, bounce 0.95, 396 planets over 4.5 km, 1..197 m).
- "Erasmos Balls" is now "Ball Pit".

Task now: asteroids as well as planets, with a similar config. Talk it through first (likely a second object and placing under universe). Wait for my first instruction.

How we work on this: build it up together, do not jump the gun. Talk it through first, editor UI first, then code. Small sketches (ASCII mockups are fine) that only show choices that make sense in the game, I decide. Keep parts generic game engine elements, not tied to this game (no "guns" on the ship: an emitter that emits objects). Use my words for things, keep concept talk simple and short, no invented names presented as settled. Do not offer an option you know will not give what I asked for. Say plainly when a control only acts on a button. No classes, mostly functional. I own the game design: do not pitch game ideas. Before removing or renaming any panel control or group, get my saved settings (I paste copy(localStorage.getItem("hunter2.settings.hunter2")) from the Chrome console) and put them into the code defaults first.

Finish each change in one go before replying: code, build, headless check, server restart. Do not stop halfway (I tested a half built change last session and it cost time). build.sh writes web/game.wasm, which the running server serves at once: a reload mid change picks up a half finished build, and a Save then can store wrong settings (that is why bullets "did not explode": my save held "object hit bullet=" with nothing). When something works headless but not for me, run the headless check with my save loaded before guessing.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/engine.md: "How we work here", "Universe and planets", "Quality of life", "Explosions on hit", "Defaults from the user's save", "Next", Machine notes, Decisions.
- focus/game.md: State and Decisions.
- focus/architecture.md, Decisions: layers (base <- platform <- render <- engine <- demo <- game), JS split by browser feature, naming (no shorthand).
- As needed: src/engine/engine.h, src/engine/engine.c (step order, gravity, hits, bursts), src/engine/placing.c, src/engine/emitter.c, src/engine/panel.c, src/demo/hunter2.c, web/ui.js.

Checks: ./build.sh builds, ./test.sh runs unit tests. tests/headless/fake.mjs runs the real game with a fake WebGPU and fake DOM (see Machine notes in focus/engine.md for how to pick out bullets and planets and press buttons; the fake has no localStorage, define one in the script, and it does not compile shaders, so looks can only be judged by me). To load my save headless, have the fake localStorage return it for "hunter2.settings.hunter2". I check every change in Chrome: after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends (pkill -f "^node serve.js", a plain "node serve.js" pattern also kills the shell running pkill). Commit only when I ask. Deploys only when I ask (handover/deploy.md).

Keep focus/engine.md (and focus/game.md for game changes) up to date yourself. Then tell me you are ready and wait for my first instruction.
