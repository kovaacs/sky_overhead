#!/usr/bin/env sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

python3 "$ROOT_DIR/tools/generate_icon_font.py"
exec arduino-cli compile "$@" "$ROOT_DIR"
