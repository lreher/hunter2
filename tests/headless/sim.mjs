// Runs web/index.js + game.wasm with a fake WebGPU. Draws the balls as ASCII and checks overlaps.
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
globalThis.window = { addEventListener() {} };
Object.defineProperty(globalThis, "navigator", { configurable: true, value: {
  gpu: { requestAdapter: async () => fake("adapter"), getPreferredCanvasFormat: () => "bgra8unorm" } } });
globalThis.devicePixelRatio = 1;
globalThis.GPUBufferUsage = {}; globalThis.GPUTextureUsage = {};
globalThis.fetch = async () => new Response(fs.readFileSync(root + "game.wasm"), { headers: { "content-type": "application/wasm" } });
let pending = null;
globalThis.requestAnimationFrame = (cb) => { pending = cb; };

await import(root + "index.js");

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

let t = 1000;
const FRAMES = +process.env.FRAMES || 900; // FRAMES=60 node sim.mjs for a quick run
for (let f = 1; f <= FRAMES; f++) {
  const cb = pending; pending = null;
  t += 1000 / 60;
  cb(t);
  if (f === 2 || f === 30 || f === 120 || f === 400 || f === 900) plot("frame " + f);
}
console.log("draw calls seen:", [...new Set(draws)].join(" "), "x", draws.length);
