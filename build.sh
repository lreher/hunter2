#!/bin/sh
# Finds clang with find_clang.sh. Override with CLANG=/path/to/clang or WASM_LD=/path/to/wasm-ld.
set -e
cd "$(dirname "$0")"
. ./find_clang.sh
$CLANG --target=wasm32 -mbulk-memory -ffreestanding -nostdlib -O2 -Wall -Wextra -Isrc \
  ${WASM_LD:+--ld-path="$WASM_LD"} -Wl,--no-entry \
  -o web/game.wasm src/main.c
echo "built web/game.wasm ($(wc -c < web/game.wasm) bytes)"
