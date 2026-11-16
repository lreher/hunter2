// Entry point: set up the browser side, load the wasm, start the game.
// The loading screen (index.html) names each step. Steps block the page, so before each one it waits for a frame to
// be painted with the new name. Each step's time goes to the console ("[js] load ...")
import { createEnv } from "./env.js";
import { createGpu } from "./gpu.js";
import { createInput } from "./input.js";
import { createUi } from "./ui.js";

const loadingStep = document.getElementById("loading-step");
let stepName = "", stepStart = performance.now();
async function step(name) {
    const now = performance.now();
    if (stepName) console.log(`[js] load ${stepName}: ${Math.round(now - stepStart)} ms`);
    stepName = name;
    loadingStep.textContent = name;
    await new Promise((resolve) => requestAnimationFrame(() => setTimeout(resolve, 0))); // painted, then go on
    stepStart = performance.now();
}

const ctx = { canvas: document.getElementById("screen"), memory: null };

await step("starting WebGPU");
const input = createInput(ctx.canvas);
const ui = createUi(ctx);
const env = { ...createEnv(ctx), ...input.env, ...ui.env, ...(await createGpu(ctx)) };
await step("loading game file");
const { instance } = await WebAssembly.instantiateStreaming(fetch("game.wasm"), { env });
ctx.memory = instance.exports.memory;
console.log("[js] wasm loaded");

await step("building the world");
instance.exports.game_init();

await step("drawing the first frame");
// one call into C per browser frame. if C traps, the loop stops instead of spamming errors
let firstFrame = true;
function frame(nowMs) {
    instance.exports.game_frame(nowMs);
    input.endFrame();
    if (firstFrame) {
        firstFrame = false;
        console.log(`[js] load ${stepName}: ${Math.round(performance.now() - stepStart)} ms`);
        document.getElementById("loading").remove();
    }
    requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
