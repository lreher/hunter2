// Debug panel: sliders that C declares. Each slider writes its value straight into C's
// memory (an f32 at the pointer C gave), so C just reads its own variable, no calls per frame.

const decoder = new TextDecoder();
const encoder = new TextEncoder();

export function createUi(ctx) {
    let panel = null;
    let title = null;
    let bar = null;      // demo bar along the bottom
    const tabs = [];     // one per demo, in C's order
    const demoIds = [];
    let pickedPtr = 0;   // C's i32 that a click writes the tab index into
    const text = (ptr, len) => decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));
    const writeI32 = (ptr, value) => { new Int32Array(ctx.memory.buffer, ptr, 1)[0] = value; }; // memory can grow: a fresh view

    let tree = null;     // the panel's folds, above the save / load row
    let folds = [];      // the open folds, outermost first: {name, body}. new controls go into the last one's body
    let demoId = "";     // the running demo, for its saved settings
    let controls = [];   // this panel's sliders, choices and texts: {key, name, text, value(), set(v)}
    let seen = new Set(); // keys that got their saved value already, so a panel built again keeps what is set now
    let keepScroll = 0;   // the tree's scroll, kept when C builds the panel again
    let block = null;     // the open block's body (js_ui_block): rows go there while it is open

    // Saved settings: one entry per demo in localStorage, {key: value}. key = "fold/name": the name of the fold the
    // control is in, then its own, so two folds can have sliders with the same name. Older saves used the bare name:
    // still read when the full key is missing. A demo starts with its last save (each control picks its value up as C
    // declares it), load puts the last save back. Storage can be missing or blocked (private window), then nothing is kept.
    const storageKey = () => `hunter2.settings.${demoId}`;
    function loadSaved() {
        try { return JSON.parse(localStorage.getItem(storageKey()) || "{}"); } catch { return {}; }
    }
    function saveAll() {
        const values = {};
        for (const control of controls) values[control.key] = control.value();
        try {
            localStorage.setItem(storageKey(), JSON.stringify(values));
            console.log(`[js] settings: saved ${Object.keys(values).length} values for ${demoId} at ${location.origin}`);
        } catch (e) { console.warn(`[js] settings: not saved, storage failed: ${e}`); }
    }
    function savedValue(saved, control) {
        const type = control.text ? "string" : "number";
        if (typeof saved[control.key] === type) return saved[control.key];
        if (typeof saved[control.name] === type) return saved[control.name]; // older save, bare name
        return undefined;
    }
    // controls missing from the save (added or renamed since) keep their value. a loaded parts list makes C build
    // the panel again: everything in it gets its saved value again then (seen is emptied)
    function loadAll() {
        resetting = false;
        const saved = loadSaved();
        for (const control of controls) {
            const value = savedValue(saved, control);
            if (value !== undefined) control.set(value);
        }
        seen = new Set();
    }

    // Defaults: the code's values, per demo, {key: value}. A control's value the first time its key shows up in this
    // page, before any save is put on it. Reset puts them all back, the parts list too (its types and who uses which).
    // The save stays as it was: load brings it back, save keeps the defaults. resetting: until load or another demo,
    // controls that show up when C builds the panel again take their default instead of the save
    const defaultsByDemo = {};
    const demoDefaults = () => (defaultsByDemo[demoId] ??= {});
    let resetting = false;
    function resetAll() {
        resetting = true;
        const defaults = demoDefaults();
        for (const control of controls) {
            const value = defaults[control.key];
            if (value !== undefined && value !== control.value()) control.set(value);
        }
        seen = new Set();
    }

    // a control picks up its saved value (or its default after reset) the first time its key shows up for this demo
    // (not when C builds the panel again after a click: then the value C has now stays)
    function addControl(name, value, set, isText = false) {
        const foldName = folds.length ? folds[folds.length - 1].name : "";
        const control = { key: foldName ? `${foldName}/${name}` : name, name, text: isText, value, set };
        controls.push(control);
        const defaults = demoDefaults();
        if (!(control.key in defaults)) defaults[control.key] = value();
        if (seen.has(control.key)) return;
        seen.add(control.key);
        if (resetting) {
            if (defaults[control.key] !== value()) set(defaults[control.key]);
            return;
        }
        const saved = savedValue(loadSaved(), control);
        if (saved !== undefined && !(isText && saved === value())) set(saved);
    }

    // The panel: a tree of folds along the right edge, above the demo bar, save, load and reset at the bottom. Hidden on load,
    // so the page opens on the game. Shows when Esc lets the mouse go, hides again while the mouse is locked (flying).
    // panelOpen outlives the panel itself: switching demos makes a new one, it stays open
    let panelOpen = false;
    const showPanel = () => { if (panel) panel.style.display = panelOpen ? "flex" : "none"; };
    document.addEventListener?.("pointerlockchange", () => { panelOpen = !document.pointerLockElement; showPanel(); showOverlays(); });

    // Page text over the game, in the browser's monospace font (what the loading screen shows until Tiny5 arrives, the
    // user liked it), clicks go through to the canvas: the controls guide in the top right corner (hidden while the
    // panel is open, it would cover it) and a hint in the middle (hidden while flying)
    let guide = null;
    let hint = null;
    const OVERLAY_STYLE = "position:fixed;font-family:monospace;pointer-events:none;user-select:none;white-space:pre";
    const showOverlays = () => {
        if (guide) guide.style.display = panelOpen ? "none" : "grid";
        if (hint) hint.style.display = document.pointerLockElement ? "none" : "block";
    };

    // Panel width: drag its left edge. Remembered for all demos in localStorage (hunter2.panel.width, CSS pixels)
    const widthKey = "hunter2.panel.width";
    const PANEL_MIN_WIDTH = 280;
    const clampWidth = (width) => Math.max(PANEL_MIN_WIDTH, Math.min(width, innerWidth - 32));
    let panelWidth = 600;
    try { panelWidth = Number(localStorage.getItem(widthKey)) || 600; } catch { }
    function addWidthHandle() {
        const handle = document.createElement("div");
        handle.style.cssText = "position:absolute;left:-4px;top:0;bottom:0;width:8px;cursor:ew-resize";
        handle.addEventListener("pointerdown", (e) => {
            e.preventDefault(); // no text selection while dragging
            handle.setPointerCapture(e.pointerId); // keeps the moves coming when the mouse leaves the thin handle
            const move = (m) => {
                panelWidth = clampWidth(innerWidth - m.clientX);
                if (panel) panel.style.width = `${panelWidth}px`;
            };
            const up = () => {
                handle.removeEventListener("pointermove", move);
                handle.removeEventListener("pointerup", up);
                try { localStorage.setItem(widthKey, String(panelWidth)); } catch { }
            };
            handle.addEventListener("pointermove", move);
            handle.addEventListener("pointerup", up);
        });
        panel.appendChild(handle);
    }
    function makePanel() {
        panel = document.createElement("div");
        panel.style.cssText = `position:fixed;top:0;right:0;bottom:40px;width:${panelWidth}px;max-width:calc(100vw - 32px);` +
            "display:flex;flex-direction:column;background:rgba(10,12,20,0.88);color:#ccc;font:12px monospace;" +
            "border-left:1px solid rgba(255,255,255,0.08)";
        tree = document.createElement("div");
        tree.style.cssText = "flex:1;overflow-y:auto;padding:8px 10px";
        panel.appendChild(tree);
        addWidthHandle();
        document.body.appendChild(panel);
        showPanel();
        const scroll = keepScroll; // after C has added everything, this frame
        if (scroll) setTimeout(() => { if (tree) tree.scrollTop = scroll; }, 0);

        const buttons = document.createElement("div");
        buttons.style.cssText = "display:flex;gap:6px;padding:8px 10px;border-top:1px solid rgba(255,255,255,0.08)";
        for (const [label, action] of [["save", saveAll], ["load", loadAll], ["reset", resetAll]]) {
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

    // Open folds: remembered per demo on every click (not part of save / load), {path: true} in localStorage.
    // path = the fold names from the top, "ship/actuators". Everything starts folded
    const openKey = () => `hunter2.open.${demoId}`;
    function loadOpen() {
        try { return JSON.parse(localStorage.getItem(openKey()) || "{}"); } catch { return {}; }
    }
    function saveOpen(path, open) {
        const opened = loadOpen();
        if (open) opened[path] = true; else delete opened[path];
        try { localStorage.setItem(openKey(), JSON.stringify(opened)); } catch { }
    }

    // where the next row goes: the open block, the open fold's body, or the top of the tree
    function target() {
        if (!panel) makePanel();
        if (block) return block;
        return folds.length ? folds[folds.length - 1].body : tree;
    }

    // Blocks: small square groups in a grid that wraps (as many side by side as fit, then the next row). Light lines
    // between them: each block draws its right and bottom edge, the grid its top and left
    const LINE = "1px solid rgba(255,255,255,0.12)";
    function openBlock(name) {
        const parent = target();
        let grid = parent.children[parent.children.length - 1];
        if (!grid || !grid.isBlockGrid) { // the blocks right after each other share one grid
            grid = document.createElement("div");
            grid.isBlockGrid = true;
            grid.style.cssText = `display:grid;grid-template-columns:repeat(auto-fill,minmax(250px,1fr));margin:4px 0 6px;border-top:${LINE};border-left:${LINE}`;
            parent.appendChild(grid);
        }
        const box = document.createElement("div");
        box.style.cssText = `padding:4px 8px 6px;border-right:${LINE};border-bottom:${LINE}`;
        const title = document.createElement("div");
        title.textContent = name;
        title.style.cssText = "color:#888;padding-bottom:2px";
        box.appendChild(title);
        grid.appendChild(box);
        block = box;
    }

    // a line: an arrow (folds only), the name, a dim note on the right (fold buttons go after it)
    function makeLine(name, note, arrow) {
        const line = document.createElement("div");
        line.style.cssText = "display:flex;gap:6px;padding:3px 0;align-items:baseline;white-space:nowrap";
        const mark = document.createElement("span");
        mark.style.cssText = "width:10px;flex:none;color:#888";
        mark.textContent = arrow;
        const title = document.createElement("span");
        title.textContent = name;
        const space = document.createElement("span");
        space.style.cssText = "flex:1";
        line.appendChild(mark);
        line.appendChild(title);
        line.appendChild(space);
        if (note) {
            const dim = document.createElement("span");
            dim.textContent = note;
            dim.style.cssText = "padding-left:12px;color:#888;overflow:hidden;text-overflow:ellipsis";
            line.appendChild(dim);
        }
        return { line, mark, title };
    }

    // a small clickable word (fold buttons, + new's options). its click does not fold or unfold the line it is on
    function makeButton(label, action) {
        const button = document.createElement("span");
        button.textContent = label;
        button.style.cssText = "padding:0 4px;color:#8ab;cursor:pointer;user-select:none";
        button.addEventListener("click", (e) => { e.stopPropagation?.(); action(); });
        return button;
    }

    // a fold inside the open one (top = at the top of the tree): a click on its line shows or hides what is in it
    function openFold(name, note, top) {
        if (!panel) makePanel();
        block = null;
        if (top) folds = [];
        const parent = target();
        const path = [...folds.map((fold) => fold.name), name].join("/");
        const { line, mark, title } = makeLine(name, note, "");
        line.style.cursor = "pointer";
        line.style.userSelect = "none";
        if (top) { title.style.color = "#fff"; title.style.fontWeight = "bold"; line.style.paddingTop = "5px"; }
        const body = document.createElement("div");
        body.style.cssText = "padding-left:14px";
        let open = loadOpen()[path] === true;
        const show = () => {
            mark.textContent = open ? "▾" : "▸";
            body.style.display = open ? "" : "none";
        };
        line.addEventListener("click", () => { open = !open; saveOpen(path, open); show(); });
        show();
        parent.appendChild(line);
        parent.appendChild(body);
        folds.push({ name, body, line, title });
    }

    // a control's row: label, then the control and its value (or the control over both columns)
    function controlRow() {
        const row = document.createElement("div");
        row.style.cssText = "display:grid;grid-template-columns:minmax(0,1fr) minmax(60px,130px) 56px;gap:8px;align-items:center;" +
            (block ? "padding:2px 0" : "padding:2px 0 2px 16px");
        target().appendChild(row);
        return row;
    }

    return {
        env: {
            // drops the whole panel, the next slider or picker makes a new one
            js_ui_clear() {
                block = null;
                keepScroll = tree ? tree.scrollTop : 0;
                if (panel) panel.remove();
                panel = null;
                tree = null;
                folds = [];
                controls = [];
            },
            // a fold at the top of the tree (ends any open ones). later controls go into it
            js_ui_group(namePtr, nameLen) {
                openFold(text(namePtr, nameLen), "", true);
            },
            // a fold inside the open one, with a dim note on the right
            js_ui_fold(namePtr, nameLen, notePtr, noteLen) {
                openFold(text(namePtr, nameLen), text(notePtr, noteLen), false);
            },
            js_ui_fold_end() {
                folds.pop();
                block = null;
            },
            // a block in the open fold: the controls after it go into it until js_ui_block_end
            js_ui_block(namePtr, nameLen) {
                openBlock(text(namePtr, nameLen));
            },
            js_ui_block_end() {
                block = null;
            },
            // a line with nothing to open: a name and a dim note
            js_ui_row(namePtr, nameLen, notePtr, noteLen) {
                target().appendChild(makeLine(text(namePtr, nameLen), text(notePtr, noteLen), "").line);
            },
            // a clickable line. no options: a click writes value into C's i32. options "a|b": a click shows them after
            // the label, picking option i writes value + i
            js_ui_button(labelPtr, labelLen, optionsPtr, optionsLen, targetPtr, value) {
                const line = document.createElement("div");
                line.style.cssText = "display:flex;gap:6px;padding:3px 0;align-items:baseline;white-space:nowrap";
                const mark = document.createElement("span");
                mark.style.cssText = "width:10px;flex:none";
                line.appendChild(mark);
                const options = optionsLen ? text(optionsPtr, optionsLen).split("|") : [];
                const choices = document.createElement("span");
                choices.style.cssText = "display:none";
                options.forEach((name, i) => choices.appendChild(makeButton(name, () => {
                    writeI32(targetPtr, value + i);
                    choices.style.display = "none";
                })));
                line.appendChild(makeButton(text(labelPtr, labelLen), () => {
                    if (!options.length) writeI32(targetPtr, value);
                    else choices.style.display = choices.style.display === "none" ? "" : "none";
                }));
                line.appendChild(choices);
                target().appendChild(line);
            },
            // a row of options like js_ui_choice, picked one green. a click on option i writes value + i. not saved
            js_ui_pick(namePtr, nameLen, optionsPtr, optionsLen, picked, targetPtr, value) {
                const label = document.createElement("span");
                label.textContent = text(namePtr, nameLen);
                const row = document.createElement("div");
                row.style.cssText = "grid-column:span 2;display:flex;flex-wrap:wrap;gap:4px";
                const options = text(optionsPtr, optionsLen).split("|").map((name, i) => {
                    const option = document.createElement("div");
                    option.textContent = name;
                    option.style.cssText = "padding:2px 6px;border-radius:4px;cursor:pointer;user-select:none";
                    option.addEventListener("click", () => { writeI32(targetPtr, value + i); show(i); });
                    row.appendChild(option);
                    return option;
                });
                const show = (chosen) => options.forEach((option, i) => {
                    option.style.color = i === chosen ? "#4f4" : "";
                    option.style.background = i === chosen ? "rgba(68,255,68,0.12)" : "";
                });
                show(picked);
                controlRow().append(label, row);
            },
            // a small word on the open fold's line, right side. a click writes value into C's i32
            js_ui_fold_button(labelPtr, labelLen, targetPtr, value) {
                const fold = folds[folds.length - 1];
                if (fold) fold.line.appendChild(makeButton(text(labelPtr, labelLen), () => writeI32(targetPtr, value)));
            },
            // a double click on the open fold's name makes it a text field. Enter (or click away) writes the typed name
            // into C's buffer (ends with 0, cut to fit), then value into C's i32. Esc puts it back
            js_ui_fold_rename(namePtr, capacity, targetPtr, value) {
                const fold = folds[folds.length - 1];
                if (!fold) return;
                const title = fold.title;
                title.addEventListener("dblclick", (e) => {
                    e.stopPropagation?.();
                    const old = title.textContent;
                    const input = document.createElement("input");
                    input.type = "text";
                    input.value = old;
                    input.style.cssText = "font:inherit;color:inherit;background:transparent;width:160px;" +
                        "border:1px solid rgba(255,255,255,0.3);border-radius:3px;padding:0 3px;outline:none";
                    let done = false;
                    const finish = (keep) => {
                        if (done) return;
                        done = true;
                        const name = input.value.trim();
                        title.textContent = old;
                        if (!keep || !name || name === old) return;
                        const bytes = encoder.encode(name).subarray(0, capacity - 1);
                        const buffer = new Uint8Array(ctx.memory.buffer, namePtr, capacity);
                        buffer.set(bytes);
                        buffer[bytes.length] = 0;
                        writeI32(targetPtr, value);
                    };
                    input.addEventListener("keydown", (e) => {
                        e.stopPropagation(); // typed keys are not the game's
                        if (e.key === "Enter") finish(true);
                        if (e.key === "Escape") finish(false);
                    });
                    input.addEventListener("blur", () => finish(true));
                    input.addEventListener("click", (e) => e.stopPropagation?.()); // clicks in it do not fold
                    title.textContent = "";
                    title.appendChild(input);
                    input.focus?.();
                    input.select?.();
                });
            },
            // a color picker: 3 floats 0..1 in C (r, g, b), saved as "#rrggbb"
            js_ui_color(namePtr, nameLen, rgbPtr) {
                const name = text(namePtr, nameLen);
                const read = () => {
                    const rgb = new Float32Array(ctx.memory.buffer, rgbPtr, 3);
                    const hex = (v) => Math.round(Math.min(1, Math.max(0, v)) * 255).toString(16).padStart(2, "0");
                    return "#" + hex(rgb[0]) + hex(rgb[1]) + hex(rgb[2]);
                };
                const set = (value) => {
                    if (!/^#[0-9a-f]{6}$/i.test(value)) return;
                    const rgb = new Float32Array(ctx.memory.buffer, rgbPtr, 3);
                    for (let i = 0; i < 3; i++) rgb[i] = parseInt(value.slice(1 + 2 * i, 3 + 2 * i), 16) / 255;
                    input.value = value;
                };
                const label = document.createElement("span");
                label.textContent = name;
                const input = document.createElement("input");
                input.type = "color";
                input.value = read();
                input.style.cssText = "grid-column:span 2;width:100%;height:18px;padding:0;border:none;background:transparent;cursor:pointer";
                input.addEventListener("input", () => set(input.value));
                addControl(name, read, set, true);
                controlRow().append(label, input);
            },
            // a text saved and loaded with the settings, not shown. setting it from a save writes it into C's buffer
            // (ends with 0, cut to fit) and 1 into C's changed flag
            js_ui_text(namePtr, nameLen, textPtr, capacity, changedPtr) {
                const read = () => {
                    const bytes = new Uint8Array(ctx.memory.buffer, textPtr, capacity);
                    const end = bytes.indexOf(0);
                    return decoder.decode(bytes.slice(0, end < 0 ? capacity : end));
                };
                const set = (value) => {
                    const bytes = encoder.encode(value).subarray(0, capacity - 1);
                    const buffer = new Uint8Array(ctx.memory.buffer, textPtr, capacity);
                    buffer.set(bytes);
                    buffer[bytes.length] = 0;
                    writeI32(changedPtr, 1);
                };
                addControl(text(namePtr, nameLen), read, set, true);
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
                seen = new Set(); // a demo (also the same one again) starts with its saved values
                resetting = false;
                keepScroll = 0;
                tabs.forEach((tab, i) => {
                    tab.style.color = i === index ? "#4f4" : "";
                    tab.style.background = i === index ? "rgba(68,255,68,0.12)" : "";
                });
                tabs[index].scrollIntoView({ block: "nearest", inline: "nearest" });
                demoId = demoIds[index];
                console.log(`[js] settings: ${Object.keys(loadSaved()).length} saved values for ${demoId} at ${location.origin}`);
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
            // two columns, so the actions line up: keys light gray, actions darker,
            // the panel's own "esc: dev menu" green at the end
            js_ui_controls(ptr, len) {
                guide?.remove();
                guide = null;
                const lines = text(ptr, len).split("\n").filter((line) => line);
                if (!lines.length) return;
                guide = document.createElement("div");
                guide.style.cssText = OVERLAY_STYLE + ";top:24px;right:12px;display:grid;grid-template-columns:auto auto;" +
                    "column-gap:0.5em;font-size:13px;line-height:20px";
                const add = (key, action, keyColor, actionColor) => {
                    for (const [content, color] of [[`${key}:`, keyColor], [action, actionColor]]) {
                        const cell = document.createElement("div");
                        cell.textContent = content;
                        cell.style.color = color;
                        guide.appendChild(cell);
                    }
                };
                for (const line of lines) {
                    const [key, action = ""] = line.split("|");
                    add(key, action, "#999ab3", "#6b6b80");
                }
                add("ESC", "dev menu", "#338040", "#338040");
                document.body.appendChild(guide);
                showOverlays();
            },
            js_ui_hint(ptr, len) {
                const content = text(ptr, len);
                if (!content) {
                    hint?.remove();
                    hint = null;
                    return;
                }
                if (!hint) {
                    hint = document.createElement("div");
                    hint.style.cssText = OVERLAY_STYLE + ";left:50%;top:50%;transform:translate(-50%,-50%);" +
                        "font-size:20px;color:#999ab3";
                    document.body.appendChild(hint);
                }
                hint.textContent = content;
                showOverlays();
            },
            // logarithmic (log = 1): equal slider distance multiplies the value by the same amount, min must be > 0
            // a button that picks an image file and puts it into a texture. stays in this browser tab, never uploaded
            js_ui_image(namePtr, nameLen, texture) {
                const label = document.createElement("span");
                label.textContent = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const input = document.createElement("input");
                input.type = "file";
                input.accept = "image/*";
                input.style.cssText = "grid-column:span 2;font:inherit;color:inherit;min-width:0";
                input.addEventListener("change", () => {
                    const file = input.files[0];
                    if (!file) return;
                    ctx.gpuLoadImage(texture, file)
                        .then(() => console.log(`[js] image ${file.name} loaded into texture #${texture}`))
                        .catch((e) => console.error(`[js] image ${file.name} failed: ${e.message}`));
                });
                controlRow().append(label, input);
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
                controlRow().append(label, row);
            },
            js_ui_slider(namePtr, nameLen, min, max, step, log, valuePtr) {
                const name = decoder.decode(new Uint8Array(ctx.memory.buffer, namePtr, nameLen));
                const write = (v) => { new Float32Array(ctx.memory.buffer, valuePtr, 1)[0] = v; }; // memory can grow, so take a fresh view
                const read = () => new Float32Array(ctx.memory.buffer, valuePtr, 1)[0];
                const set = (v) => {
                    write(v);
                    input.value = toPosition(v);
                    shown.value = format(v);
                };
                const digits = step >= 1 ? 0 : 2;
                // the slider's digits, more when a typed value has more (0.005 does not show as 0.01)
                const format = (v) => {
                    const text = v.toFixed(digits);
                    if (Math.abs(Number(text) - v) <= 1e-6 * Math.max(1, Math.abs(v))) return text;
                    return String(Number(v.toFixed(4)));
                };
                const LOG_STEPS = 1000;
                const fromPosition = (p) => log ? Math.round(min * (max / min) ** (p / LOG_STEPS) / step) * step : p;
                const toPosition = (v) => { // a typed value past the ends puts the slider at that end
                    const p = log ? LOG_STEPS * Math.log(v / min) / Math.log(max / min) : v;
                    return Number.isFinite(p) ? p : 0;
                };

                const label = document.createElement("span");
                label.textContent = name;
                const input = document.createElement("input");
                input.type = "range";
                input.min = log ? 0 : min;
                input.max = log ? LOG_STEPS : max;
                input.step = log ? 1 : step;
                input.toPosition = toPosition; // for the headless tests
                // the value is a text field too: click, type a number, Enter (or click away) sets it, Esc puts it back.
                // not held to the slider's range
                const shown = document.createElement("input");
                shown.type = "text";
                set(read());
                addControl(name, read, set);

                input.addEventListener("input", () => {
                    const v = fromPosition(Number(input.value));
                    write(v);
                    shown.value = format(v);
                });
                input.addEventListener("keydown", (e) => e.stopPropagation()); // arrows move the slider, not the game
                shown.addEventListener("keydown", (e) => {
                    e.stopPropagation(); // typed keys are not the game's
                    if (e.key === "Enter") shown.blur();
                    if (e.key === "Escape") { shown.value = format(read()); shown.blur(); }
                });
                shown.addEventListener("change", () => { // Enter or click away, after an edit
                    const v = Number(shown.value.trim());
                    if (shown.value.trim() !== "" && Number.isFinite(v)) set(v);
                    else shown.value = format(read());
                });
                input.style.cssText = "width:100%;min-width:0";
                shown.style.cssText = "width:100%;min-width:0;box-sizing:border-box;text-align:right;font:inherit;color:inherit;" +
                    "background:transparent;border:1px solid rgba(255,255,255,0.15);border-radius:3px;padding:0 3px;outline:none";
                controlRow().append(label, input, shown);
            },
        },
    };
}
