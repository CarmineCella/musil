#!/usr/bin/env bash
# build.sh — everyday native build.
#
#   ./build.sh                 configure (Release) and build everything into build/
#   ./build.sh musil           build just one target (musil, musil-listener, musil-docs)
#   ./build.sh --debug         Debug build into build-debug/ (symbols, no optimization)
#   ./build.sh --clean         delete the build directory first and start over
#   ./build.sh --test          build, then run the test suite (ctest)
#   ./build.sh --run FILE.mu   build, then run a Musil file with the CLI
#   ./build.sh --listener      build, then open the Listener
#   ./build.sh --no-raylib     language and libraries only, no dependency (no plot, no Listener),
#                              into build-noraylib/
#   ./build.sh --docs          build, then regenerate the documentation and the PDF manual
#
# Afterwards a plain  cmake --build build -j  does the same as ./build.sh.
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="build"; TYPE="Release"; TARGET=""; RUN=""; TEST=""; LISTENER=""; RAYLIB="ON"; CLEAN=""; DOCS=""
args=("$@")
for ((i=0; i<${#args[@]}; i++)); do
    arg="${args[$i]}"
    case "$arg" in
        --debug)     BUILD_DIR="build-debug"; TYPE="Debug" ;;
        --clean)     CLEAN=1 ;;
        --test)      TEST=1 ;;
        --listener)  LISTENER=1 ;;
        --docs)      DOCS=1 ;;
        --no-raylib) RAYLIB="OFF"; BUILD_DIR="build-noraylib" ;;
        --run)       i=$((i+1)); RUN="${args[$i]:-}"; [[ -n "$RUN" ]] || { echo "--run needs a .mu file" >&2; exit 1; } ;;
        -h|--help)   sed -n '2,15p' "$0"; exit 0 ;;
        -*)          echo "unknown option $arg (try --help)" >&2; exit 1 ;;
        *)           TARGET="$arg" ;;
    esac
done
[[ -n "$CLEAN" ]] && { echo "==> Removing $BUILD_DIR"; rm -rf "$BUILD_DIR"; }

# Always (re)configure: it is a no-op when nothing changed, it regenerates help.txt from the
# source comments, and it repairs a build directory left half-made by a failed first configure.
echo "==> Configuring ($TYPE, raylib $RAYLIB) into $BUILD_DIR"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$TYPE" -DMUSIL_RAYLIB="$RAYLIB"
echo "==> Building ${TARGET:-all}"
cmake --build "$BUILD_DIR" -j ${TARGET:+--target "$TARGET"}

if [[ -n "$DOCS" ]]; then
    echo "==> Documentation"
    cmake --build "$BUILD_DIR" --target musil-docs
fi
if [[ -n "$TEST" ]]; then
    echo "==> Testing"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
if [[ -n "$RUN" ]]; then
    echo "==> Running $RUN"
    MUSIL_PATH="$PWD/src" "./$BUILD_DIR/musil" "$RUN"
fi
if [[ -n "$LISTENER" ]]; then
    [[ "$RAYLIB" == "ON" ]] || { echo "--listener needs raylib (drop --no-raylib)" >&2; exit 1; }
    echo "==> Opening the Listener"
    "./$BUILD_DIR/musil-listener"
fi
