#!/usr/bin/env sh
set -eu

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <version> <firmware-dir> <output-dir>" >&2
  exit 1
fi

VERSION="$1"
FIRMWARE_DIR="$2"
OUTPUT_DIR="$3"
PACKAGE_NAME="sky-overhead-$VERSION"

for file in \
  sky_overhead.ino.bin \
  sky_overhead.ino.bootloader.bin \
  sky_overhead.ino.merged.bin \
  sky_overhead.ino.partitions.bin
do
  if [ ! -f "$FIRMWARE_DIR/$file" ]; then
    echo "Missing firmware output: $FIRMWARE_DIR/$file" >&2
    exit 1
  fi
done

mkdir -p "$OUTPUT_DIR/$PACKAGE_NAME"

cp "$FIRMWARE_DIR/sky_overhead.ino.bin" "$OUTPUT_DIR/$PACKAGE_NAME/"
cp "$FIRMWARE_DIR/sky_overhead.ino.bootloader.bin" "$OUTPUT_DIR/$PACKAGE_NAME/"
cp "$FIRMWARE_DIR/sky_overhead.ino.merged.bin" "$OUTPUT_DIR/$PACKAGE_NAME/"
cp "$FIRMWARE_DIR/sky_overhead.ino.partitions.bin" "$OUTPUT_DIR/$PACKAGE_NAME/"
cp README.md "$OUTPUT_DIR/$PACKAGE_NAME/"
cp FLASHING.md "$OUTPUT_DIR/$PACKAGE_NAME/"
cp config.example.txt "$OUTPUT_DIR/$PACKAGE_NAME/"
cp THIRD_PARTY_NOTICES.md "$OUTPUT_DIR/$PACKAGE_NAME/"
cp "$FIRMWARE_DIR/sky_overhead.ino.merged.bin" "$OUTPUT_DIR/$PACKAGE_NAME-merged.bin"

(
  cd "$OUTPUT_DIR"
  if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    # Normalize archive metadata for reproducible container builds.
    python3 - "$PACKAGE_NAME" "$SOURCE_DATE_EPOCH" <<'PY'
import os
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
timestamp = int(sys.argv[2])
for path in [root, *sorted(root.rglob("*"))]:
    os.chmod(path, 0o755 if path.is_dir() else 0o644)
    os.utime(path, (timestamp, timestamp))
PY
  fi
  # Recreate the archive so reruns cannot retain files from a previous ZIP.
  rm -f "$PACKAGE_NAME-firmware.zip"
  LC_ALL=C find "$PACKAGE_NAME" -type f | LC_ALL=C sort | zip -Xq "$PACKAGE_NAME-firmware.zip" -@
  shasum -a 256 "$PACKAGE_NAME-merged.bin" "$PACKAGE_NAME-firmware.zip" > SHA256SUMS
)
