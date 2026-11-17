#!/usr/bin/env bash
# deploy_windows.sh — build Musil for Windows and package it (run in an MSYS2 MINGW64 shell).
#
#   ./deploy_windows.sh            Release, static, into build-dist-windows/
#
# Produces dist/musil-<version>-windows/ (musil.exe the CLI, musil-ide.exe, lib/ with the .mu
# libraries and help.txt, examples/ with their data, the manual, run.bat) and
# dist/musil-<version>-windows.zip. The executables need no DLLs.
set -euo pipefail
cd "$(dirname "$0")"

VERSION="$(sed -n 's/.*MUSIL_VERSION "\(.*\)".*/\1/p' src/core.h | head -1)"
BUILD_DIR="build-dist-windows"
OUT="dist/musil-$VERSION-windows"

echo "==> Configuring (Release, static)"
cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DMUSIL_TESTS=OFF -DMUSIL_READLINE=OFF
echo "==> Building"
cmake --build "$BUILD_DIR" --target musil-ide musil

echo "==> Collecting into $OUT"
rm -rf "$OUT"; mkdir -p "$OUT/lib"
cp "$BUILD_DIR/musil-ide.exe" "$BUILD_DIR/musil.exe" "$OUT/"
strip "$OUT/musil-ide.exe" "$OUT/musil.exe" 2>/dev/null || true
cp src/*.mu src/help.txt src/hrtf_kemar.csv src/hall_concertgebouw.wav "$OUT/lib/"
mkdir -p "$OUT/examples" && cp -R examples/. "$OUT/examples/"
[[ -f docs/musil_manual.pdf ]] && cp docs/musil_manual.pdf "$OUT/"
cp README.md LICENSE.md "$OUT/"
printf '@echo off\r\nrem the IDE finds lib\\ next to itself; the CLI needs MUSIL_PATH\r\ncd /d "%%~dp0"\r\nset MUSIL_PATH=%%~dp0lib\r\nstart "" musil-ide.exe %%*\r\n' > "$OUT/run.bat"
( cd dist && rm -f "musil-$VERSION-windows.zip" && zip -qr "musil-$VERSION-windows.zip" "musil-$VERSION-windows" )
du -sh "$OUT" "dist/musil-$VERSION-windows.zip" | sed 's/^/    /'
echo "Done: dist/musil-$VERSION-windows.zip"
