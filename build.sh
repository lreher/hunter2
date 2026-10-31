#!/bin/sh
set -e
cd "$(dirname "$0")"
CLANG=/opt/homebrew/opt/llvm/bin/clang
LD=/opt/homebrew/opt/lld/bin/wasm-ld
$CLANG --target=wasm32 -ffreestanding -nostdlib -O2 -Wall -Wextra -Isrc \
  -fuse-ld=$LD -Wl,--no-entry \
  -o web/game.wasm src/main.c
echo "built web/game.wasm ($(wc -c < web/game.wasm) bytes)"
