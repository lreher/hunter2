// Basic functions C can call: logging, screen size, clock.
// ctx is shared by all modules: { canvas, memory }. memory is set once wasm is instantiated.

const decoder = new TextDecoder();

// show errors on the page, not just the console
export function fail(msg) {
    console.error("[js]", msg);
    document.body.innerHTML = `<pre style="color:#f55;padding:16px">${msg}</pre>`;
    throw new Error(msg);
}

export function createEnv(ctx) {
    return {
        js_log(ptr, len) {
            const text = decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));
            console.log("[c]", text);
        },
        js_screen_width() {
            return ctx.canvas.width;
        },
        js_screen_height() {
            return ctx.canvas.height;
        },
        js_now_ms() {
            return performance.now();
        },
    };
}
