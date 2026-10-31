// Sends fake key events and counts respawns.
// Runs the real index.js + wasm against a fake WebGPU, drives 200 frames at 60 Hz,
// and draws the instance buffer C sent as ASCII so we can see the balls.
import fs from "fs";
const root = new URL("../../web/", import.meta.url).pathname; // real wasm + JS, fake WebGPU
const SW = 1600, SH = 900;
let lastInstances = null, draws = [];

const fake = (name) => new Proxy(function () {}, {
  get(_, p) {
    if (p === "then") return undefined;
    if (p === "lost") return new Promise(() => {});
    if (p === "getCompilationInfo") return async () => ({ messages: [] });
    if (p === "addEventListener") return () => {};
    return fake(name + "." + String(p));
  },
  apply(_, __, args) {
    const m = name.split(".").pop();
    if (m === "writeBuffer" && args[4] > 16) lastInstances = new Float32Array(args[2].slice(args[3], args[3] + args[4]));
    if (m === "draw") draws.push(args.join(","));
    if (m === "createRenderPipeline") console.log("[webgpu] stride", args[0].vertex.buffers[0].arrayStride, "attrs", args[0].vertex.buffers[0].attributes.length);
    return fake(name + "()");
  },
});
const canvas = { clientWidth: SW, clientHeight: SH, width: 0, height: 0, getContext: () => fake("ctx") };
globalThis.document = { getElementById: () => canvas, body: {} };
const handlers = {};
globalThis.window = { addEventListener(type, fn) { handlers[type] = fn; } };
Object.defineProperty(globalThis, "navigator", { configurable: true, value: {
  gpu: { requestAdapter: async () => fake("adapter"), getPreferredCanvasFormat: () => "bgra8unorm" } } });
globalThis.devicePixelRatio = 1;
globalThis.GPUBufferUsage = {}; globalThis.GPUTextureUsage = {};
globalThis.fetch = async () => new Response(fs.readFileSync(root + "game.wasm"), { headers: { "content-type": "application/wasm" } });
let pending = null;
globalThis.requestAnimationFrame = (cb) => { pending = cb; };

const logs = [];
const origLog = console.log;
console.log = (...a) => { const line = a.join(" "); if (line.includes("balls spawned")) logs.push(line); origLog(...a); };
await import(root + "index.js");
console.log = origLog;
console.log = (...a) => { const line = a.join(" "); if (line.includes("balls spawned")) logs.push(line); if (!line.includes("stats") && !line.includes("gpu create")) origLog(...a); };

function plot(label) {
  const COLS = 80, ROWS = 22;
  const grid = Array.from({ length: ROWS }, () => Array(COLS).fill(" "));
  const n = lastInstances.length / 16;
  let circles = 0, outside = 0, minY = 1e9;
  for (let i = 0; i < n; i++) {
    const [x, y, w, h] = lastInstances.slice(i * 16, i * 16 + 4);
    const shape = lastInstances[i * 16 + 12];
    if (shape === 2) { for (const row of grid) row.fill("."); continue; }
    if (shape !== 1) continue;
    circles++;
    const cx = x + w / 2, cy = y + h / 2, r = w / 2;
    if (cx - r < -0.5 || cx + r > SW + 0.5 || cy - r < -0.5 || cy + r > SH + 0.5) outside++;
    minY = Math.min(minY, SH - (cy + r));
    const col = Math.min(COLS - 1, Math.floor(cx / SW * COLS)), row = Math.min(ROWS - 1, Math.floor(cy / SH * ROWS));
    grid[row][col] = "o";
  }
  const balls = [];
  for (let i = 0; i < n; i++) { const q = lastInstances.slice(i * 16, i * 16 + 13); if (q[12] === 1) balls.push({ x: q[0] + q[2] / 2, y: q[1] + q[3] / 2, r: q[2] / 2 }); }
  let worst = 0, overlapping = 0, nan = 0;
  for (const b of balls) if (!Number.isFinite(b.x) || !Number.isFinite(b.y)) nan++;
  for (let i = 0; i < balls.length; i++) for (let j = i + 1; j < balls.length; j++) {
    const d = Math.hypot(balls[i].x - balls[j].x, balls[i].y - balls[j].y), o = balls[i].r + balls[j].r - d;
    if (o > worst) worst = o; if (o > 1) overlapping++;
  }
  console.log(`    overlap: worst ${(worst / 100).toFixed(3)} m (${worst.toFixed(1)} px), pairs overlapping >1px: ${overlapping}, NaN: ${nan}`);
  console.log(`--- ${label}: ${n} quads, ${circles} circles, ${outside} outside the box, lowest ball bottom ${minY.toFixed(1)}px above floor`);
  console.log(grid.map((r) => r.join("")).join("\n"));
}



function positions() {
  const out = [];
  for (let i = 0; i < lastInstances.length / 16; i++) if (lastInstances[i * 16 + 12] === 1) out.push([lastInstances[i * 16] + lastInstances[i * 16 + 2] / 2, lastInstances[i * 16 + 1] + lastInstances[i * 16 + 3] / 2]);
  return out;
}
const key = (code, repeat = false) => ({ code, repeat, preventDefault() { prevented++; } });
let prevented = 0, t = 1000;
function frame() { const cb = pending; pending = null; t += 1000 / 60; cb(t); return positions(); }
function meanDiff(a, b) { return a.reduce((s, p, i) => s + Math.hypot(p[0] - b[i][0], p[1] - b[i][1]), 0) / a.length; }

for (let f = 0; f < 300; f++) frame();
console.log("spawns so far:", logs.length, "(expect 1, the initial one)");
const before = frame();

// 1. one press
handlers.keydown(key("Space")); handlers.keyup(key("Space"));
const after1 = frame();
console.log("after one press: spawns", logs.length, "| mean move vs before", meanDiff(before, after1).toFixed(1), "px (a respawn is hundreds)");

// 2. held key: first event then repeats, over 10 frames
handlers.keydown(key("Space"));
const heldA = frame();
let heldSpawnsBefore = logs.length;
for (let f = 0; f < 10; f++) { handlers.keydown(key("Space", true)); frame(); }
console.log("held for 11 frames: extra spawns after the first frame =", logs.length - heldSpawnsBefore, "(expect 0)");
handlers.keyup(key("Space"));

// 3. unknown key and arrows do nothing to the balls
const spawnsBeforeOther = logs.length;
handlers.keydown(key("KeyZ")); handlers.keydown(key("ArrowLeft")); frame();
console.log("other keys: extra spawns =", logs.length - spawnsBeforeOther, "(expect 0)", "| preventDefault calls so far:", prevented);

// 4. second press later gives a different layout than the first respawn
for (let f = 0; f < 100; f++) frame();
const settled = frame();
handlers.keydown(key("Space")); handlers.keyup(key("Space"));
const after2 = frame();
console.log("second press: spawns", logs.length, "| layout differs from previous spawn by", meanDiff(heldA, after2).toFixed(1), "px");

// 5. blur while held, then press again works
handlers.keydown(key("Space")); handlers.blur(); handlers.keyup(key("Space")); frame();
handlers.keydown(key("Space")); frame();
console.log("after blur and another press: spawns", logs.length);
