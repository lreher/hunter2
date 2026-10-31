# focus: first-frame

## Status
DONE. Proved C -> wasm -> JS -> WebGPU. Code has since been restructured (see architecture.md).

Goal: browser page where C clears the screen to a color through WebGPU. Proves C -> wasm -> JS -> GPU.

## Plan
1. Layout, build script, page loads wasm, C logs "hello" via JS. 
2. JS sets up WebGPU. C asks JS to clear to a color.
3. Frame loop: JS calls C every frame, C changes the color over time.

## Layout
- src/main.c: game code (C)
- web/index.html, web/platform.js: our JS glue
- web/game.wasm: build output
- build.sh: builds wasm
- serve.js: tiny node static server (no deps)
- run: node serve.js, open http://localhost:8000

## Log
- 2026-10-09: created focus. Starting step 1.
- 2026-10-09: step 1 built. 371 byte wasm. Node test prints "[c] hello from C". Browser not checked yet.
- 2026-10-09: replaced python server with serve.js (node, no deps). Sets wasm MIME, no-cache, logs each request.
- 2026-10-09: back to serve.js, using require() so node needs no package.json.
- 2026-10-09: step 2 built. C calls js_clear(0.1,0.4,0.8). Node stub confirms the call. Errors now show on page. Browser not checked yet.
- 2026-10-09: step 2 confirmed in browser: blue screen. Next: white 'hello world' text centered.
- 2026-10-09: step 3 built: white "Hello World" centered. C owns an 8x8 bitmap font (only H W e l o r d so far), builds a 128x64 atlas, uploads as r8unorm texture. JS has a quad pipeline (instanced, 48 byte Quad, matches struct in main.c). New C->JS calls: js_screen_width/height, js_upload_texture, js_begin_frame/js_draw_quads/js_end_frame (replaced js_clear). Node stub check: 11 quads, x starts 448, y 418 on a 1600x900 screen, glyph art looks right. Browser not checked yet.
- Known gaps: draws once at init (no frame loop, no resize handling). Text is 8x scale, hardcoded.
- 2026-10-09: confirmed in browser: white "Hello World" on blue. Focus done.
- Note: art style is NOT decided. Don't assume pixel art. Bitmap font was just the quickest proof.
- Next focus: architecture.md (code is in 2 files, needs structure).
