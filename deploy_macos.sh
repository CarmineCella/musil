#!/bin/sh
# deploy_macos.sh — build Musil for macOS and package it.
#
#   ./deploy_macos.sh            universal (arm64 + x86_64) Release build, then:
#                                  dist/Musil.app        the Listener, with icon, font, libraries, manual
#                                  dist/musil            the command-line interpreter
#                                  dist/lib/             the .mu libraries and help.txt (for ~/.musil)
#                                  dist/Musil-<ver>-macos.zip   all of the above
#   ./deploy_macos.sh --dmg      also dist/Musil-<ver>.dmg
#
# Everything is statically linked; the only requirement on the target Mac is the OS.
# The bundle is ad-hoc signed; for distribution outside your own machines, sign with a
# Developer ID and notarize. The icon comes from docs/icon.png.
set -e
cd "$(dirname "$0")"
BUILD=build-release
DIST=dist
APP=$DIST/Musil.app
VERSION=$(sed -n 's/.*MUSIL_VERSION "\(.*\)".*/\1/p' src/core.h | head -1)

echo "== documentation"
if command -v python3 > /dev/null; then python3 tools/gendoc.py; fi
if command -v pdflatex > /dev/null && [ ! -f docs/musil_manual.pdf ]; then
  (cd docs && pdflatex -interaction=nonstopmode musil_manual.tex > /dev/null && pdflatex -interaction=nonstopmode musil_manual.tex > /dev/null) || true
fi

echo "== building (universal)"
cmake -B $BUILD -DCMAKE_BUILD_TYPE=Release -DMUSIL_TESTS=OFF \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 > /dev/null
cmake --build $BUILD -j"$(sysctl -n hw.ncpu)" --target musil-listener musil

echo "== dist"
rm -rf "$DIST"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/lib" "$DIST/lib"
# NB: the executable inside the bundle is "Musil" and nothing else goes in MacOS/,
# because a case-insensitive file system cannot hold "Musil" and "musil" side by side.
cp $BUILD/musil-listener "$APP/Contents/MacOS/Musil"
cp $BUILD/musil "$DIST/musil"
cp listener/assets/JetBrainsMono-Regular.ttf listener/assets/JetBrainsMono-OFL.txt "$APP/Contents/Resources/"
cp src/*.mu src/help.txt "$APP/Contents/Resources/lib/"
cp src/*.mu src/help.txt "$DIST/lib/"
if [ -f docs/musil_manual.pdf ]; then cp docs/musil_manual.pdf "$APP/Contents/Resources/"; cp docs/musil_manual.pdf "$DIST/"; fi
cp README.md LICENSE.md "$DIST/"

echo "== icon"
ICONSET=$DIST/Musil.iconset
rm -rf "$ICONSET"; mkdir -p "$ICONSET"
for s in 16 32 64 128 256 512; do
  sips -z $s $s docs/icon.png --out "$ICONSET/icon_${s}x${s}.png" > /dev/null
  d=$((s*2)); sips -z $d $d docs/icon.png --out "$ICONSET/icon_${s}x${s}@2x.png" > /dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/Musil.icns"
rm -rf "$ICONSET"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Musil</string>
  <key>CFBundleDisplayName</key><string>Musil</string>
  <key>CFBundleIdentifier</key><string>com.carminecella.musil</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleExecutable</key><string>Musil</string>
  <key>CFBundleIconFile</key><string>Musil</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>LSMinimumSystemVersion</key><string>12.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSMicrophoneUsageDescription</key><string>Musil can analyse sound from the microphone.</string>
  <key>CFBundleDocumentTypes</key><array><dict>
    <key>CFBundleTypeName</key><string>Musil source</string>
    <key>CFBundleTypeExtensions</key><array><string>mu</string></array>
    <key>CFBundleTypeRole</key><string>Viewer</string>
  </dict></array>
</dict></plist>
PLIST

echo "== sign (ad hoc)"
codesign --force --deep --sign - "$APP"
codesign --force --sign - "$DIST/musil"

echo "== zip"
(cd "$DIST" && rm -f "Musil-$VERSION-macos.zip" && zip -qry "Musil-$VERSION-macos.zip" Musil.app musil lib README.md LICENSE.md $( [ -f musil_manual.pdf ] && echo musil_manual.pdf ))

if [ "$1" = "--dmg" ]; then
  echo "== dmg"
  rm -f "$DIST/Musil-$VERSION.dmg"
  hdiutil create -volname Musil -srcfolder "$APP" -ov -format UDZO "$DIST/Musil-$VERSION.dmg" > /dev/null
fi
lipo -info "$APP/Contents/MacOS/Musil" 2>/dev/null || true
echo "done: $DIST"
