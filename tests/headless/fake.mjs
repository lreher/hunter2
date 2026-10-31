// Shared fake browser for the headless scripts: runs the real web/index.js + game.wasm
// against a fake WebGPU, and reads back what C sent to the GPU.
import fs from "fs";
const root = new URL("../../web/", import.meta.url).pathname;
export const SW = 1600, SH = 900;
export const SPHERE_VERTEX_COUNTS = [240, 960, 3840, 15360]; // icosphere levels of detail, 1 to 4 subdivisions

const writes = new WeakMap(); // fake buffer -> last data written to it
const slots = [];             // {buffer, offset} bound to vertex slots
let depthPass = false;        // draws in a depth pass (the shadow map) are not what the camera sees
export const draws = [];      // "vertexCount,instanceCount" per draw call
export let spheres = [];      // {x, y, z, r} drawn in the last screen pass, all levels of detail
export let camera = null;     // floats last written to the scene uniform (view_proj, light, eye, ...)
const uniforms = new WeakSet(); // fake buffers made with the UNIFORM usage flag

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
      if (uniforms.has(args[0])) camera = data;
    }
    if (m === "beginRenderPass") {
      depthPass = args[0].colorAttachments.length === 0;
      if (!depthPass) spheres = [];
    }
    if (m === "setVertexBuffer") slots[args[0]] = { buffer: args[1], offset: args[2] ?? 0 };
    if (m === "draw") {
      draws.push(args.join(","));
      if (!depthPass && SPHERE_VERTEX_COUNTS.includes(args[0])) {
        const d = writes.get(slots[1].buffer), first = slots[1].offset / 4;
        for (let i = 0; i < args[1]; i++) {
          const o = first + i * 16;
          spheres.push({ x: d[o], y: d[o + 1], z: d[o + 2], r: d[o + 4] });
        }
      }
    }
    const result = fake(name + "()");
    if (m === "createBuffer" && args[0].usage & 0x40) uniforms.add(result);
    return result;
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
// debug panel elements made by web/ui.js. sliders[name].set(v) moves a slider like a user would
export const sliders = {};
const element = (tag) => ({ tag, style: {}, handlers: {}, textContent: "", value: "",
  addEventListener(type, fn) { this.handlers[type] = fn; },
  getBoundingClientRect: () => ({ top: 12, bottom: 66, left: 16, right: 200 }),
  append(label, input) { sliders[label.textContent] = input; }, // ui.js appends label, slider, value
  set(v) { this.value = String(v); this.handlers.input(); } });
globalThis.document = {
  getElementById: () => canvas, body: { appendChild() {} }, pointerLockElement: null, createElement: element,
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
