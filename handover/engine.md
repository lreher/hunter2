Read brain.md first.

Project: hunter2 (this repo). A browser game in C, compiled to wasm, rendered with WebGPU through our own thin JS bridge. Physics from scratch (src/phys/). The game (src/demo/hunter2.c) is a flyable ship with GPU particle exhaust.

Stage: the engine is built (src/engine/, focus/engine.md "Built"). The game is made of parts written as plain tables in hunter2.c: an object (the ship), actuator types and actuators (push: engine, reverse, side, up/down; turn: pitch, yaw, roll with auto level), emitter types and emitters (one per nozzle, each reading its actuator), inputs as data (each part has a Source it reads when it runs). The engine builds the debug panel from the parts' data. The user checked it in Chrome: it flies the same as before. Nothing is committed yet.

Task now: the engine / game UI. The panel (web/ui.js, one HTML overlay, top right) now has 14 groups in wrapping rows and looks cluttered ("gosh darn awful"). Look at how real game engines show this kind of thing (Unity, Unreal, Godot: hierarchy or tree of parts, an inspector for the selected one, shared definitions as assets or resources, how inputs and wiring are shown) and work out with me how our parts should be visualized: objects with their actuators and emitters, shared types edited in one place, inputs as wiring. Long term the game shows all its parts and lets me configure them like a real engine, so this is the first shape of that editor.

How we work on this: build it up together, do not jump the gun. Talk it through first, editor UI first, then code. Small sketches (ASCII mockups are fine), I decide. Keep concept talk simple and short, no invented names presented as settled. No classes, mostly functional. I own the game design: do not pitch game ideas. Important: before removing or renaming any panel control or group, get my saved settings (I paste copy(localStorage.getItem("hunter2.settings.hunter2")) from the Chrome console) and put them into the code defaults first.

Setup: run ./setup.sh first (checks tools, only does what is missing; ask before installing anything).

Then read:
- focus/engine.md: all of it, especially "Built" (code map, panel, saves) and "Next".
- focus/game.md: State and Decisions (the panel decisions: grouped columns that fold, only feel numbers, log sliders for max particles and rate, save and load, no reset).
- focus/architecture.md, Decisions: layers (base <- platform <- render <- engine <- demo <- game), JS split by browser feature, naming (no shorthand).
- web/ui.js (the panel), src/platform/platform.h (plat_ui_*), src/engine/engine.c (engine_add_sliders), src/demo/hunter2.c as needed.

Checks: ./build.sh builds, ./test.sh runs unit tests. tests/headless/fake.mjs runs the real game with a fake WebGPU and fake DOM (sliders, tabs; see Machine notes in focus/engine.md). I check every change in Chrome: after every change, build and restart the server yourself (npm start, http://localhost:8000) so I only reload. Stop it when the session ends. Commit only when I ask. Deploys only when I ask (handover/deploy.md).

Keep focus/engine.md (and focus/game.md for game changes) up to date yourself. Then tell me you are ready and wait for my first instruction.
