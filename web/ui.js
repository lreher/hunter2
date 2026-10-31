// Debug panel: sliders that C declares. Each slider writes its value straight into C's
// memory (an f32 at the pointer C gave), so C just reads its own variable, no calls per frame.

const decoder = new TextDecoder();

export function createUi(ctx) {
    let panel = null;
    let title = null;

    function makePanel() {
        panel = document.createElement("div");
        panel.style.cssText = "position:fixed;top:12px;right:12px;padding:10px 12px;background:rgba(10,12,20,0.8);" +
            "color:#ccc;font:12px monospace;border-radius:6px;display:grid;grid-template-columns:auto 140px 48px;gap:6px 8px;align-items:center";
        document.body.appendChild(panel);
    }

    return {
        env: {
            // big text in the top left corner, in the Tiny5 pixel font (Google Fonts, see index.html)
            js_ui_title(ptr, len) {
                if (!title) {
                    title = document.createElement("div");
                    title.style.cssText = "position:fixed;top:12px;left:16px;font:46px/54px 'Tiny5',monospace;" +
                        "color:#4f4;pointer-events:none;user-select:none";
                    document.body.appendChild(title);
                }
                title.textContent = decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));
            },
            // bottom edge of the title in canvas pixels, so C can draw below it. 0 without a title
            js_ui_title_bottom() {
                if (!title) return 0;
                return Math.ceil(title.getBoundingClientRect().bottom * devicePixelRatio);
            },
            js_ui_slider(namePtr, nameLen, min, max, step, valuePtr) {
                if (!panel) makePanel();
                const name = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const value = new Float32Array(ctx.memory.buffer, valuePtr, 1)[0];
                const digits = step >= 1 ? 0 : 2;

                const label = document.createElement("span");
                label.textContent = name;
                const input = document.createElement("input");
                input.type = "range";
                input.min = min;
                input.max = max;
                input.step = step;
                input.value = value;
                const shown = document.createElement("span");
                shown.textContent = value.toFixed(digits);

                input.addEventListener("input", () => {
                    const v = Number(input.value);
                    new Float32Array(ctx.memory.buffer, valuePtr, 1)[0] = v; // memory can grow, so take a fresh view
                    shown.textContent = v.toFixed(digits);
                });
                input.addEventListener("keydown", (e) => e.stopPropagation()); // arrows move the slider, not the game
                panel.append(label, input, shown);
            },
        },
    };
}
