// Debug panel: sliders that C declares. Each slider writes its value straight into C's
// memory (an f32 at the pointer C gave), so C just reads its own variable, no calls per frame.

const decoder = new TextDecoder();

export function createUi(ctx) {
    let panel = null;
    let title = null;
    let bar = null;      // demo bar along the bottom
    const tabs = [];     // one per demo, in C's order
    const demoIds = [];
    let pickedPtr = 0;   // C's i32 that a click writes the tab index into
    const text = (ptr, len) => decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));

    function makePanel() {
        panel = document.createElement("div");
        panel.style.cssText = "position:fixed;top:12px;right:12px;padding:10px 12px;background:rgba(10,12,20,0.8);" +
            "color:#ccc;font:12px monospace;border-radius:6px;display:grid;grid-template-columns:auto 140px 48px;gap:6px 8px;align-items:center";
        document.body.appendChild(panel);
    }

    return {
        env: {
            // drops the whole panel, the next slider or picker makes a new one
            js_ui_clear() {
                if (panel) panel.remove();
                panel = null;
            },
            // tabs are divs, not buttons: a focused button would also react to Space, which respawns
            js_ui_demo(idPtr, idLen, chapterPtr, chapterLen, namePtr, nameLen) {
                if (!bar) {
                    bar = document.createElement("div");
                    bar.style.cssText = "position:fixed;left:0;right:0;bottom:0;display:flex;gap:4px;padding:6px 12px;" +
                        "overflow-x:auto;background:rgba(10,12,20,0.8);color:#999;font:13px monospace";
                    document.body.appendChild(bar);
                }
                const index = tabs.length;
                const tab = document.createElement("div");
                tab.textContent = `ch ${text(chapterPtr, chapterLen)}  ${text(namePtr, nameLen)}`;
                tab.style.cssText = "padding:4px 10px;border-radius:4px;cursor:pointer;white-space:pre;user-select:none";
                tab.addEventListener("click", () => {
                    if (pickedPtr) new Int32Array(ctx.memory.buffer, pickedPtr, 1)[0] = index;
                });
                tabs.push(tab);
                demoIds.push(text(idPtr, idLen));
                bar.append(tab);
            },
            js_ui_demo_bar(ptr) {
                pickedPtr = ptr;
                const scene = new URLSearchParams(location.search).get("scene");
                if (!scene) return;
                const index = demoIds.indexOf(scene);
                if (index >= 0) new Int32Array(ctx.memory.buffer, ptr, 1)[0] = index;
                else console.warn(`[js] no scene "${scene}", there is: ${demoIds.join(", ")}`);
            },
            js_ui_demo_current(index) {
                tabs.forEach((tab, i) => {
                    tab.style.color = i === index ? "#4f4" : "";
                    tab.style.background = i === index ? "rgba(68,255,68,0.12)" : "";
                });
                tabs[index].scrollIntoView({ block: "nearest", inline: "nearest" });
                const url = new URL(location.href);
                url.searchParams.set("scene", demoIds[index]);
                history.replaceState(null, "", url); // reload keeps the demo, no new history entry per switch
            },
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
            // logarithmic (log = 1): equal slider distance multiplies the value by the same amount, min must be > 0
            // a button that picks an image file and puts it into a texture. stays in this browser tab, never uploaded
            js_ui_image(namePtr, nameLen, texture) {
                if (!panel) makePanel();
                const label = document.createElement("span");
                label.textContent = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const input = document.createElement("input");
                input.type = "file";
                input.accept = "image/*";
                input.style.cssText = "grid-column:span 2;font:inherit;color:inherit";
                input.addEventListener("change", () => {
                    const file = input.files[0];
                    if (!file) return;
                    ctx.gpuLoadImage(texture, file)
                        .then(() => console.log(`[js] image ${file.name} loaded into texture #${texture}`))
                        .catch((e) => console.error(`[js] image ${file.name} failed: ${e.message}`));
                });
                panel.append(label, input);
            },
            js_ui_slider(namePtr, nameLen, min, max, step, log, valuePtr) {
                if (!panel) makePanel();
                const name = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const value = new Float32Array(ctx.memory.buffer, valuePtr, 1)[0];
                const digits = step >= 1 ? 0 : 2;
                const LOG_STEPS = 1000;
                const fromPosition = (p) => log ? Math.round(min * (max / min) ** (p / LOG_STEPS) / step) * step : p;
                const toPosition = (v) => log ? LOG_STEPS * Math.log(v / min) / Math.log(max / min) : v;

                const label = document.createElement("span");
                label.textContent = name;
                const input = document.createElement("input");
                input.type = "range";
                input.min = log ? 0 : min;
                input.max = log ? LOG_STEPS : max;
                input.step = log ? 1 : step;
                input.value = toPosition(value);
                input.toPosition = toPosition; // for the headless tests
                const shown = document.createElement("span");
                shown.textContent = value.toFixed(digits);

                input.addEventListener("input", () => {
                    const v = fromPosition(Number(input.value));
                    new Float32Array(ctx.memory.buffer, valuePtr, 1)[0] = v; // memory can grow, so take a fresh view
                    shown.textContent = v.toFixed(digits);
                });
                input.addEventListener("keydown", (e) => e.stopPropagation()); // arrows move the slider, not the game
                panel.append(label, input, shown);
            },
        },
    };
}
