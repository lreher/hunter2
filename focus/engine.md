# focus: engine

Goal: shape hunter2's code into a game engine made of parts (objects, actuators, emitters) that can be configured like a real game engine, defined together with the user step by step (user, 2026-11-15). This model informs how everything ties in: the game (focus/game.md) is built from these parts, the physics (focus/physics-engine.md, src/phys/) moves the objects, the code rules (focus/architecture.md: layers, naming, unity build) still apply.

## How we work here
- Build it up together, do not jump the gun (user). Talk a piece through before writing code, the user decides.
- No classes. The engine is mostly functional: plain data and functions over it (user, "fun challenge").
- Concept talk stays simple and short: plain words, a small tree or sketch at most. No formulas, numbers, code details, invented names or extra side points unless asked (user, after a reply full of those startled them).
- Never present a placeholder name as settled. Use the user's words; offer names as options (user rejected "look", Claude's made up name).
- Think like a game developer building it in an editor UI first, then how the code does the same (user).
- No abstraction layers ahead of need (user rejected a first plan with types, field tables and save key changes all at once).

## Long term (user)
- The game itself shows all its "elements" (name not picked) and lets you configure them like a real game engine: a game inside a game engine, playable at any time.
- Not now. It is why every part is data with attributes the panel (and later the in-game editor) can show and change.

## The model (agreed 2026-11-15, user: "a good starting point")
Pattern: a shared definition, and separate instances made from it. Each instance is its own thing in the engine (own state, own place). Editing the shared definition changes every instance made from it, like the panel sliders do now (user).

Object (name for now): a physics object.
- name ("ship" is just its name), shape type ("sphere"), mass, drag, size, etc, model (image, overlay mesh).
- a list of actuators that affect it, each attached at x, y, z relative to the object.

Actuator: applies a force to the object it is attached to.
- Defined on its own with a name (e.g. "engine"), not inline in the object, for organization (user).
- Actuator type (user, 2026-11-15): push (a force on the object) or turn (turns the object around one of its axes at up to a speed, with a response). Steering = pitch and yaw turn actuators with the mouse aim as input. Auto level = the roll turn actuator, input Q/E, falling back to the ship's tilt when they are up. Mouse aim and tilt are inputs the engine works out, like a key. Same numbers as ship.c today, so the feel stays.
- Two objects with "engine" get two actuators, separate in the engine (each its own ramp, input, position), made from the one "engine" (user).
```
actuator "engine"   type: push   force, push direction, response
actuator "pitch"    type: turn   around the right axis, speed, response

object "ship"
  shape: sphere   mass, drag, size, model
  actuators:
    "engine"   at x, y, z   input: W
    "engine"   at x, y, z   input: ...
```

