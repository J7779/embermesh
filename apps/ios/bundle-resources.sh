#!/bin/bash
# Copies the web content into the app bundle (Xcode build phase "Bundle web UI + guides").
#
#   <bundle>/www/          <- ui/                      (the shared web UI; may be absent while in development)
#   <bundle>/www-diag/     <- apps/ios/Diagnostics/    (bridge-test.html: always shipped, fallback start page)
#   <bundle>/guides.pack   <- first existing of: $EMBER_GUIDES_PACK, build/guides.pack (canonical,
#                             `./build.sh guides`), build/guides/guides.pack, dist/guides.pack,
#                             guides/guides.pack, ui/guides.pack
#   <bundle>/hubweb/       <- dist/web/  (`./build.sh web`: the WASM app served to hub guests by the
#                             phone hub; optional — without it guests get the host library's built-in
#                             page). Override with EMBER_HUB_WEB. A guide pack inside it that is
#                             identical to <bundle>/guides.pack is hard-linked instead of copied twice.
#
# Set EMBER_GUIDES_PACK (environment or Xcode build setting) to point at a specific pack.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
DEST="${TARGET_BUILD_DIR:?run from Xcode}/${UNLOCALIZED_RESOURCES_FOLDER_PATH:?}"

mkdir -p "$DEST/www" "$DEST/www-diag"

if [ -d "$REPO/ui" ] && [ -n "$(ls -A "$REPO/ui" 2>/dev/null)" ]; then
  rsync -a --delete --exclude '.DS_Store' --exclude 'node_modules' --exclude '*.md' --exclude 'tests' \
    --exclude 'guides.pack' "$REPO/ui/" "$DEST/www/"
  [ -f "$REPO/ui/index.html" ] || echo "warning: ui/index.html not found; the app will start with the diagnostic page"
else
  rm -rf "$DEST/www"/*
  echo "warning: ui/ is empty; bundling only the diagnostic page"
fi

rsync -a --delete "$HERE/Diagnostics/" "$DEST/www-diag/"

PACK=""
for c in "${EMBER_GUIDES_PACK:-}" "$REPO/build/guides.pack" "$REPO/build/guides/guides.pack" "$REPO/dist/guides.pack" \
         "$REPO/guides/guides.pack" "$REPO/ui/guides.pack"; do
  if [ -n "$c" ] && [ -f "$c" ]; then PACK="$c"; break; fi
done
if [ -n "$PACK" ]; then
  if ! cmp -s "$PACK" "$DEST/guides.pack"; then cp "$PACK" "$DEST/guides.pack"; fi
  echo "bundled guides.pack from $PACK ($(wc -c < "$PACK" | tr -d ' ') bytes)"
else
  rm -f "$DEST/guides.pack"
  echo "note: no guides.pack found (set EMBER_GUIDES_PACK); the offline library will be empty"
fi

HUBWEB="${EMBER_HUB_WEB:-$REPO/dist/web}"
if [ -f "$HUBWEB/index.html" ] && [ -d "$HUBWEB/wasm" ]; then
  mkdir -p "$DEST/hubweb"
  rsync -a --delete --exclude '.DS_Store' --exclude '*.md' "$HUBWEB/" "$DEST/hubweb/"
  if [ -f "$DEST/guides.pack" ]; then
    find "$DEST/hubweb" -name '*.pack' -type f | while read -r f; do
      if cmp -s "$f" "$DEST/guides.pack"; then ln -f "$DEST/guides.pack" "$f"; fi
    done
  fi
  echo "bundled hub web app from $HUBWEB ($(du -sk "$DEST/hubweb" | cut -f1) KB)"
else
  rm -rf "$DEST/hubweb"
  echo "note: $HUBWEB has no WASM build (run ./build.sh web); the phone hub will serve its built-in page"
fi
