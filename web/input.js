// Keyboard and mouse state for C to poll. Key order must match the KEY_ enum in src/platform/input.h.
import { fail } from "./env.js";

const KEY_NAMES = ["Space", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "KeyW", "KeyA", "KeyS", "KeyD", "KeyQ", "KeyE", "ShiftLeft", "KeyO", "BracketLeft", "BracketRight", "KeyR", "Backspace", "ControlLeft", "MouseRight", "MouseLeft"];
const KEY_ALIASES = { ShiftRight: "ShiftLeft", ControlRight: "ControlLeft" }; // both shift keys count as one, both control keys too
const MOUSE_RIGHT = KEY_NAMES.indexOf("MouseRight"); // the right mouse button, held like a key
const MOUSE_LEFT = KEY_NAMES.indexOf("MouseLeft");   // the left one too, but only while the mouse is locked

export function createInput(canvas) {
    const down = new Uint8Array(KEY_NAMES.length);
    const pressed = new Uint8Array(KEY_NAMES.length);
    let mouseDx = 0, mouseDy = 0;

    function keyIndex(code) {
        return KEY_NAMES.indexOf(KEY_ALIASES[code] ?? code);
    }

    window.addEventListener("keydown", (e) => {
        const key = keyIndex(e.code);
        if (key < 0) return;
        e.preventDefault(); // no page scrolling on space and arrows
        if (e.repeat) return;
        down[key] = 1;
        pressed[key] = 1;
    });
    window.addEventListener("keyup", (e) => {
        const key = keyIndex(e.code);
        if (key >= 0) down[key] = 0;
    });
    // we would never see the keyup if the window loses focus while a key is held
    window.addEventListener("blur", () => down.fill(0));

    // left click locks the mouse to the canvas (does nothing while locked), Esc lets it go (the browser does that
    // itself, a page cannot change it). The right button is held like a key (MouseRight), the left one too while
    // locked (MouseLeft: the click that locks does not count). No browser menu on right click over the canvas.
    const locked = () => document.pointerLockElement === canvas;
    canvas.addEventListener("click", () => {
        if (!locked()) canvas.requestPointerLock()?.catch?.(() => {}); // the browser refuses a relock right after Esc, that is fine
    });
    canvas.addEventListener("mousedown", (e) => {
        const key = e.button === 2 ? MOUSE_RIGHT : e.button === 0 && locked() ? MOUSE_LEFT : -1;
        if (key < 0) return;
        if (!down[key]) pressed[key] = 1;
        down[key] = 1;
    });
    window.addEventListener("mouseup", (e) => {
        if (e.button === 2) down[MOUSE_RIGHT] = 0;
        if (e.button === 0) down[MOUSE_LEFT] = 0;
    });
    document.addEventListener?.("pointerlockchange", () => { if (!locked()) down[MOUSE_LEFT] = 0; }); // Esc while firing
    canvas.addEventListener("contextmenu", (e) => e.preventDefault());
    window.addEventListener("mousemove", (e) => {
        if (!locked()) return;
        mouseDx += e.movementX;
        mouseDy += e.movementY;
    });

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
            js_mouse_locked() {
                return locked() ? 1 : 0;
            },
            js_mouse_dx() {
                return mouseDx;
            },
            js_mouse_dy() {
                return mouseDy;
            },
        },
        // call after each game frame, so "pressed" and mouse movement last exactly one frame
        endFrame() {
            pressed.fill(0);
            mouseDx = 0;
            mouseDy = 0;
        },
    };
}
