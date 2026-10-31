#!/bin/sh
# Tools come from PATH. Override with CLANG=/path/to/clang or WASM_LD=/path/to/wasm-ld.
set -e
cd "$(dirname "$0")"
CLANG=${CLANG:-clang}
$CLANG --target=wasm32 -mbulk-memory -ffreestanding -nostdlib -O2 -Wall -Wextra -Isrc \
  ${WASM_LD:+--ld-path="$WASM_LD"} -Wl,--no-entry \
  -o web/game.wasm src/main.c
echo "built web/game.wasm ($(wc -c < web/game.wasm) bytes)"
