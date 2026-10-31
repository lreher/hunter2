// Entry point: set up the browser side, load the wasm, start the game.
import { createEnv } from "./env.js";
import { createGpu } from "./gpu.js";
import { createInput } from "./input.js";

const ctx = { canvas: document.getElementById("screen"), memory: null };

const input = createInput(ctx.canvas);
const env = { ...createEnv(ctx), ...input.env, ...(await createGpu(ctx)) };
const { instance } = await WebAssembly.instantiateStreaming(fetch("game.wasm"), { env });
ctx.memory = instance.exports.memory;
console.log("[js] wasm loaded");

instance.exports.game_init();

// one call into C per browser frame. if C traps, the loop stops instead of spamming errors
function frame(nowMs) {
    instance.exports.game_frame(nowMs);
    input.endFrame();
    requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
