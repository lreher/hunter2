# focus: game

Goal: turn the Gravity Cloud demo (src/demo/cloud.c) into a game. The game is 3D (user, 2026-11-14).

## How we work here
- The user owns the game design. Claude does not guess or pitch game ideas, and does not ask leading questions about them. Wait for the user to say what to build, then build that.

## State
- Starting point: Gravity Cloud (src/demo/cloud.c). Up to 500 bodies (CLOUD_MAX), all pairs gravity (FORCE_ATTRACT force generators, book 3.2 and ch 5) and our sphere collisions, 4 solver passes. One material: mass = density * r^3 (r 0.1 is 1 kg). No ground gravity, no box, no damping. Start modes: explode, random, spin, scaled by a speed setting. G slider (in units of 0.001).
- hunter2 is its own demo (src/demo/hunter2.c, id "hunter2"), first in the bar and the one that loads by default (user, 2026-11-14). Other demos (balls, cloud, bridge, pool) stay as they are.

## Decisions
- Game is 3D (user, 2026-11-14).
- First feature: a controllable spaceship. Making it fun to drive is the user's job later; it needs a clear in-code interface for trying different movement settings (user, 2026-11-14).
- Ship plan, in this order (user, 2026-11-14):
  1. Quaternion math in base/ (orientation), with unit tests.
  2. Hand-made mesh in a C array + mesh pipeline with a model matrix. Wedge ship first.
  3. Ship state (particle + orientation + turn speed) and a default movement model (thrust along the nose, turning). Clear tuning interface.
  4. Chase camera (behind and above, smoothed). Free-fly camera kept as a debug toggle. Debug console later.
  5. Optional: ship in the cloud's gravity and collisions.

## Ship code map (src/demo/ship.c)
- ship_update_aim, once per browser frame: mouse turns ship_aim (unit world direction) around the camera's up and right, kept within limit of the nose. Mouse free or free camera: aim = nose. ShipAimTuning: sensitivity 0.002 rad/px, full_turn_angle 0.6 rad, limit 1.0 rad, invert_pitch.
- ship_read_input: angle from nose to aim in ship axes (atan2 of the aim's up and right parts against ahead) / full_turn_angle -> pitch, yaw (-1..1), plus keys (W/S thrust, A/D yaw, Q/E roll). Mapping lives here. No input while the free camera is on.
- ship_draw_marks: a cross where the nose points, an orange dot for the aim (both 200 m out, render_world_to_screen). ship_draw_hint while unlocked.
- ship_step(input, time_step): the flight model. Thrust along the nose (particle_add_force), drag as particle damping (1 - drag * time_step), turn speed in ship axes eases toward input * turn speed (turn_response), then quaternion_add_rotation.
- ShipTuning ship_tuning / ShipCameraTuning ship_camera_tuning: every feel number, defaults in their initializers, all on the panel as sliders (kept across demo switches).
- ship_camera_step / ship_camera_view: chase camera, goal = ship + rotation * (0, height, distance), eye and up close follow * time_step of the gap per step, looks look_ahead m in front of the ship.
- Camera hook: demo_view(demo, alpha, DemoView*) in demo.h, Demo.own_camera = 1 for hunter2. Backspace toggles the free camera (game.c), which starts from the chase view (f32_atan2). demo_free_camera (set by game every frame) tells the demo to leave keys and mouse alone. demo_update now gets the frame's seconds.

## Open questions
- (user decides the design)

## Log
- 2026-11-14: focus created. Book paused (focus/physics-engine.md). Last commit before this stage: 9a05cad.
- 2026-11-14: new session. setup ok, build and unit tests pass. Waiting for the first instruction.
- 2026-11-14: step 1 done. base/: Quaternion (w, x, y, z), quaternion_identity, _from_axis_angle, _multiplied, _normalize, _add_rotation (book 9.4.3, angular velocity in world axes), _rotated_vector, _lerp (short way round), matrix4_from_rotation_position. Renamed M4 -> Matrix4, m4_ -> matrix4_ everywhere (user). tests/test.c test_quaternion; mutation checks on multiply, rotate and add_rotation order all make tests fail. Nothing in the game uses it yet.
- 2026-11-14: step 2 built (browser check pending). The instanced mesh pipeline already turned instances by a quaternion, so no new pipeline or model matrix: render_create_model(corners, triangle_count) builds a flat shaded shape from a C array (faces turned away from the center, so shapes must be convex), render_draw_model(model, position, rotation, scale, color). Up to 16 models, 256 instances each, drawn in the shadow pass too. src/demo/ship.c (Ship: position, rotation, previous of both for blending, angular_velocity): a wedge, nose -z, spins at 0.6 rad/s around y at (0, 0, 12) in the cloud demo. Cloud shadow area grows to include the ship. Headless: one 18 vertex draw per pass, no errors.
- 2026-11-14: hunter2 demo added (DEMO_HUNTER2 first, default on load, no chapter: ui.js shows just the name). Ship moved out of the cloud demo into it, at the origin, camera behind and above. Headless sim/jitter/keys scripts now set SCENE=balls themselves (they assumed balls was the default). All run clean.
- 2026-11-14: steps 3 and 4 built together (browser check pending): the ship flies off screen without a chase camera. Defaults: mass 1000 kg, thrust 20000 N, reverse 10000 N, drag 0.5/s (top speed 40 m/s), pitch 1.5, yaw 1.0, roll 2.5 rad/s, turn response 6/s. Camera 6 m behind, 1.8 m up, look ahead 4 m, follow 5/s. Keys W/S thrust, arrows up/down pitch, A/D yaw, Q/E roll. Debug grid (lines, 10 m cells, y = -5, follows the ship in whole cells), panel toggle. HUD speed. Far plane 200 -> 400 m. Headless drive: speed 13, 23, 30, 34, 36 m/s over 5 s of W, D swings right (+x), up arrow climbs.
- 2026-11-14: mouse steering (user): virtual stick, not direct (user). Arrow keys dropped. Free camera moved from mouse lock to Backspace (user, new KEY_BACKSPACE). Mouse lock now means flying. Defaults: range 200 px, dead zone 0.05, recenter 0, pitch not inverted. Headless: unlocked mouse does nothing, stick right swings right, stick up climbs, centered flies straight, Backspace free camera moves 5 m/s and the ship gets no input. keys.mjs (balls) unchanged.
- 2026-11-14: virtual stick removed (user found it weird), replaced by direct mouse aim: an angle per pixel, turns right away, stops when the mouse stops. ShipInput got aim_pitch / aim_yaw. Headless: 523 px right turns 90 degrees, ship holds its heading after.
- 2026-11-14: direct mouse felt too free (user). Now two points (user's idea): the mouse moves an aim point, the ship turns toward it through the normal flight model (turn speeds, turn response), so it keeps the stick feel. render_world_to_screen added. Headless: aim 0.4 rad right settles smoothly in about 2 s, no overshoot.
- 2026-11-14: panel grouped and trimmed (user found it confusing). plat_ui_group(name) / js_ui_group: the panel is a row of columns. hunter2 groups: flight (thrust, reverse, drag, pitch, yaw, roll rad/s, turn response), camera (distance, height, look ahead, follow), debug (grid). Mass and aim tuning off the panel, code only. Other demos unchanged (one unnamed column).
