#!/bin/sh
# Gets a fresh clone ready: checks tools, clones the book's source, extracts the book text.
# Safe to run again, it only does what is missing.
cd "$(dirname "$0")"
missing=0
# any copy of the book in the repo root, whatever the exact file name
BOOK=$(ls Game\ Physics\ Engine\ Development*.pdf 2>/dev/null | head -n 1)

check_tool() { # command, what provides it
    if command -v "$1" >/dev/null 2>&1; then
        echo "ok       $1"
    else
        echo "MISSING  $1 (install $2, it must be on PATH)"
        missing=1
    fi
}
check_tool node node
check_tool pdftotext poppler

# build a tiny wasm the same way build.sh does: catches a clang without the wasm32 target or a missing wasm-ld
CLANG=${CLANG:-clang}
tmp=$(mktemp -d)
echo 'int f(void) { return 1; }' > "$tmp/t.c"
if "$CLANG" --target=wasm32 -mbulk-memory -ffreestanding -nostdlib ${WASM_LD:+--ld-path="$WASM_LD"} -Wl,--no-entry -Wl,--export=f \
    -o "$tmp/t.wasm" "$tmp/t.c" >/dev/null 2>&1; then
    echo "ok       clang + wasm-ld (wasm32 build)"
else
    echo "MISSING  clang + wasm-ld that can build wasm32 (install LLVM clang and lld, on PATH or set CLANG / WASM_LD)"
    missing=1
fi
rm -rf "$tmp"

if [ -d reference/cyclone-physics ]; then
    echo "ok       reference/cyclone-physics"
else
    git clone -q https://github.com/idmillington/cyclone-physics.git reference/cyclone-physics && echo "cloned   reference/cyclone-physics"
fi

# the book is copyrighted, so it is not in git and cannot be downloaded: the user copies it in by hand
if [ -z "$BOOK" ]; then
    echo "MISSING  Game Physics Engine Development*.pdf (copy it into the repo root by hand)"
    missing=1
elif command -v pdftotext >/dev/null 2>&1; then
    if [ -f reference/millington.txt ]; then
        echo "ok       reference/millington.txt"
    else
        pdftotext -layout "$BOOK" reference/millington.txt && echo "made     reference/millington.txt"
    fi
    if [ -f reference/contents.txt ]; then
        echo "ok       reference/contents.txt"
    else
        pdftotext -layout -f 8 -l 16 "$BOOK" - | tr '\f' '\n' > reference/contents.txt && echo "made     reference/contents.txt"
    fi
fi

# commits go out as the user's GitHub noreply address (keeps the real email private)
if [ -z "$(git config user.email)" ]; then
    git config user.name lreher
    git config user.email 53959287+lreher@users.noreply.github.com
    echo "set      git identity (repo only)"
fi

if [ $missing = 0 ]; then
    echo "setup done"
else
    echo "setup not complete, see MISSING above"
    exit 1
fi
