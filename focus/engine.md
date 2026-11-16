# focus: engine

Goal: shape hunter2's code into a game engine made of parts (objects, actuators, emitters) that can be configured like a real game engine, defined together with the user step by step (user, 2026-11-15). This model informs how everything ties in: the game (focus/game.md) is built from these parts, the physics (focus/physics-engine.md, src/phys/) moves the objects, the code rules (focus/architecture.md: layers, naming, unity build) still apply.

## How we work here
- Build it up together, do not jump the gun (user). Talk a piece through before writing code, the user decides.
- No classes. The engine is mostly functional: plain data and functions over it (user, "fun challenge").
- Concept talk stays simple and short: plain words, a small tree or sketch at most. No formulas, numbers, code details, invented names or extra side points unless asked (user, after a reply full of those startled them).
- Never present a placeholder name as settled. Use the user's words; offer names as options (user rejected "look", Claude's made up name).
- Think like a game developer building it in an editor UI first, then how the code does the same (user).
- No abstraction layers ahead of need (user rejected a first plan with types, field tables and save key changes all at once).
- Sketches show only choices that make sense in the game: a type list that offered "reverse" for the right thruster confused the user and led to a long back and forth. Use the names the user knows ("forward thrusters"), say plainly which part a thing sits on.

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
- engine.c: Engine = pointers + counts of the game's tables, inputs_on. engine_start. engine_step (FORCE_INLINE): previous state + input_work_out per object, actuator_ease for all, then per object: turn actuators sum into a turn (own axes) -> quaternion_add_rotation, push actuators add forces along the new axes, drag (cut while drag_cut_input), mass, particle_integrate. engine_draw: models, then every emitter (input clamped at 0). engine_add_sliders (gone, replaced by engine/panel.c, see Panel): a group per object (drag, drag cut %), per actuator type (push: force N, boost % if any instance has a boost input, response /s, cutoff %; turn: speed rad/s, response /s), "particles" (max particles, streak, particle drag), per emitter type.
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

