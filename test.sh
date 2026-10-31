#!/bin/sh
# Tools come from PATH. Override with CLANG=/path/to/clang or WASM_LD=/path/to/wasm-ld.
set -e
cd "$(dirname "$0")"
CLANG=${CLANG:-clang}
$CLANG --target=wasm32 -ffreestanding -nostdlib -O1 -Wall -Wextra -Isrc \
  ${WASM_LD:+--ld-path="$WASM_LD"} -Wl,--no-entry \
  -o tests/test.wasm tests/test.c
node tests/run.js
