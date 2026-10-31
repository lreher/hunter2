# Sets CLANG for build.sh, test.sh and setup.sh. Sourced, not run.
# CLANG already set: used as is. Otherwise clang from PATH if it can build wasm32,
# else Homebrew's LLVM, which is keg only (not on PATH). Apple's /usr/bin/clang has no wasm32 target.
can_build_wasm() { echo 'int x;' | "$1" --target=wasm32 -x c -c -o /dev/null - >/dev/null 2>&1; }
if [ -z "$CLANG" ]; then
    CLANG=clang
    if ! can_build_wasm clang; then
        for c in /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang; do # Apple Silicon, Intel
            if [ -x "$c" ] && can_build_wasm "$c"; then
                CLANG=$c
                break
            fi
        done
    fi
fi
