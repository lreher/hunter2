#!/bin/sh
# Gets a fresh clone ready: checks tools, clones the book's source, extracts the book text.
# Safe to run again, it only does what is missing.
cd "$(dirname "$0")"
missing=0
BOOK="Game Physics Engine Development - Ian Millington.pdf"

check_tool() { # name, path or command, how to get it
    if [ -x "$2" ] || command -v "$2" >/dev/null 2>&1; then
        echo "ok       $1"
    else
        echo "MISSING  $1 ($3)"
        missing=1
    fi
}
check_tool clang /opt/homebrew/opt/llvm/bin/clang "brew install llvm"
check_tool wasm-ld /opt/homebrew/opt/lld/bin/wasm-ld "brew install lld"
check_tool node node "brew install node"
check_tool pdftotext pdftotext "brew install poppler"

if [ -d reference/cyclone-physics ]; then
    echo "ok       reference/cyclone-physics"
else
    git clone -q https://github.com/idmillington/cyclone-physics.git reference/cyclone-physics && echo "cloned   reference/cyclone-physics"
fi

# the book is copyrighted, so it is not in git and cannot be downloaded: the user copies it in by hand
if [ ! -f "$BOOK" ]; then
    echo "MISSING  $BOOK (copy it into the repo root by hand)"
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
