// Shared fake browser for the headless scripts: runs the real web/index.js + game.wasm
// against a fake WebGPU, and reads back what C sent to the GPU.
import fs from "fs";
const root = new URL("../../web/", import.meta.url).pathname;
export const SW = 1600, SH = 900;
export const SPHERE_VERTICES = 960; // icosphere, 2 subdivisions: 320 triangles

const writes = new WeakMap(); // fake buffer -> last data written to it
const slots = [];             // fake buffers bound to vertex slots
export const draws = [];      // "vertexCount,instanceCount" per draw call
export let spheres = [];      // {x, y, z, r} from the last sphere draw
export let camera = null;     // last 20 floats written to the camera uniform (view_proj + light)

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
    if (m === "writeBuffer") {
      const data = new Float32Array(args[2].slice(args[3], args[3] + args[4]));
      writes.set(args[0], data);
      if (args[4] === 80) camera = data;
    }
    if (m === "setVertexBuffer") slots[args[0]] = args[1];
    if (m === "draw") {
      draws.push(args.join(","));
      if (args[0] === SPHERE_VERTICES) {
        const d = writes.get(slots[1]);
        spheres = [];
        for (let i = 0; i < args[1]; i++) spheres.push({ x: d[i * 16], y: d[i * 16 + 1], z: d[i * 16 + 2], r: d[i * 16 + 4] });
      }
    }
    return fake(name + "()");
  },
});

// browser objects, with handlers kept so scripts can send events
export const windowHandlers = {}, canvasHandlers = {};
const canvas = {
  clientWidth: SW, clientHeight: SH, width: 0, height: 0,
  getContext: () => fake("ctx"),
  addEventListener(type, fn) { canvasHandlers[type] = fn; },
  requestPointerLock() { globalThis.document.pointerLockElement = canvas; },
};
globalThis.document = {
  getElementById: () => canvas, body: {}, pointerLockElement: null,
  exitPointerLock() { this.pointerLockElement = null; },
};
globalThis.window = { addEventListener(type, fn) { windowHandlers[type] = fn; } };
Object.defineProperty(globalThis, "navigator", { configurable: true, value: {
  gpu: { requestAdapter: async () => fake("adapter"), getPreferredCanvasFormat: () => "bgra8unorm" } } });
globalThis.devicePixelRatio = 1;
globalThis.GPUBufferUsage = {}; globalThis.GPUTextureUsage = {};
globalThis.fetch = async () => new Response(fs.readFileSync(root + "game.wasm"), { headers: { "content-type": "application/wasm" } });
let pending = null;
globalThis.requestAnimationFrame = (cb) => { pending = cb; };

export async function start() {
  await import(root + "index.js");
}

// runs one browser frame at 60 Hz, returns the spheres drawn
let t = 1000;
export function frame() {
  const cb = pending; pending = null;
  t += 1000 / 60;
  cb(t);
  return spheres;
}
