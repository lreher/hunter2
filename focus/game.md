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

## Open questions
- (user decides the design)

## Log
- 2026-11-14: focus created. Book paused (focus/physics-engine.md). Last commit before this stage: 9a05cad.
- 2026-11-14: new session. setup ok, build and unit tests pass. Waiting for the first instruction.
- 2026-11-14: step 1 done. base/: Quaternion (w, x, y, z), quaternion_identity, _from_axis_angle, _multiplied, _normalize, _add_rotation (book 9.4.3, angular velocity in world axes), _rotated_vector, _lerp (short way round), matrix4_from_rotation_position. Renamed M4 -> Matrix4, m4_ -> matrix4_ everywhere (user). tests/test.c test_quaternion; mutation checks on multiply, rotate and add_rotation order all make tests fail. Nothing in the game uses it yet.
- 2026-11-14: step 2 built (browser check pending). The instanced mesh pipeline already turned instances by a quaternion, so no new pipeline or model matrix: render_create_model(corners, triangle_count) builds a flat shaded shape from a C array (faces turned away from the center, so shapes must be convex), render_draw_model(model, position, rotation, scale, color). Up to 16 models, 256 instances each, drawn in the shadow pass too. src/demo/ship.c (Ship: position, rotation, previous of both for blending, angular_velocity): a wedge, nose -z, spins at 0.6 rad/s around y at (0, 0, 12) in the cloud demo. Cloud shadow area grows to include the ship. Headless: one 18 vertex draw per pass, no errors.
- 2026-11-14: hunter2 demo added (DEMO_HUNTER2 first, default on load, no chapter: ui.js shows just the name). Ship moved out of the cloud demo into it, at the origin, camera behind and above. Headless sim/jitter/keys scripts now set SCENE=balls themselves (they assumed balls was the default). All run clean.
