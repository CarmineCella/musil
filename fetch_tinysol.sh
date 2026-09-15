#!/usr/bin/env bash
# fetch_tinysol.sh — download the full TinySOL (about 800 MB of orchestral samples with its feature
# file) into datasets/, a folder git ignores, next to the tree. Nothing in examples/ changes: the
# examples run on the bundled MicroSOL (examples/data/microsol) and each has a commented line to
# switch to ../datasets/TinySOL.spectrum.db. Any other *SOL database (OrchideaSOL, FullSOL) can be
# put in datasets/ by hand in the same layout: X.spectrum.db next to a folder X/ with the sounds.
# Usage: ./fetch_tinysol.sh [URL]
set -euo pipefail
cd "$(dirname "$0")"
URL="${1:-https://github.com/CarmineCella/musil/releases/download/data/TinySOL.zip}"
DEST="datasets"
mkdir -p "$DEST"
if [ -f "$DEST/TinySOL.spectrum.db" ]; then echo "$DEST/TinySOL.spectrum.db is already there; delete it to fetch again"; exit 0; fi
echo "==> Downloading TinySOL from $URL"
TMP="$(mktemp -d)"
curl -L --progress-bar -o "$TMP/TinySOL.zip" "$URL"
echo "==> Unpacking into $DEST/"
unzip -q "$TMP/TinySOL.zip" -d "$TMP/unz"
# the zip holds TinySOL.spectrum.db and TinySOL/ at its top level, or inside one folder
if [ -f "$TMP/unz/TinySOL.spectrum.db" ]; then cp -R "$TMP/unz/." "$DEST/"; else cp -R "$TMP/unz/"*/. "$DEST/"; fi
rm -rf "$TMP"
echo "==> Done: $(find "$DEST/TinySOL" -name '*.wav' | wc -l | tr -d ' ') sounds in $DEST/TinySOL, features in $DEST/TinySOL.spectrum.db"
echo "    in a program: (db-load \"../datasets/TinySOL.spectrum.db\") from examples/, or the absolute path from anywhere"
