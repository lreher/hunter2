// Sends fake key and mouse events: respawns on space, camera moves and turns.
import { start, frame, windowHandlers as win, canvasHandlers as cvs } from "./fake.mjs";
import * as gpu from "./fake.mjs";

const logs = [];
const origLog = console.log;
console.log = (...a) => { const line = a.join(" "); if (line.includes("balls spawned")) logs.push(line); if (!line.includes("stats") && !line.includes("gpu create")) origLog(...a); };
await start();

const pos = () => frame().map((b) => [b.x, b.y, b.z]);
const meanDiff = (a, b) => a.reduce((s, p, i) => s + Math.hypot(p[0] - b[i][0], p[1] - b[i][1], p[2] - b[i][2]), 0) / a.length;
let prevented = 0;
const key = (code, repeat = false) => ({ code, repeat, preventDefault() { prevented++; } });
const cam = () => Array.from(gpu.camera.slice(0, 16));
const camDiff = (a, b) => a.reduce((s, v, i) => s + Math.abs(v - b[i]), 0);

for (let f = 0; f < 300; f++) frame();
console.log("spawns so far:", logs.length, "(expect 1, the initial one)");
const before = pos();

// 1. one press
win.keydown(key("Space")); win.keyup(key("Space"));
const after1 = pos();
console.log("after one press: spawns", logs.length, "| mean move vs before", meanDiff(before, after1).toFixed(2), "m (a respawn is meters)");

// 2. held key: first event then repeats, over 10 frames
win.keydown(key("Space"));
const heldA = pos();
const heldSpawnsBefore = logs.length;
for (let f = 0; f < 10; f++) { win.keydown(key("Space", true)); frame(); }
console.log("held for 11 frames: extra spawns after the first frame =", logs.length - heldSpawnsBefore, "(expect 0)");
win.keyup(key("Space"));

// 3. unknown key does nothing
const spawnsBeforeOther = logs.length;
win.keydown(key("KeyZ")); frame();
console.log("unknown key: extra spawns =", logs.length - spawnsBeforeOther, "(expect 0)");

// 4. second press later gives a different layout
for (let f = 0; f < 100; f++) frame();
win.keydown(key("Space")); win.keyup(key("Space"));
const after2 = pos();
console.log("second press: spawns", logs.length, "| layout differs from previous spawn by", meanDiff(heldA, after2).toFixed(2), "m");

// 5. blur while held, then press again works
win.keydown(key("Space")); win.blur(); win.keyup(key("Space")); frame();
win.keydown(key("Space")); frame(); win.keyup(key("Space"));
console.log("after blur and another press: spawns", logs.length);

// 6. camera: nothing moves it while unlocked. locked: W for 1 s moves it, Shift makes it faster, mouse turns it
const holdW = (shift) => {
  if (shift) win.keydown(key("ShiftRight"));
  win.keydown(key("KeyW")); for (let f = 0; f < 60; f++) frame(); win.keyup(key("KeyW"));
  if (shift) win.keyup(key("ShiftRight"));
  frame();
};
frame();
const c0 = cam();
holdW(false);
win.mousemove({ movementX: 200, movementY: 0 }); frame();
console.log("unlocked: W and mouse changed the view by", camDiff(c0, cam()).toFixed(4), "(expect 0)");

cvs.click();
const c1 = cam();
holdW(false);
const c2 = cam();
holdW(true);
const c3 = cam();
// view_proj translation (column 3) changes with position. compare how much each second moved it
console.log("locked:", document.pointerLockElement !== null, "| W for 1 s changed the view by", camDiff(c1, c2).toFixed(2), "| with Shift", camDiff(c2, c3).toFixed(2), "(expect about 4x)");
win.mousemove({ movementX: 200, movementY: 0 }); frame();
console.log("mouse move while locked changed the view by", camDiff(c3, cam()).toFixed(2));

cvs.click();
const c4 = cam();
holdW(false);
console.log("click again unlocks:", document.pointerLockElement === null, "| W after unlock changed the view by", camDiff(c4, cam()).toFixed(4), "(expect 0)");
console.log("preventDefault calls:", prevented);
