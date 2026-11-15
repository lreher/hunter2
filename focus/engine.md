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

## Next
- Code work starts with the emitter (user, 2026-11-15): emitter types + emitters as in The model, the ship's nozzles become emitters, side and up/down get their own emitter type. Talk each piece through with the user before writing it (How we work here).
- To settle with the user at the start, not decided: where the engine code lives (folder, layer: base <- platform <- render <- demo <- game); struct and function names (ask with a small example, brain rule 10); how emitter types show on the panel (saves are keyed by slider name only, two types with the same slider names collide; the user's saved values must keep loading, bake them into code defaults before renaming anything, focus/game.md); the look changes from dropping the rate split and the main thrust share (open question below), the user should know before it lands.
- Then actuators and objects (order with the user).

## Machine notes
- Headless scripts on fake.mjs: draws and dispatches are never cleared, clear them per frame in long runs. hooks.write sees every buffer write: filter the particle frame uniform exactly (header 36 floats + 64 anchors x 8 + emitter_count x 24, emitter_count at u32 index 31), a loose length filter matched other buffers and ran out of memory. The ship's model instance is 16 floats with scale 1 (index 4) and green 0.87 (index 13). Scene uniform: eye at floats 20..22.

## Open questions
- Later maybe: real turning physics (forces off center turn the object, book rotation chapters). Would make every actuator a push and give its position a job, but changes the feel. Not now.
- Actuator x, y, z: what it does while objects have no turning physics (a force off center would turn a rigid body; the ship is a point mass, its turning is ship.c's own). Not discussed.
- A/D: one side actuator each way (two actuators) or one signed? Parked, it jumped ahead.
- Today two nozzles share one thruster's rate (split by nozzle count) and side sparks are slower because their share is measured against main thrust. In the model each emitter fires its type's own rate and the type sets its speeds. Mention when building (the look changes).
- Local emitters (anchors in render, sparks move with the emitter) are unused (focus/game.md open question). Could become an emitter type setting later.
- Name for the "elements" (later). "Object" is a name for now.
- Where the engine code lives (folder, layer) and how the panel shows parts: not discussed.

## Today's code (where the model will land)
- Loop: game_frame (src/game/game.c:165). Once per frame demo_update -> hunter2_update -> ship_update_aim. Fixed 1/60 s steps (up to 4) -> hunter2_step: ship_read_input (keys + aim -> ShipInput), ship_step (turning by quaternion_add_rotation, no phys; thrust eases per axis; forces into ship.particle; particle_integrate), ship_camera_step. Draw blended by alpha: ship_camera_view, hunter2_draw (grid, ship, ship_draw_exhaust -> render_emit per nozzle), render_end (particle compute pass, shadow, screen, particles last).
- game/ owns time, knows nothing about ships, calls demos through plain switches (demo.c).
- phys/: for the ship only a point mass pushed by forces (Particle, particle_add_force, particle_integrate). Contacts, force generators, springs unused in hunter2.
- ship.c: a file of globals (ship, ship_camera, ship_tuning, ship_exhaust), only one ship can exist. Its thrusters are SHIP_THRUSTER_* with ShipNozzle entries (focus/game.md code map).
- render/particles.c: RenderEmitter mixes the shared settings (rate, speed, cone, life, size, colors), the placement (position, direction, velocity, every frame) and its own memory (previous position, carry). ship_draw_exhaust copies ship_exhaust over every nozzle's emitter each frame, keeps the memory, writes the placement on top. That copy is where emitter types land.
- Panel: wired by hand, one plat_ui_slider per field into a tuning struct (ship_add_sliders). Saves are keyed by slider name only (web/ui.js), so two groups with the same slider names would collide: matters once there are two emitter types.

## Decisions
- "Force affector" is called actuator (user).
- Object, actuator, emitter type, emitter as in The model (user).
- Shared definition + separate instances, edits to the shared one reach every instance (user).
- Actuator types: push and turn. Steering and auto level are turn actuators (user).

## Log
- 2026-11-15: focus created. User wants the side and up/down thrusters to use a different emitter type, as the start of thinking about the code as a game engine. Claude's first plan (emitter types as handles, field tables, group save keys) rejected: too many layers at once, build it together.
- 2026-11-15: walked through how loop, physics, ship and particles work together (Today's code). User: too low level, think in components. User's first model: a physics object, "force affectors" that listen to keys, push it and trigger emitters, all entities with attributes.
- 2026-11-15: actuator picked. Inputs as data explained. Emitter replies: one invented "look" and was far too long (rejected), one nested emitters under actuators (wrong: the link is a wire, not a parent). Then explained as an editor UI: parts with input slots.
- 2026-11-15: user refined to The model: object with an actuator list, actuators defined on their own (two objects = two actuators from one shared definition, no types, edits reach all), emitters standalone and optionally attached, with their own input. Emitter talked through (type + instance, what it does each frame). User: "beautiful". Next: set the engine up.
- 2026-11-15: user asked how steering and auto level fit. Suggested push and turn actuators (open question updated).
- 2026-11-15: user agreed: actuator types push and turn, steering and auto level are turn actuators.
- 2026-11-15: focus readied for the code work, handover/engine.md written.
