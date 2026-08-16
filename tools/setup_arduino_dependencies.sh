#!/usr/bin/env sh
set -eu

SEEED_GFX_REVISION="a2de1abca0597c202193f22d01e9fa35d1ff613b"
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LIBRARY_DIR="$ROOT_DIR/.arduino-sketchbook/libraries/Seeed_GFX"

arduino-cli lib install ArduinoJson@7.4.3

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

if [ "$(git -C "$LIBRARY_DIR" rev-parse HEAD)" != "$SEEED_GFX_REVISION" ]; then
  git -C "$LIBRARY_DIR" fetch origin "$SEEED_GFX_REVISION"
  git -C "$LIBRARY_DIR" checkout --detach "$SEEED_GFX_REVISION"
fi

echo "Seeed_GFX is pinned at $SEEED_GFX_REVISION"
