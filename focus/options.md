# focus: options

## Status
DONE. Final picks: C, browser only, clang straight to wasm, own JS platform layer, WebGPU. Architecture continues in architecture.md.

Goal: pick the tech foundation for a 2D PC game, engine from scratch.

## Decisions
- Language: C
- Target: browser only for now (was macOS first, Windows later)
- Platform layer: our own small JS file (no SDL, no Emscripten)
- Toolchain: clang straight to wasm
- Rendering: WebGPU (for flexibility)

## Open questions
(none)

## Log
- 2026-10-09: created project, brain, and this focus file.
- 2026-10-09: picked C.
- 2026-10-09: picked macOS first, Windows later.
- 2026-10-09: picked SDL for platform layer. Added third party rule to brain.
- 2026-10-09: switched target to browser only for now. Rendering narrowed to software or WebGPU.
- 2026-10-09: picked raw clang wasm + own JS. Dropped SDL.
- 2026-10-09: picked WebGPU. All foundation questions answered.
- 2026-10-09: installed brew llvm + lld. Verified: C -> wasm32 compiles, add(2,3)=5 in node. clang at /opt/homebrew/opt/llvm/bin/clang, linker at /opt/homebrew/opt/lld/bin/wasm-ld.
