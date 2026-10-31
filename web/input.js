// Keyboard state for C to poll. Order must match the KEY_ enum in src/platform/input.h.
import { fail } from "./env.js";

const KEY_NAMES = ["Space", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "KeyW", "KeyA", "KeyS", "KeyD"];

export function createInput() {
    const down = new Uint8Array(KEY_NAMES.length);
    const pressed = new Uint8Array(KEY_NAMES.length);

    window.addEventListener("keydown", (e) => {
        const key = KEY_NAMES.indexOf(e.code);
        if (key < 0) return;
        e.preventDefault(); // no page scrolling on space and arrows
        if (e.repeat) return;
        down[key] = 1;
        pressed[key] = 1;
    });
    window.addEventListener("keyup", (e) => {
        const key = KEY_NAMES.indexOf(e.code);
        if (key >= 0) down[key] = 0;
    });
    // we would never see the keyup if the window loses focus while a key is held
    window.addEventListener("blur", () => down.fill(0));

    function check(key) {
        if (key < 0 || key >= KEY_NAMES.length) fail(`input: unknown key ${key}`);
    }

    return {
        env: {
            js_key_down(key) {
                check(key);
                return down[key];
            },
            js_key_pressed(key) {
                check(key);
                return pressed[key];
            },
        },
        // call after each game frame, so "pressed" lasts exactly one frame
        endFrame() {
            pressed.fill(0);
        },
    };
}
