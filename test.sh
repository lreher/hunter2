#!/bin/sh
set -e
cd "$(dirname "$0")"
CLANG=/opt/homebrew/opt/llvm/bin/clang
LD=/opt/homebrew/opt/lld/bin/wasm-ld
$CLANG --target=wasm32 -ffreestanding -nostdlib -O1 -Wall -Wextra -Isrc \
  -fuse-ld=$LD -Wl,--no-entry \
  -o tests/test.wasm tests/test.c
node tests/run.js
