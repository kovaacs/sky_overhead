#!/usr/bin/env sh
set -eu

SEEED_GFX_COMMIT="0b13b21f284c9bce3351b394bbd871b688d6aec7" # V3.1.0
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LIBRARY_DIR="$ROOT_DIR/.arduino-sketchbook/libraries/Seeed_GFX"

arduino-cli lib install ArduinoJson@7.4.3
arduino-cli lib install QRCode@0.0.1

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

curl -fL --retry 3 \
  "https://github.com/Seeed-Studio/Seeed_GFX/archive/$SEEED_GFX_COMMIT.tar.gz" \
  -o "$TMPDIR/seeed_gfx.tar.gz"
tar -xzf "$TMPDIR/seeed_gfx.tar.gz" -C "$TMPDIR"

rm -rf "$LIBRARY_DIR"
mkdir -p "$(dirname -- "$LIBRARY_DIR")"
mv "$TMPDIR/Seeed_GFX-$SEEED_GFX_COMMIT" "$LIBRARY_DIR"

echo "Seeed_GFX is pinned at $SEEED_GFX_COMMIT"
