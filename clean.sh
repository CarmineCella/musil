#!/usr/bin/env bash
# clean.sh — back to a fresh-clone state: every build directory, every packaged output, and the
# generated documentation.
#
#   ./clean.sh           remove build/, every build-*/ (debug, dist, dist-linux, ...), dist/,
#                        src/help.txt, docs/generated/, docs/musil_manual.pdf and LaTeX leftovers
#   ./clean.sh --dry     only show what would be removed
#
# Never touches sources or examples/data. The generated documentation comes back
# at the next configure (help.txt, docs/generated/) and with  ./build.sh musil-docs  (the PDF).
set -euo pipefail
cd "$(dirname "$0")"

found=()
for d in build build-* dist docs/generated src/help.txt docs/musil_manual.pdf \
         docs/musil_manual.aux docs/musil_manual.log docs/musil_manual.out docs/musil_manual.toc; do
    [[ -e "$d" ]] && found+=("$d")
done
if [[ ${#found[@]} -eq 0 ]]; then echo "already clean"; exit 0; fi

if [[ "${1:-}" == "--dry" ]]; then
    echo "would remove:"; du -sh "${found[@]}" | sed 's/^/    /'
    exit 0
fi
echo "removing:"; du -sh "${found[@]}" | sed 's/^/    /'
rm -rf "${found[@]}"
find . -name .DS_Store -not -path './.git/*' -delete 2>/dev/null || true
echo "done — next: ./build.sh"