Emitter: stands on its own (user).
- Emitter type (user's word): the shared particle settings. rate, speed, speed spread, cone, life, life spread, size, colors, plus how the sparks change with input strength (base speed, cone squeeze).
- Emitter: one placed instance. type, attached to an object (x, y, z and rotation relative to it) or on its own (static, in the world), and its own input (an object's actuator, a key, always on). It handles its own input or triggers.
- Every frame: read its input (0..1; from an actuator = how hard it pushes), work out where it is (attached: the object's position and rotation plus its offset, sparks also get the object's speed; on its own: its own spot), ask the GPU for sparks with its type's settings scaled by the input.
- Streak and particle drag stay shared by all sparks for now (the GPU applies them to every spark).
```
emitter type "main"
  rate, speed, speed spread, cone, life, life spread, size, colors
  base speed, cone squeeze

emitter "left engine"
  type: main
  attached to: ship   at x, y, z   rotation
  input: the ship's main engine actuator

emitter (on its own)
  type: ...   at x, y, z   rotation   input: always on
```

How listening works (inputs as data, used in every sketch since, mechanics confirmed when we build): nobody calls anybody. Each part has an input that points at a source (a key, a mouse axis, an actuator), and reads it when it runs: actuators every physics step, emitters every drawn frame. No callbacks (no function pointers rule, focus/architecture.md). The wiring is data, so the editor can show and change it. Code sketch shown to the user:
```c
typedef struct { i32 kind; i32 index; } Source; // SOURCE_KEY: index = KEY_W. SOURCE_ACTUATOR: index = which actuator
f32 source_read(Source source); // switch on kind: plat_key_down, or that actuator's strength now
```

Applied to the ship (target): object "ship" with push actuators (main engine, reverse, side, up/down) and turn actuators (pitch, yaw: mouse aim; roll: Q/E, else tilt = auto level); emitters attached to it. Main and forward emitters use one emitter type, side and up/down emitters another (the user's original ask).

## Folder plan (agreed 2026-11-15, user: "love it")
The whole code is rearchitected around the model (user: "the whole point"). Names are proposals until the user picks.
```
src/
  base/ phys/ platform/ render/   unchanged, know nothing about the parts
  engine/   NEW: the parts, plain data + functions over it, one public header
    inputs      sources a part reads: key, mouse aim, tilt, an actuator
    objects     physics objects + their attached actuators
    actuators   shared definitions (push, turn) + instances
    emitters    emitter types + placed emitters
    engine      holds all parts, runs a physics step and a drawn frame over them
  demo/     hunter2 becomes data (ship object, actuators, emitters); chase camera stays here for now; other demos unchanged
  game/     unchanged: loop calls the demo, the demo runs the engine
```
Layers: base <- platform <- render, phys <- engine <- demo <- game.
Each physics step: actuators read input, push or turn their object, physics moves it. Each drawn frame: objects draw, emitters read input, place themselves, send sparks.
ship.c empties piece by piece: flight -> actuators, nozzles -> emitters, ship -> object. Left at the end: data in hunter2.c + the camera.
Build order (each step works in Chrome before the next):
1. engine/ + emitter types and emitters; nozzles move over first, ship.c still flies.
2. Inputs as data (keys, mouse aim, tilt as sources).
3. Actuators (push, turn) take over thrust and steering.
4. Objects (ship becomes an object with its actuators).
5. Panel shows each part's settings from its data instead of hand wired sliders.
Before anything changes: the user pastes their saved settings (localStorage hunter2.settings.hunter2), Claude bakes them into code defaults. Look change from rate split / thrust share: deal with once the engine is worked out (user).

## Built (2026-11-15, user: "just do everything". Checked in Chrome, user: "feels similar enough")
The whole plan in one go: engine/ with inputs, objects, actuators, emitters and the engine loop; ship.c is gone, hunter2.c holds the ship as tables plus the camera, marks, hint, grid, HUD. Names are the code's for now, the user has not picked them (only EmitterType / Emitter: "sure for now").

Code map (src/engine/, one public header engine.h, unity build order input, object, actuator, emitter, engine):
- Source {kind, index, negative} + INPUT_* table macros (INPUT_KEY(KEY_W), INPUT_KEY_PAIR(KEY_E, KEY_Q), INPUT_AIM_PITCH, INPUT_AIM_YAW, INPUT_TILT, INPUT_ACTUATOR(i), INPUT_ALWAYS). source_read(engine, source, object): keys, aim and tilt read 0 while engine.inputs_on is 0 (free camera); actuator and always still read.
- input.c: InputTuning input_tuning (aim sensitivity 0.002, full turn angle 0.6, limit 1.0, invert 0, level angle 0.8, level strength 0.5; code only). object_update_aim (once per frame, old ship_update_aim). input_work_out (per step, per object): aim_pitch / aim_yaw from the aim with the pitch and yaw turn actuators' speeds (the ellipse), tilt for auto level.
- object.c: Object = name, start_position, mass, drag, drag_cut % + drag_cut_input (the drift), model (-1 none), size (draw scale), color, and state (particle, previous position, rotation, previous rotation, aim, aim_pitch, aim_yaw, tilt). object_start, object_axis, object_drawn_position / rotation. No shape yet (nothing uses it).
- actuator.c: ActuatorType = name, kind (ACTUATOR_PUSH / ACTUATOR_TURN; "kind" because type means the shared definition), force N, boost %, cutoff %, speed rad/s, response /s. Actuator = type, object, direction (push: the way it pushes; turn: the axis, input 1 turns right hand around it), input, fallback (read when input is 0), block (asks nothing while on), boost (push: adds the type's boost while on and input > 0), now (N or rad/s). actuator_ease: turn eases to input * speed; push eases to input * force (* boost), never below cutoff (lights at it, goes out under it). actuator_strength = now / full (past 1 with boost): what emitters read.
- emitter.c: as step 1, plus Emitter.object (-1 = on its own, world position) and Emitter.input.
- engine.c: Engine = pointers + counts of the game's tables, inputs_on. engine_start. engine_step (FORCE_INLINE): previous state + input_work_out per object, actuator_ease for all, then per object: turn actuators sum into a turn (own axes) -> quaternion_add_rotation, push actuators add forces along the new axes, drag (cut while drag_cut_input), mass, particle_integrate. engine_draw: models, then every emitter (input clamped at 0). engine_add_sliders: a group per object (drag, drag cut %), per actuator type (push: force N, boost % if any instance has a boost input, response /s, cutoff %; turn: speed rad/s, response /s), "particles" (max particles, streak, particle drag), per emitter type.
- Push direction and turn axis sit on the instance, not the type (the sketch had them on the type): side thrust and up/down thrust are one type each, used by two actuators pointing opposite ways, so one slider covers both like before.
- Actuator x, y, z left out: nothing uses it until turning physics (open question).

hunter2.c tables (defaults = user's save 2026-11-15):
- Object "ship": mass 1000, drag 0.5, drag cut 80 % while right mouse (drift).
- Actuator types: engine (push 70000, boost 150 %), reverse (45000), side thrust (34500), up/down thrust (30000), all cutoff 10 %, response 4; pitch 2.2, yaw 2.3, roll 3.2 rad/s, response 11.8.
- Actuators: engine W (+ boost Space), reverse S, right D, left A, up Shift, down Control, all blocked by right mouse (drift). Pitch: aim up/down, axis +x. Yaw: aim left/right, axis -y. Roll: E/Q pair, fallback tilt (auto level), axis -z.
- Emitter types main (rate 5000, main engine + reverse) and side (rate 5000, side + up/down). 8 emitters (same spots as before), input = their actuator.
- Camera (Hunter2CameraTuning, panel group camera), boosting = Space + W, not S, no drift, not free camera. Marks, hint, grid, HUD (thrust N = engine now - reverse now).

Checks: headless scripted flight (mouse aim, W, boost, side, up, roll, reverse, drift, A + Control) gives the same position and rotation as the old code to 3 decimals every 30 frames. Tests pass, keys.mjs and sim.mjs fine.

Look changes (the user deferred this, "deal with it once the engine is worked out"): each emitter's input is now its own actuator's strength (now / its full force), not the share of the main engine. Main engine unchanged. Reverse at full now fires like the main engine at full (was about two thirds). Side and up/down: side type rate set to 5000 so at full they fire about what they did (they were about half of main); speed and cone very slightly different.

Panel: groups ship, engine, reverse, side thrust, up/down thrust, pitch, yaw, roll, particles, main, side, camera, debug, save/load. It wraps into rows now (web/ui.js, right aligned, scrolls if needed). Old saves: "drag /s" and "boost %" load by bare name; the other flight values were shared sliders and are now per type (defaults = the user's save).

Small behavior differences (only when keys fight): W + S together now pushes with engine minus reverse (was nothing). Switching W to S: the two thrusters ramp separately (net force the same, cutoff snaps differ slightly).

## Next: the engine / game UI (user, 2026-11-15: the panel "looks gosh darn awful", make it less cluttered)
- Today: web/ui.js draws one HTML panel over the canvas, top right, a flex row of groups that wraps (14 groups: ship, engine, reverse, side thrust, up/down thrust, pitch, yaw, roll, particles, main, side, camera, debug, save/load). Each group is a column of label, slider, value; a click on its title folds it (remembered per demo). C declares controls one by one (plat_ui_group, plat_ui_slider, plat_ui_slider_log, plat_ui_choice); engine_add_sliders builds the groups from the parts' data. Saves: localStorage hunter2.settings.<demo>, keyed "group/name", older saves fall back to the bare name.
- Goal: look at how real engines show this (Unity, Unreal, Godot: a list or tree of parts, an inspector for the selected one, shared definitions as assets/resources) and work out with the user how the engine's parts should show: objects with their actuators and emitters under them, shared types edited in one place, inputs shown as wiring. Long term the game shows its own parts in an in-game editor (Long term above), so this is its first shape.
- Talk it through first (How we work here): editor UI first, then code. Small sketches, the user decides. No game design pitches.
- Before removing or renaming any panel control or group: the user's saved values go into code defaults first (ask them to paste copy(localStorage.getItem("hunter2.settings.hunter2")) from the Chrome console).
- Other open points: names (actuator types, emitter types "main"/"side", Source / INPUT_*, Engine), reverse sparks now as strong as main at full (own emitter type?), nothing committed yet.

## Machine notes
- Headless scripts on fake.mjs: draws and dispatches are never cleared, clear them per frame in long runs. hooks.write sees every buffer write: filter the particle frame uniform exactly (header 36 floats + 64 anchors x 8 + emitter_count x 24, emitter_count at u32 index 31), a loose length filter matched other buffers and ran out of memory. The ship's model instance is 16 floats with scale 1 (index 4) and green 0.87 (index 13). Scene uniform: eye at floats 20..22.

## Open questions
- Later maybe: real turning physics (forces off center turn the object, book rotation chapters). Would make every actuator a push and give its position a job, but changes the feel. Not now.
- Actuator x, y, z: what it does while objects have no turning physics (a force off center would turn a rigid body; the ship is a point mass, its turning is ship.c's own). Not discussed.
- A/D: built as two actuators (right, left) from one type "side thrust". Same for W/S (engine, reverse) and up/down.
- Today two nozzles share one thruster's rate (split by nozzle count) and side sparks are slower because their share is measured against main thrust. In the model each emitter fires its type's own rate and the type sets its speeds. Mention when building (the look changes).
- Local emitters (anchors in render, sparks move with the emitter) are unused (focus/game.md open question). Could become an emitter type setting later.
- Name for the "elements" (later). "Object" is a name for now.

## Code before the engine (for history)
- ship.c held the ship as globals (ship, ship_tuning, ship_camera, ship_exhaust) with ShipInput read from keys and aim, ship_step doing turning and thrust per axis, ship_draw_exhaust copying one look over every nozzle. All of it now lives in engine/ and hunter2.c (Built above).

## Decisions
- "Force affector" is called actuator (user).
- Object, actuator, emitter type, emitter as in The model (user).
- Shared definition + separate instances, edits to the shared one reach every instance (user).
- Actuator types: push and turn. Steering and auto level are turn actuators (user).
- Folder plan and build order (user). Built all at once (user: "just do everything, the plan is decided").
- Panel saves keyed group/name, old saves still load by bare name (user agreed to copying saved values first).

## Log
- 2026-11-15: focus created. User wants the side and up/down thrusters to use a different emitter type, as the start of thinking about the code as a game engine. Claude's first plan (emitter types as handles, field tables, group save keys) rejected: too many layers at once, build it together.
- 2026-11-15: walked through how loop, physics, ship and particles work together (Today's code). User: too low level, think in components. User's first model: a physics object, "force affectors" that listen to keys, push it and trigger emitters, all entities with attributes.
- 2026-11-15: actuator picked. Inputs as data explained. Emitter replies: one invented "look" and was far too long (rejected), one nested emitters under actuators (wrong: the link is a wire, not a parent). Then explained as an editor UI: parts with input slots.
- 2026-11-15: user refined to The model: object with an actuator list, actuators defined on their own (two objects = two actuators from one shared definition, no types, edits reach all), emitters standalone and optionally attached, with their own input. Emitter talked through (type + instance, what it does each frame). User: "beautiful". Next: set the engine up.
- 2026-11-15: user asked how steering and auto level fit. Suggested push and turn actuators (open question updated).
- 2026-11-15: user agreed: actuator types push and turn, steering and auto level are turn actuators.
- 2026-11-15: focus readied for the code work, handover/engine.md written.
- 2026-11-15 (session 2): folder plan agreed (engine/ between phys/render and demo/), build order 1-5. User wants a copy of saved settings baked in before the rearchitecture.
- 2026-11-15 (session 2): user's save baked into code defaults (changed: pull_back 8.1, height 3.3, exhaust rate 10000, speed 28.5, stretch 0). Full save as pasted:
  {"thrust N":70000,"boost %":150,"reverse N":45000,"side N":34500,"up/down N":30000,"drag /s":0.5,"pitch rad/s":2.2,"yaw rad/s":2.3,"roll rad/s":3.2,"turn response /s":11.8,"thrust response /s":4,"engine cutoff %":10,"drift drag cut %":80,"distance m":6,"max distance m":10,"boost max distance m":16,"pull back m per 100 m/s":8.1,"pull response /s":2,"height m":3.3,"look ahead m":7.9,"follow /s":5,"max particles":262144,"rate /s":10000,"life s":1,"life spread s":1.55,"speed m/s":28.5,"base speed m/s":30,"speed spread m/s":5,"cone rad":0.15,"cone squeeze":0.3,"size m":0.05,"streak s":0,"particle drag /s":1,"grid":1}
- Step 1 talk: type vs emitter split re-explained, user agreed. Proposed parts as plain tables (like ship_nozzles) instead of add functions; names EmitterType / Emitter pending.
- 2026-11-15 (session 2): step 1 (emitters) built, user: "looks fine I think". Then user: "just do everything". Built inputs, objects, actuators, engine loop, panel from data; ship.c removed. Same flight headless, look change on reverse (and slightly side).
- 2026-11-15 (session 2, end): user checked the engine in Chrome ("feels similar enough"). Next: fix the cluttered panel, look at other engines for how to show the parts. handover/engine.md rewritten for it.
