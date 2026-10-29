#!/usr/bin/env sh
set -eu

SEEED_GFX2_COMMIT="e25570844ca702e53592fcb2880c337f6d6db189" # v1.0.0 + ePaper polarity/partial refresh fixes
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LIBRARY_DIR="$ROOT_DIR/.arduino-sketchbook/libraries/Seeed_GFX2"

arduino-cli lib install ArduinoJson@7.4.3
arduino-cli lib install QRCode@0.0.1

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

curl -fL --retry 3 \
  "https://github.com/Seeed-Studio/Seeed_GFX2/archive/$SEEED_GFX2_COMMIT.tar.gz" \
  -o "$TMPDIR/seeed_gfx2.tar.gz"
tar -xzf "$TMPDIR/seeed_gfx2.tar.gz" -C "$TMPDIR"

rm -rf "$LIBRARY_DIR"
mkdir -p "$(dirname -- "$LIBRARY_DIR")"
mv "$TMPDIR/Seeed_GFX2-$SEEED_GFX2_COMMIT" "$LIBRARY_DIR"

echo "Seeed_GFX2 is pinned at $SEEED_GFX2_COMMIT"
