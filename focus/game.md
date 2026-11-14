# focus: game

Goal: turn the Gravity Cloud demo (src/demo/cloud.c) into a game. The game is 3D (user, 2026-11-14).

## How we work here
- The user owns the game design. Claude does not guess or pitch game ideas, and does not ask leading questions about them. Wait for the user to say what to build, then build that.

## State
- Starting point: Gravity Cloud (src/demo/cloud.c). Up to 500 bodies (CLOUD_MAX), all pairs gravity (FORCE_ATTRACT force generators, book 3.2 and ch 5) and our sphere collisions, 4 solver passes. One material: mass = density * r^3 (r 0.1 is 1 kg). No ground gravity, no box, no damping. Start modes: explode, random, spin, scaled by a speed setting. G slider (in units of 0.001).
- Other demos (balls, bridge, pool) stay as they are.

## Decisions
- Game is 3D (user, 2026-11-14).

## Open questions
- (user decides the design)

## Log
- 2026-11-14: focus created. Book paused (focus/physics-engine.md). Last commit before this stage: 9a05cad.
