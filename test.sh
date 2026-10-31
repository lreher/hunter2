#!/bin/sh
# Finds clang with find_clang.sh. Override with CLANG=/path/to/clang or WASM_LD=/path/to/wasm-ld.
set -e
cd "$(dirname "$0")"
. ./find_clang.sh
$CLANG --target=wasm32 -mbulk-memory -ffreestanding -nostdlib -O1 -Wall -Wextra -Isrc \
  ${WASM_LD:+--ld-path="$WASM_LD"} -Wl,--no-entry \
  -o tests/test.wasm tests/test.c
node tests/run.js
