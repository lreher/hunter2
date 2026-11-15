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

    let group = null;    // the panel column that new sliders go into
    let demoId = "";     // the running demo, for its saved settings
    let controls = [];   // this panel's sliders and choices: {name, value(), set(v)}

    // Saved settings: one entry per demo in localStorage, {name: value}. A demo starts with its last save (each
    // control picks its value up as C declares it), load puts the last save back. Storage can be missing or blocked (private window), then nothing is kept.
    const storageKey = () => `hunter2.settings.${demoId}`;
    function loadSaved() {
        try { return JSON.parse(localStorage.getItem(storageKey()) || "{}"); } catch { return {}; }
    }
    function saveAll() {
        const values = {};
        for (const control of controls) values[control.name] = control.value();
        try { localStorage.setItem(storageKey(), JSON.stringify(values)); } catch { }
    }
    function loadAll() { // controls missing from the save (added or renamed since) keep their value
        const saved = loadSaved();
        for (const control of controls) if (typeof saved[control.name] === "number") control.set(saved[control.name]);
    }
    function addControl(name, value, set) {
        controls.push({ name, value, set });
        const saved = loadSaved()[name];
        if (typeof saved === "number") set(saved);
    }

    // the panel is a row of groups (js_ui_group), each a column of label, control, value
    function makePanel() {
        panel = document.createElement("div");
        panel.style.cssText = "position:fixed;top:12px;right:12px;padding:10px 12px;background:rgba(10,12,20,0.8);" +
            "color:#ccc;font:12px monospace;border-radius:6px;display:flex;gap:20px;align-items:flex-start";
        document.body.appendChild(panel);
        group = null;
        controls = [];

        // save and load, always the last column (flex order), whatever groups come after
        const buttons = document.createElement("div");
        buttons.style.cssText = "order:1;display:flex;flex-direction:column;gap:6px";
        for (const [label, action] of [["save", saveAll], ["load", loadAll]]) {
            const button = document.createElement("div"); // a div: a focused button would also react to Space
            button.textContent = label;
            button.style.cssText = "padding:2px 8px;border-radius:4px;cursor:pointer;user-select:none;background:rgba(255,255,255,0.08)";
            button.addEventListener("click", () => {
                action();
                button.style.color = "#4f4"; // a short flash, so the click shows
                setTimeout(() => { button.style.color = ""; }, 300);
            });
            buttons.appendChild(button);
        }
        panel.appendChild(buttons);
    }

    // Folded groups: remembered per demo on every click (not part of save / load), {name: true} in localStorage
    const foldKey = () => `hunter2.folded.${demoId}`;
    function loadFolds() {
        try { return JSON.parse(localStorage.getItem(foldKey()) || "{}"); } catch { return {}; }
    }
    function saveFold(name, folded) {
        const folds = loadFolds();
        if (folded) folds[name] = true; else delete folds[name];
        try { localStorage.setItem(foldKey(), JSON.stringify(folds)); } catch { }
    }

    // a named group folds: a click on its title hides everything under it, another click shows it again.
    // it starts the way it was left. controls added later go in hidden too, while it is folded (see target)
    function makeGroup(name) {
        if (!panel) makePanel();
        group = document.createElement("div");
        const columns = "auto 140px 48px";
        group.style.cssText = `display:grid;grid-template-columns:${columns};gap:6px 8px;align-items:center`;
        if (name) {
            const header = document.createElement("div");
            const column = group;
            let folded = loadFolds()[name] === true;
            const show = () => {
                header.textContent = (folded ? "▸ " : "▾ ") + name;
                column.style.gridTemplateColumns = folded ? "auto" : columns;
                for (const child of column.children) {
                    if (child === header) continue;
                    if (folded) { // some rows are display:flex, keep it for unfolding. already hidden: keep what was kept
                        if (child.style.display !== "none") child.foldedDisplay = child.style.display;
                        child.style.display = "none";
                    }
                    else if (child.foldedDisplay !== undefined) child.style.display = child.foldedDisplay;
                }
            };
            header.style.cssText = "grid-column:1/-1;color:#fff;font-weight:bold;cursor:pointer;user-select:none";
            header.addEventListener("click", () => { folded = !folded; saveFold(name, folded); show(); });
            column.refold = show; // after each control is added, so a folded group hides it too
            group.appendChild(header);
            show();
        }
        panel.appendChild(group);
    }

    // where the next control goes: the current group, or a first one without a name
    function target() {
        if (!panel || !group) makeGroup("");
        return group;
    }

    return {
        env: {
            // drops the whole panel, the next slider or picker makes a new one
            js_ui_clear() {
                if (panel) panel.remove();
                panel = null;
                group = null;
            },
            // starts a new column in the panel, with name on top. later controls go into it
            js_ui_group(namePtr, nameLen) {
                makeGroup(text(namePtr, nameLen));
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
                const chapter = text(chapterPtr, chapterLen); // empty for the game itself
                tab.textContent = chapter ? `ch ${chapter}  ${text(namePtr, nameLen)}` : text(namePtr, nameLen);
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
                demoId = demoIds[index];
                const url = new URL(location.href);
                url.searchParams.set("scene", demoIds[index]);
                history.replaceState(null, "", url); // reload keeps the demo, no new history entry per switch
            },
            // big text in the top left corner, in the Tiny5 pixel font (Google Fonts, see index.html)
            js_ui_title(ptr, len, color) {
                if (!title) {
                    title = document.createElement("div");
                    title.style.cssText = "position:fixed;top:12px;left:16px;font:46px/54px 'Tiny5',monospace;" +
                        "pointer-events:none;user-select:none";
                    document.body.appendChild(title);
                }
                title.textContent = decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));
                title.style.color = "#" + (color >>> 0).toString(16).padStart(6, "0");
            },
            // bottom edge of the title in canvas pixels, so C can draw below it. 0 without a title
            js_ui_title_bottom() {
                if (!title) return 0;
                return Math.ceil(title.getBoundingClientRect().bottom * devicePixelRatio);
            },
            // logarithmic (log = 1): equal slider distance multiplies the value by the same amount, min must be > 0
            // a button that picks an image file and puts it into a texture. stays in this browser tab, never uploaded
            js_ui_image(namePtr, nameLen, texture) {
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
                target().append(label, input);
                group.refold?.();
            },
            // a row of options, the picked one green. a click writes its index into C's i32.
            // divs, not buttons, like the demo tabs: a focused button would also react to Space
            js_ui_choice(namePtr, nameLen, optionsPtr, optionsLen, valuePtr) {
                const label = document.createElement("span");
                label.textContent = text(namePtr, nameLen);
                const row = document.createElement("div");
                row.style.cssText = "grid-column:span 2;display:flex;gap:4px";
                const show = (picked) => options.forEach((option, i) => {
                    option.style.color = i === picked ? "#4f4" : "";
                    option.style.background = i === picked ? "rgba(68,255,68,0.12)" : "";
                });
                const options = text(optionsPtr, optionsLen).split("|").map((name, i) => {
                    const option = document.createElement("div");
                    option.textContent = name;
                    option.style.cssText = "padding:2px 6px;border-radius:4px;cursor:pointer;user-select:none";
                    option.addEventListener("click", () => {
                        new Int32Array(ctx.memory.buffer, valuePtr, 1)[0] = i;
                        show(i);
                    });
                    row.appendChild(option);
                    return option;
                });
                const read = () => new Int32Array(ctx.memory.buffer, valuePtr, 1)[0];
                const pick = (i) => {
                    new Int32Array(ctx.memory.buffer, valuePtr, 1)[0] = i;
                    show(i);
                };
                show(read());
                addControl(label.textContent, read, pick);
                target().append(label, row);
                group.refold?.();
            },
            js_ui_slider(namePtr, nameLen, min, max, step, log, valuePtr) {
                const name = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const write = (v) => { new Float32Array(ctx.memory.buffer, valuePtr, 1)[0] = v; }; // memory can grow, so take a fresh view
                const read = () => new Float32Array(ctx.memory.buffer, valuePtr, 1)[0];
                const set = (v) => {
                    write(v);
                    input.value = toPosition(v);
                    shown.textContent = v.toFixed(digits);
                };
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
                input.toPosition = toPosition; // for the headless tests
                const shown = document.createElement("span");
                set(read());
                addControl(name, read, set);

                input.addEventListener("input", () => {
                    const v = fromPosition(Number(input.value));
                    write(v);
                    shown.textContent = v.toFixed(digits);
                });
                input.addEventListener("keydown", (e) => e.stopPropagation()); // arrows move the slider, not the game
                target().append(label, input, shown);
                group.refold?.();
            },
        },
    };
}
