#!/usr/bin/env sh
set -eu

SEEED_GFX_VERSION="V3.1.0"
SEEED_GFX_COMMIT="0b13b21f284c9bce3351b394bbd871b688d6aec7"
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LIBRARY_DIR="$ROOT_DIR/.arduino-sketchbook/libraries/Seeed_GFX"

arduino-cli lib install ArduinoJson@7.4.3
arduino-cli lib install QRCode@0.0.1

mkdir -p "$(dirname -- "$LIBRARY_DIR")"

if [ ! -d "$LIBRARY_DIR/.git" ]; then
  if [ -e "$LIBRARY_DIR" ]; then
    echo "$LIBRARY_DIR exists but is not a Git checkout" >&2
    exit 1
  fi

  git clone https://github.com/Seeed-Studio/Seeed_GFX.git "$LIBRARY_DIR"
fi

if [ -n "$(git -C "$LIBRARY_DIR" status --porcelain)" ]; then
  echo "$LIBRARY_DIR has local changes; refusing to change its revision" >&2
  exit 1
fi

git -C "$LIBRARY_DIR" fetch origin "refs/tags/$SEEED_GFX_VERSION:refs/tags/$SEEED_GFX_VERSION"

if [ "$(git -C "$LIBRARY_DIR" rev-parse "$SEEED_GFX_VERSION^{commit}")" != "$SEEED_GFX_COMMIT" ]; then
  echo "Seeed_GFX $SEEED_GFX_VERSION does not match pinned commit $SEEED_GFX_COMMIT" >&2
  exit 1
fi

if [ "$(git -C "$LIBRARY_DIR" rev-parse HEAD)" != "$SEEED_GFX_COMMIT" ]; then
  git -C "$LIBRARY_DIR" checkout --detach "$SEEED_GFX_COMMIT"
fi

echo "Seeed_GFX is pinned at $SEEED_GFX_VERSION"