Panel (old, replaced in session 3, see Panel): groups ship, engine, reverse, side thrust, up/down thrust, pitch, yaw, roll, particles, main, side, camera, debug, save/load. It wrapped into rows (web/ui.js, right aligned, scrolls if needed). Old saves: "drag /s" and "boost %" load by bare name; the other flight values were shared sliders and are now per type (defaults = the user's save).

Small behavior differences (only when keys fight): W + S together now pushes with engine minus reverse (was nothing). Switching W to S: the two thrusters ramp separately (net force the same, cutoff snaps differ slightly).

## Panel (built 2026-11-15, session 3)
The first shape of the in-game editor. Asked for: the old panel (14 groups in wrapping rows, top right) "looks gosh darn awful". Looked at how engines do it (Unity, Godot: a tree of parts, shared definitions kept in one place) and worked it out with the user step by step.

What the user sees (web/ui.js):
- A side panel on the right edge, full height above the demo bar, 340px. Hidden while the mouse is locked (flying), shown when Esc lets the mouse go. Other demos never lock the mouse, so they always show it (their groups are top folds now).
- A tree of folds, all folded at first. Open folds remembered per demo (localStorage hunter2.open.<demo>, {path: true}, path = "ship/actuators"). Save and load at the bottom. Scroll kept when C builds the panel again.
```
ship                      drag /s, drag cut %
  actuators
    right   side thrust <- D          opens: "shared with left" + split, or "not shared"
  emitters
    reverse left   side <- reverse    opens: type  main [side], shared with ..., split
actuators                 the shared ones: engine push, ..., pitch turn  (copy, x, double click renames)
  + new  (push / turn)
emitters                  main, side, ...  (copy, x, double click renames)
  + new
camera
debug
```
- Rows on the ship show name, the shared type it uses, and its input ("engine  engine <- W, boost Space", "roll  roll <- E/Q, else tilt"). Inputs are display only (editable later, user). The drift block (right mouse) is not shown in the notes.
- split = a copy of the type ("side thrust 2") only this part uses. x only shows when nothing uses the type. Only emitters pick a type: for actuators a list with "reverse" etc. for "right" made no sense to the user, split is enough.
- Every slider value is also a text field (light box): click, type, Enter or click away sets it, Esc or junk puts the old value back. Typed values are not held to the slider's range. More digits shown when a typed value needs them.
- Blocks (user, 2026-11-15, "much nicer"): a part's settings in small square groups with a title, side by side when there is room, stacked when not, light crossed lines between them (plat_ui_block / plat_ui_block_end; ui.js: blocks right after each other share one grid, auto-fill columns of at least 250px; each block draws its right and bottom edge, the grid its top and left). Emitter types: rate | life | speed (with drag) | cone | size (with streak) | color. Panel 600px wide for it. Labels unchanged so save keys stay ("main/speed spread m/s"); shorter labels inside blocks ("spread") need the user's save pasted first. Camera split (distance | position | follow) offered, not decided.
- Emitter types have color pickers "color at start" and "color at end" (a spark fades from one to the other over its life), saved as "#rrggbb" (8 bits per channel, so a saved color comes back rounded to 1/255).
- Streak and drag are per emitter type now (were one "particles" group for all sparks). Max particles is code only (render_particle_tuning.capacity 262144 = the user's save).
- Everything (numbers, colors, new / copied / renamed / split types, picks) is kept only by Save, load goes back to the last save.

Code:
- engine/panel.c: engine_add_panel builds the tree, engine_update_panel(engine) does what a click asked for and returns 1 when the panel must be built again. A click writes PANEL_ACTION(what, index) + option into engine->panel_action (new, copy, delete, rename, split, pick). hunter2_update calls engine_update_panel every frame and on 1 does plat_ui_clear + hunter2_add_panel.
- Parts list: a text saved under the key "parts", by name ("actuator push engine", "emitter main", "actuator use right=side thrust", "emitter use reverse left=side"). Applying it rebuilds the type tables in its order, a type whose name is still there keeps its numbers, a new name gets starting numbers (panel_new_actuator_type / panel_new_emitter_type), then each part takes its type by name.
- Type names are char[ENGINE_NAME_SIZE] (32), typed names lose | = and line breaks. Type tables have ENGINE_MAX_ACTUATOR_TYPES / ENGINE_MAX_EMITTER_TYPES (16) places, hunter2.c sizes its arrays to them. Actuator.name and Emitter.name (instances) are set in hunter2.c: actuators engine, reverse, right, left, up, down, pitch, yaw, roll; emitters engine left/right, reverse left/right, left wing, right wing, keel, top ridge (names for now, the user can rename).
- source_describe(engine, source, text, capacity): an input in words. plat_key_name(key) in platform/input.c.
- Platform UI calls (platform.h): plat_ui_group (a top fold), plat_ui_fold / plat_ui_fold_end (a fold in the open one, with a dim note), plat_ui_row, plat_ui_button (options "push|turn" show on click), plat_ui_pick (like choice, not saved, writes value + i), plat_ui_fold_button (small word on the fold's line), plat_ui_fold_rename (double click), plat_ui_text (hidden saved text), plat_ui_color, plus slider, slider_log, choice, image as before.
- Saves (localStorage hunter2.settings.<demo>): keys are the innermost fold name + control name ("engine/force N", "main/streak s", "camera/height m", "parts"), the same as before for every old slider. A control gets its saved value only the first time its key shows up for a demo (the seen set in ui.js), so a panel built again after a click keeps the values C has now; load empties the set; a demo switch starts fresh.
- Particles: RenderEmitter has type, drag, stretch. render/particles.c keeps a 16 entry per type table in the frame uniform (after the 36 float header, before the anchors: keep share, stretch). Each spark's anchor field is anchor_and_type (anchor low 8 bits, type above), so a spark stays 48 bytes and gets its type's drag and streak all its life (move and draw shaders, render/shaders.c).

Checked: headless every step (tree, save keys, typed values, pick, split, new, delete, copy, rename, save, load, restart from a save, colors reach the sparks). In Chrome: per type streak/drag ("looks fine"), the side panel seen. Blocks seen in Chrome (user: "amazing"). Not yet checked in Chrome by the user: parts editing (pick, split, new, copy, rename, delete) and colors.

## Bullets (talked through and built 2026-11-15, session 4, not yet checked in Chrome)
User: shoot bullets with the mouse, one type for now ("machine gun"), real physics objects, two under the wings like a jet. More bullet types later.
- No "guns" on the ship (user: binds the engine to this game). A gun is an emitter whose type emits objects instead of sparks.
- Agreed sketch (user: "amaazing"):
```
objects
  ship                [ mass ] [ drag ] [ model ]
    actuators
    emitters
      ...
      under left wing    machine gun <- left mouse
      under right wing   machine gun <- left mouse
  bullet              [ mass ] [ drag ] [ life ] [ model: size, color ]
actuators
emitters
  main           emits: sparks
  side           emits: sparks
  machine gun    emits: bullet    [ rate ] [ speed ] [ cone ]
```
- Objects get their own entries under an "objects" heading, ship included (user: "their own entry just like ship"). The emitter type picks the object it emits, rate / speed / cone stay in the emitter type, the object holds mass, size, life, color.
- A bullet starts with the ship's velocity plus the emitter's speed, is gone when its life runs out. Nothing to hit yet.
- Look: a small ball built in code, size and color configurable under the object's "model" (user).
- No recoil (user).
- Ship gets the same model block (size, color) as the bullet (user: "yes get it").
- Later maybe (user): a rendered object (just the drawing) separate from a physics object, attachable to an emitter or to a physics object directly. Not now.
- The two emitters fire together (asked, not answered: together is what got built).

Built:
- User's save (session 4) baked into code defaults first: emitter types main, side and the user's own "reverse" (reverse left / right use it). No color keys were in the save (the user did not change colors, or they were not saved: not looked into).
- Object: emitted_only (not in the world, not moved or drawn, only its copies), lifetime. EmitterType: emits (EMITS_SPARKS / EMITS_OBJECTS) and object. Emitter: owed. EmittedObject (code name for a copy in flight: object, life_left, particle, previous_position, rotation), a pool the game hands the engine (hunter2: 1024 places, full = no new ones).
- engine_step: after the objects, copies run their life down and move (Particle, damping from the object's drag), then emitter_send_objects (emitter.c) for every emitter whose type emits objects: input <= 0 makes it ready (first copy leaves the moment the button goes down), then rate * input per second. A copy starts at the emitter's spot with the object's velocity + speed (give or take speed spread) along the emitter's direction, up to cone off it; its previous position is the emitter's spot a step ago so it is drawn leaving the emitter. No recoil.
- engine_draw: copies drawn with their object's model, size, color; emitters of objects skip sparks.
- KEY_MOUSE_LEFT ("left mouse"): only while the mouse is locked (web/input.js), so the click that locks never fires; let go on Esc.
- hunter2: object "bullet" (mass 0.1 kg, drag 0, life 2 s, size 0.25 m, color 1, 0.8, 0.35, a ball: octahedron cut into 128 faces, 1 m across at size 1), emitter type "machine gun" (rate 15 per emitter, speed 400 m/s, spread 0, cone 0.003 rad), emitters "under left wing" / "under right wing" at (-+0.45, -0.1, 0.2) firing along the nose, input left mouse.
- Panel: "objects" heading, each object a fold with blocks mass (log slider "mass kg") | drag | life (emitted_only only) | model ("size m", "color"); actuators / emitters folds only when it has some. Emitter type fold note "emits: sparks" / "emits: bullet", a pick "emits" (sparks, then every emitted_only object); a type that emits objects shows only rate (linear 0.5..100) | speed (speed, speed spread) | cone. Save keys unchanged for old controls ("ship/drag /s"); new ones "ship/mass kg", "ship/size m", "ship/color", "bullet/...". Open folds paths changed (objects/ship/...), so folds start closed once.
- Parts list: "emitter machine gun=bullet" for a type that emits an object, plain "emitter main" = sparks. An old list without the type keeps it (a part uses it).
- Glow (user asked "can they glow?", picked A of: A bright solid, B A plus a soft halo like the sparks): Object.glow (panel: "glow" Off|On in the model block, bullet on). render_draw_model takes glow (0..1), carried in MeshInstance's 8th float (was pad1); the mesh shader mixes the lit color toward the plain color by it. The ball still casts a sun shadow. B (halo) not built.
- A looked like a flat dot, not a glow (user). Halo (B) built: Object.halo (m, panel "halo m" in the model block, always shown, drawn only while glow is on; bullet 1 m). render_draw_glow(center, radius, color) in render/mesh.c: a camera facing square per glow, additive, depth tested not written, (1 - d^2)^2 fade like the sparks, drawn last in the screen pass (render_shader_glow). Up to 1024 a frame.
- Render draws at most 256 instances per model (MESH_MAX_MODEL_INSTANCES): more bullets than that in flight are not drawn.
- Checked headless: no fire before the lock, holding fires 2 per step at first then 15/s each (30 after 1 s), all gone 2 s after letting go, the same with the user's saved parts list loaded.

## Universe and planets (talked through 2026-11-15, session 4. Part 1 built, not yet checked in Chrome)
User: bring Gravity Cloud in: planets of random size and velocity flying around, interacting, at a large scale.
- Agreed sketch (user: "amazing", "sounds good"):
```
universe
  gravity     [ strength ]  [ bounce ]          everything pulls on everything, hits bounce
  planets     places: planet   [ count 150 ] [ spread 5 km ] [ size 20..200 m ] [ speed 0..20 m/s ]   again
objects
  ship / bullet / planet [ density ] [ model ]   mass = density x size
```
- "universe" heading (user renamed "world"), placing things goes under it with gravity (user). Gravity on everything for now: ship and bullets too (user). Hits: planets bounce off each other, bullets hit planets with real force, so does the ship (user). Scale: try 20..200 m planets over 5 km (user: "lets try that and see how it goes").
- Build in parts, user checks each: 1 universe + placing (planets drift), 2 gravity, 3 hits, 4 tune strength for the scale.

Part 1 built:
- Copies generalized: EmittedObject is now ObjectCopy (used, object, placing (-1 = from an emitter), life_left, size, particle, previous_position, rotation). engine_add_copy(engine, object) finds a free place. Object.emitted_only is now copies_only; lifetime 0 = forever; density > 0 = a copy's mass is density x the volume of a ball size across (set every step, so the slider acts right away).
- Placing (engine.h, engine/placing.c): name, object, center, count, spread (m across), size_min/max, speed_min/max. placing_place takes its copies away and puts count down: random spot in the ball (100 tries, no overlap with its copies, 50 m clear of objects in the world), random size, random speed in a random direction. engine_start places every placing.
- Panel: "universe" first, a fold per placing ("planets", note "places: planet", "again" on its line), blocks count (0..500) | spread m | size min/max m | speed min/max m/s. Objects: density block (log, "density kg/m³") instead of mass when density > 0; a placed object has no drag, life or size slider (the placing picks sizes).
- hunter2: object "planet" (density 1000, color 0.55, 0.5, 0.45, a ball cut 16 times = 2048 faces), placing "planets" 150, 5000 m, 20..200 m, 0..20 m/s. Copy places 2048. Render: 1024 instances per model (was 256). demo_far (demo.h, default 400 m, reset per demo init): hunter2 draws to 20 km, near plane 0.1 m then.
- Checked headless: 150 planets, sizes 20..200, nearest edge 405 m from the ship start, farthest 2.5 km, each drifts at its own speed. Bullets unchanged.

Part 1 checked in Chrome (user: "looks sick").

Part 2 built (gravity, not yet checked in Chrome):
- Engine.gravity: real G (6.674e-11) times this, 0 = none. engine_add_gravity (engine.c) every step after the actuators: every object in the world and every used copy (bullets too) pull on each other, all pairs, force on both. Closer than their two radii counts as touching (no blow up while hits are not built). Object.radius: its ball in the world (ship 0.9 m), copies use half their size.
- Panel: universe / "gravity" fold (note "everything pulls on everything"), block strength, log slider "strength × real G" 1..10^7. hunter2 starts at 100000: a 200 m planet pulls the ship 2.8 m/s^2 at its surface, two 100 m planets 500 m apart 0.001 m/s^2 (minutes to close in). Part 4 tunes it.
- Headless: 600 steps fast (52 ms in node), planets still drift mostly at their own speeds after 10 s, the ship pulled a little.

- In Chrome: gravity felt weak or missing on the ship and bullets (user). It works, just weak next to thrust (70 m/s^2 vs 2.8 at a big planet's surface, 0.03 at 1 km). The user turned strength up ("increasing g helped"); their value is only in their save so far.
- "planet dials don't do anything" (user): count, spread, sizes, speeds only acted on "again". Now placing_update (called from engine_update_panel every frame) places again as soon as one of them changes (Placing.placed_with keeps the numbers it last placed with). Color and density always acted right away (checked headless).
- Offered, not picked: the pull on the ship in the HUD line.

- Planet dials checked in Chrome (user: "works now").

Part 3 built (hits, not yet checked in Chrome):
- engine_step reordered (engine.c): copies whose life is up go first, actuators ease, engine_gather_bodies (every world object and used copy, ids: object place, then object_count + copy place, so contacts come out sorted for the warm start), gravity on that list, turn / push / drag, particle_apply_forces on all, engine_move_with_hits (as Gravity Cloud: speculative sphere contacts for every pair that could touch this step, warm start, 4 solver passes, move, bounce), then emitters send out new copies.
- A copy never hits the object whose emitter sent it (ObjectCopy.from): bullets start inside the ship's 0.9 m ball. Bullets bounce off planets (they are not taken away on a hit).
- Engine.bounce, panel: universe / gravity fold (note "everything pulls on everything, hits bounce"), block bounce. hunter2 0.3 (as Gravity Cloud). Up to 8192 contacts a step.
- Headless: strength 10^7, spread 1500: planets clump, 460 pairs touching after 60 s, worst overlap 0.09 m, no NaN, 3600 steps in 0.6 s in node. Bullets fired into a dense field ended up to 177 m off the firing line (bounced).

## Quality of life (session 5, 2026-11-15)
- Auto level felt off in space (user: "auto rotate"). Offered A no auto level, B a panel choice; user: "lets try both". Built B starting on nothing: Actuator.fallback_on (0 = input 0 asks for nothing, 1 = reads fallback). Panel: roll fold, choice "when idle" nothing | level (shown on any actuator with a fallback, options "nothing|" + the fallback's name). The tilt source's name is now "level" (was "tilt"); the fold note no longer says ", else tilt". hunter2 roll starts on nothing. Checked in Chrome.
- Headless: E held 0.5 s rolls 1.33 rad, after letting go it eases out 0.27 rad more and stays rolled.
- User: Q/E should set the new level. Built Object.level_up (the object's own up while Q/E drive the roll, tilt worked out against it). In Chrome: "completely bugged out the camera and the controls feel super off" (user, did not say which "when idle" setting). Taken out again on request ("go back to what it was before the last change"), cause not looked into.

- "Camera no longer zooms out with thrust" (user): not a code change. The user's save has ship mass 100000 kg (code 1000): 70000 N gives 0.7 m/s^2, 3.7 m/s after 8 s of W, so the speed based pull back barely moves (6.9 to 7.1 m). Code defaults reach 136 m/s and 10.5 m. User's gravity strength in that save: 23714 (not yet in code defaults, the user said to only update what could be the issue).
- Up/down thrusters (user: two from the middle of the wings, picked B of A both on top / B down on top, up underneath): emitters "keel" and "top ridge" replaced by "left wing bottom", "right wing bottom" (up, sparks down, y -0.08) and "left wing top", "right wing top" (down, sparks up, y 0.12), all at x +-0.45, z 0.3 (just behind the machine guns), type side. Names were Claude's offer, not confirmed. An old parts list naming keel / top ridge is skipped line by line. Headless: Shift and Control each fire two. Checked in Chrome (user: "great").
- Planets looked polygony (user: "the balls in the other demos look muuuch nicer"). Now drawn with the renderer's sphere (render_draw_sphere: smooth normals, level of detail by size on screen up to 5120 triangles, contact shadows). Object.model = OBJECT_MODEL_BALL (engine.h) picks it, engine_draw_model draws either. No panel pick (user: "just use the nice one no picker"). Balls have no glow or halo, so the planet's glow / halo controls do nothing now. Bullets keep their built ball (4 cuts, 128 faces). The 16 cut planet model is gone. Headless: 150 spheres, radius 10..99.8 m. Not yet checked in Chrome.
- Also found, not changed: sun shadows only cover 3 m around the ship (hunter2_draw), anything outside reads the shadow map's clamped edge. User: it is mostly the polygons.

## Explosions on hit (session 5, talked through and built, not yet checked in Chrome)
User: bullets that hit a planet explode (after their push lands, a tick or so) into a once off emitter, a beautiful explosion with the thruster's particle look. Agreed sketch (user: "AWESOME"): object block "on hit" [ burst: explosion ] [ gone after: 1 step ]; emitter type "explosion" emits sparks, fires once. Sparks move with: user said "A and C" (what it hits / nothing), built as a choice. Bursts on any object (user).
Built:
- EmitterType.burst (> 0: fires that many sparks once, not a rate). Panel: the emits pick has "sparks once" (sparks | sparks once | bullet | planet); a burst type shows "sparks" (log 10..100000) instead of rate, no base speed or cone squeeze (input is always 1). Parts list: "emitter explosion=sparks once".
- Object (copies_only): hit_burst (0 nothing, k = emitter type k - 1), hit_steps, hit_moves_with (0 what it hits, 1 nothing). Panel block "on hit": pick "burst" (nothing | every emitter type), slider "gone after steps" 1..30, choice "moves with" what it hits | nothing. Parts list: "object hit bullet=explosion" (the pick is not a saved control, so it goes there). Types an object bursts with are kept by a parts list load and count as users (no delete while used).
- engine.c: after the bounce, every contact the solver pushed (impulse + fix_impulse > 0) marks a copy whose object bursts: hit_left = hit_steps, hit_body = the other. At the start of a step a copy whose hit_left reaches 0 is gone and queues a burst (engine_bursts, 256) at its spot with the hit body's velocity now (or none). engine_draw fires queued bursts after the spark emitters (emitter_burst -> render_emit with burst); render takes 16 emitters a frame, render_emit now returns 0 when a burst does not fit and it waits for the next frame.
- hunter2: "explosion" 2000 sparks, speed 15 +-10 m/s, cone 3.14, life 1 +-0.8 s, size 0.08 m, main's colors, drag 1.5. Bullet: burst explosion (set in hunter2_init), 1 step, moves with what it hits.
- Headless (spread 300 m, left mouse 4 s): 75 bursts of 2000 sparks.
- Process note: the user tested before hunter2 had the explosion type ("its not exploding") and said "you keep stopping": finish a change through build, check and server restart in one go before replying.
- "Not exploding" in Chrome was the user's save: a Save during a half built moment stored "object hit bullet=" (nothing), which loads over the code default. Fixed by the user picking explosion again. build.sh writes web/game.wasm that the running server serves, so a reload picks up half finished builds: avoid building mid change where a Save could catch it.

## Loading screen (session 5, user picked B: names the step)
- index.html: #loading over everything (black, "hunter2" in Tiny5, the step under it). index.js: step(name) logs the last step's time ("[js] load <step>: N ms"), shows the name, waits for a paint (rAF then setTimeout) before going on. Steps: starting WebGPU, loading game file, building the world (game_init), drawing the first frame; removed after the first frame.
- fake.mjs: rAF while start() runs (not the loop's "frame") runs at once; getElementById gives an element for ids other than "screen".
- Headless (user's save): world 9 ms, so placing is not what is slow; Chrome's console times will say which step is. Not yet checked in Chrome.

## Defaults from the user's save (session 5 end)
- The user's save baked into code defaults: gravity 11858 x real G, bounce 0.95; planets 396, spread 4500 m, size 1..197 m, speed 0..5.5 m/s; bullet mass 19.5 kg, life 7.2 s, size 0.15 m, color #6694ff, halo 0.2 m; machine gun rate 10, speed 2000, spread 8, cone 0; explosion speed 0, spread 13.5, size 0.235 m, colors #fff9e5 -> black; side colors #828282 -> black, reverse #8f8f8f -> black; debug grid off. Everything else already matched. "bullet/gone after steps" was 0 in the save (below the slider's 1, acts as 1): code keeps 1.
- Demo "balls" renamed "Ball Pit" (was "Erasmos Balls"), id unchanged.

## Stars (session 6, 2026-11-15, talked through and built, not yet checked in Chrome)
User: "beautiful sparkly stars". Agreed sketch (user: "amazing"): universe / stars [ count ] [ size ] [ brightness ] [ twinkle ] [ colors ], on the sky (infinitely far, turn with the camera, never closer), look only.
- render/stars.c + render_shader_stars (end of shaders.c): no vertex buffer, star k's direction, brightness (most faint: magnitude = random^5), tint and twinkle timing come from a hash of k. Drawn with w = 0 (a direction, so the camera's move drops out), first in the screen pass, additive, depth off: everything after covers them. A quad 4 core sizes out: core, soft glow, a cross of light on the bright ones (sparkle), all flicker with twinkle. render_draw_stars(RenderStars) once a frame, up to 100000.
- Engine.stars (RenderStars), engine_draw draws them when count > 0. Panel: universe / fold "stars" ("on the sky, look only"), blocks stars (count 0..20000, "size px" 0.5..8), light (brightness 0..3, twinkle 0..1), colors ("base color", "tint 1", "tint 2": each star is the base color tinted a random amount toward one of the tints). Picker names were Claude's, not confirmed.
- hunter2: 6000, 2.5 px, brightness 1, twinkle 0.4, base #fff8eb-ish, tints blue (0.6, 0.75, 1) and orange (1, 0.7, 0.4).
- Star uniform is uploaded before the mesh scene: the headless fake reads the last uniform write as the camera.
- Headless: draw(4, 6000) each frame, the count slider changes it, 0 draws none. The fake does not compile shaders: the WGSL is unchecked until Chrome.

## Explosion aim (session 6, built, not yet checked in Chrome)
User: the explosion shoots off in a cone between A (mirrored off the surface, a ricochet) and B (straight out of the surface), with a control for how much toward B; the cone stays the emitter type's cone slider.
- Object.hit_straight_out (0 = mirrored, 1 = straight out), panel "on hit" slider "straight out" (Claude's name, not confirmed). hunter2 bullet 0.5. ObjectCopy.hit_direction set at the first hit: engine_contact_closing keeps each contact's velocity difference before the solver, engine_mark_hit mirrors it off the contact normal and blends toward the normal. EngineBurst and emitter_burst take the direction (was straight up).
- Headless (spread 300, at 0.5): bullets came in 9 degrees off head on (coming . out = -0.987), bursts pointed 4.5 degrees off straight out (0.996): halfway, as it should. The fake keeps one slider per label and the planet's "on hit" block also has "straight out", so 0 and 1 were not tried by slider.
- The explosion type's cone is still 3.14 (every way) in code: the user turns it down to see the cone.
- In Chrome: "doesnt look like its working". Likely cause told to the user: explosion speed 0 with spread 13.5 gives half the sparks a negative speed, so they shoot backwards (a mirror cone). Offered A raise speed above spread in the panel (picked implicitly, the user moved on), B never let speed go below 0 (not built).
- User: the cone edge is too sharp, should be densest in the middle and thin out to the cone's edge. Built EmitterType.cone_middle (panel cone block "denser middle" 0..1, Claude's name, every type). 0 = even over the cap as before, up to 1 the share of the cap is drawn as 1 - (1 - random)^(1 / (1 + 3 x middle)): density goes as (1 - share)^(3 x middle), none at the edge. Sent to the GPU in the type's spare slot (ParticleTypeGpu.cone_middle). Explosion 1, others 0 (thrusters unchanged). Not yet checked in Chrome.

## Explosion speed from the hit (session 6, built, not yet checked in Chrome)
User: explosion speed based on bullet speed / impact force. Offered A the bullet's speed against what it hit, B only its speed into the surface (head on fast, skimming slow); user: "lets try B first and see".
- Object.hit_speed_share, panel "on hit" slider "speed from hit" 0..1 (Claude's name). ObjectCopy.hit_speed = the copy's speed toward the other's center at the first hit (from engine_contact_closing, never below 0). The burst's sparks get type speed + share x hit_speed (EngineBurst.extra_speed, emitter_burst's extra_speed). hunter2 bullet 0.05.
- Headless (code defaults, 2000 m/s bullets): hits at 1971..1980 m/s into the surface of 1997..2005 m/s, so about 99 m/s extra at 0.05.
- Bullets hard to see (user, "not a priority"): told the numbers (0.15 m is 1.2 px at 100 m), offered A bigger, B streaks along their way (code), C slower. Not picked. The user said they are not at 2000 m/s.

## Planet looks and asteroids (session 6, built, not yet checked in Chrome)
User: planets look bad (smooth but boring, all the same); wants many smaller flying things and a few big planets, small ones with more varied speed. Sketch agreed (user: "1. greeat 2. B sounds better"): looks on the object (three colors, each copy mixes its own; surface pattern patches | bands | both, detail, roughness; air color, thickness), and B: a second placing "asteroids" with its own object.
- Renderer: RenderSurface + render_draw_sphere_surface(center, radius, surface, seed). Up to 16 surfaces a frame in a uniform (mesh binding 7, MeshSurface 80 bytes), told apart by pointer. The instance's photo field carries 2 + slot + seed (photo now only 0.5..1.5). Mesh shader: value noise layered 5 times (roughness = how much fine layers count), patches (two edges: color a -> b at 0.44..0.5, -> c at 0.6..0.7, finer shading inside), bands (stripes along a seed tilted axis, warped by noise, spots of c), both = seed picks. The seed also shifts the noise, rotates the colors' order (3 ways) and lightness 0.85..1.15. Air: rim pow(1 - n.view, 3), brighter on the sunny side, added after lighting.
- Object.surface (RenderSurface), color_one copied from Object.color when drawn (keeps the saved "color" key). Copy seed = fract(place x 0.618). Panel for ball objects: model block adds "color 2", "color 3"; blocks "surface" (pattern, detail, roughness) and "air" ("air color", thickness). Glow / halo left as they are (they do nothing on balls; not removed, no save asked for).
- Placing: mostly_small (sizes from even to random^5, as a blend: no powf without libc), small_faster (top of the speed range comes down to speed min the bigger it is). Panel: "mostly small" in the size block, "small ones faster" in the speed block. placed_with is 8 numbers.
- hunter2: object "asteroid" (appended to the enum, density 2500, greys and brown, patches, detail 3, roughness 0.8, no air). Planet surface: sand / deep blue / cream, both, detail 2.5, roughness 0.5, blue air 0.6. Code defaults: planets 30, 60..400 m, 0..3 m/s, mostly small 0.5, small faster 1; asteroids 300, 2..40 m, 0..25 m/s, 0.7, 1, both spread 4500. The user's save still holds planets 396, 1..197 m, 0..5.5 m/s, which load over these.
- Headless: 330 spheres, sizes median 10 m, 90% 37.6, max 393; small ones up to 24 m/s, big ones under 2. With the user's planet numbers + asteroids: 696 bodies, 2.1 ms per frame in node. The fake does not compile shaders.

- User: the colors make them all look the same. Offered A color spread (hue turn per copy), B several color sets; user: 10 sets, not picked by them, asteroids just more variation. Built: RenderSurface.color_sets (0 its three, 1 each copy picks one of 10 sets in the shader, COLOR_SETS: sea/land/sand, rust, ice, cream, purples, black/red/orange, greens, ocean, desert, deep blue) and variation (hue turn up to about 70 degrees either way at 1, lightness spread 0.3 + 0.5 x variation). MeshSurface is 96 bytes (variety vec4). Panel model block: choice "colors" these three | 10 sets, slider "variation" (Claude's names). Planet: sets, variation 0.15. Asteroid: its three, variation 0.4. WGSL: "set" is reserved, renamed.

## Many asteroids, shared spread (session 6, built, not yet checked in Chrome)
User: render a lot more asteroids, denser throughout; spread shared between planets and asteroids. Agreed: asteroids "pulls: no" (still pulled by planets, user asked and was told yes).
- User's save pasted first (full text below in Log) and baked into code defaults: gravity 10000; stars 20000, 2.9 px, brightness 0.6, twinkle 0.1, tint 2 #ffeb66; planets 32, size min 520 / max 450 (min above max, kept: 450..520 m), speed 0..5.5; asteroids 1..20 m (count 2000 in code, the save has 500 and loads over it); bullet size 0.1, color #ff66b8, halo 0.08, speed from hit 0.13; planet density 13804; machine gun speed 145, spread 0; explosion 480 sparks, life 1.6 +-0.8, speed 0 +-16, drag 0.45, cone 1.24, denser middle 0.55, size 0.09, colors #ff0a0a -> #fffca8. Spread: the save had planets 8000, asteroids 10000; shared default 10000.
- Engine.spread (universe fold "spread", note "every placing puts its copies in this ball", slider "spread m"); Placing.spread gone, placed_with[1] is the universe's spread. Count slider up to 5000. Copy capacity 8192, bodies 8192.
- Object.pull_off (panel block "gravity", choice "pulls" yes | no). engine_add_gravity: every puller with every body, two pullers once with force on both, a non-puller only pulled. Cost pullers x bodies. Asteroid and bullet pull_off = 1 (bullet: Claude's call, 19.5 kg pulls nothing visible; told the user).
- Hits: sort and sweep along x (EngineSpan: x +- radius + reach, merge sorted), pairs only where spans overlap, then contacts merge sorted by (id_a, id_b) for the warm start.
- Headless: 2032 bodies 0.6 ms per frame, 5032 bodies 1.5 ms (node), no NaN after 10 s of firing. Spread 1500: 208 bullet hits on planets, 42 on asteroids in 15 s, up to 130 contacts a step.

## Session 6 end: defaults from the user's final save
- Baked in (user: "my final settings ... save these as defaults"): gravity 2000; spread 15000; planets 30, 282..1144 m, mostly small 0.15, speed 0..17.5, small ones faster 1; asteroids 5000, 5..30 m, mostly small 0.95, speed 5..30, small ones faster 0.95; bullet mass 9817.48 kg; planet density 5000; asteroid density 1334. Pulls as before (ship and planet yes, bullet and asteroid no). Everything else already matched.
- Checked in Chrome by the user through the session: stars ("looks awesome"), explosion aim / denser middle / speed from hit ("kick ass"), planet looks ("seriously awesome"), color sets ("very nice"), many asteroids (user tuned them up to 5000 and saved). Headless with the final defaults: 5030 bodies, 1.45 ms per frame in node, no NaN after 10 s of firing.

## Next
- Waiting on the user's first instruction next session. Picked up from them, not started: bullets hard to see (offered A bigger, B streaks along their way, C slower; "not a priority").
- Open from session 6: "variation", "colors", "color 2/3", "surface", "air", "thickness", "mostly small", "small ones faster", "straight out", "speed from hit", "denser middle", "pulls", "base color", "tint 1/2" are Claude's names, none confirmed. The planet object still shows an "on hit" block (does nothing). Glow / halo still shown on ball objects (do nothing there).
- Waiting on the user: shorter labels inside blocks (needs their save pasted first), camera in blocks (distance | position | follow) offered.
- Later, not now (user): put down new actuators and emitters anywhere on an object or in the world, set x, y, z (relative or fixed), etc. Inputs editable from the panel.
- Open: names (actuator types, emitter types "main"/"side", instance names, Source / INPUT_*, Engine). The user found "reverse left" as an emitter name confusing (reads like a thruster); "nose left" was offered, not picked.
- Session 6 first part (stars, explosion aim, denser middle, speed from hit) committed as 1d7859f, pushed and deployed. The rest (planet looks, color sets, asteroids, shared spread, pulls, sweep, final defaults) committed and pushed at the session's end on the user's word ("commit and publish this as a final revision for tonight") as d9e6098, then deployed when the user asked ("did you push and deploy?" ... "yes").
- Commit only when the user asks. Deploy only when the user asks.

## Machine notes
- Headless picks for model instances (16 floats: position 0..2, scale 4..6, glow 7, rotation 8..11, color 12..14, alpha 15): bullets = scale 0.25 and glow 1, planets = scale >= 10 with equal x/y scale and alpha 1, the ship's ball model 384 corners, planet model 6144. Planet dial sliders by label: "count", "spread m", "size min m", "size max m", "speed min m/s", "speed max m/s", "strength × real G", "bounce". canvasHandlers.click() locks the mouse, canvasHandlers.mousedown({button: 0}) fires, windowHandlers.mouseup({button: 0}) stops. The fake has no localStorage: define one in the script.
- Headless scripts on fake.mjs: draws and dispatches are never cleared, clear them per frame in long runs. hooks.write sees every buffer write: filter the particle frame uniform exactly (header 36 floats + 16 emitter types x 4 + 64 anchors x 8 + emitter_count x 24, emitter_count at u32 index 31), a loose length filter matched other buffers and ran out of memory. The ship's model instance is 16 floats with scale 1 (index 4) and green 0.87 (index 13). Scene uniform: eye at floats 20..22.

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
- Panel: a side panel tree, folded at first, hidden while flying, shown on Esc (user, sketch "love it"). Emitters under their object, emitter types not. Top lists called "actuators" and "emitters", no "types" folder (user: semantically useless). No particles group: streak and drag per emitter type, max particles code only (user).
- Shared is shared, split makes it its own (user). Only emitters pick a type (user: option A). New types from "+ new" (actuators ask push or turn) and copy (user: both).
- Slider values typed by hand in a light box (user). Particle colors (start and end) picked in the panel (user).
- A part's settings grouped into blocks with light crossed lines, side by side and stacked (user). Emitter types: rate, life, speed, cone, size, color.
- Bullets: no guns on the ship (user: too tied to this game). An emitter type emits sparks or copies of an object (user). Objects get their own entries under "objects", ship too (user). Model block (size, color, glow, halo) on objects (user). No recoil (user). Glow A (flat full color) then halo B (user).
- Universe heading (user's name, was "world"): gravity (strength, bounce) and placings. Gravity on everything, hits for everything (user). Planet dials act right away (user found "again" only confusing).

## Log
- 2026-11-15 (session 6): user's save before spread became shared (baked into code defaults, see "Many asteroids"): planets/spread m 8000, asteroids/spread m 10000, asteroids/count 500, the rest as listed there.
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
- 2026-11-15 (session 3): panel talked through with sketches: side panel tree (user: "love it"), emitters under the ship, top lists "actuators" / "emitters" (user rejected a "types" folder), no particles group (streak and drag per emitter type: built, user: "looks fine, although it looks stronger than before (sides and etc)"). Side panel built. Slider values typed by hand. Explained speed / base speed / speed spread (the user's base speed 30 is above speed 28.5, so sparks are slightly slower at full thrust). Long back and forth on picking types: the user found a type list on actuators that includes "reverse" for "right" senseless, and asked why thrusters would know about emitters (they do not, the pick is on the emitter; the emitter names like "reverse left" made it look otherwise). Settled: split for both, pick only for emitters, + new and copy. Built, then color pickers, then blocks for an emitter type's settings (user: "much nicer", "amazing"). Session ended with a handover.
- 2026-11-15 (session 4): parts editing checked in Chrome ("good for now"; the user made their own "reverse" emitter type). Bullets talked through: no guns on the ship, an emitter type emits objects, objects get their own entries under "objects" (ship too), model block on both, no recoil. User's save baked in, then built.
