#!/usr/bin/env python3
import hashlib
import os
import shutil
import sys
import zipfile
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit(f"Usage: {sys.argv[0]} <version> <firmware-dir> <output-dir>")

version, firmware_dir, output_dir = sys.argv[1:]
firmware = Path(firmware_dir)
output = Path(output_dir)
package_name = f"sky-overhead-{version}"
package = output / package_name
files = [firmware / f"sky_overhead.ino.{suffix}" for suffix in
         ("bin", "bootloader.bin", "merged.bin", "partitions.bin")]
for file in files:
    if not file.is_file():
        raise SystemExit(f"Missing firmware output: {file}")

package.mkdir(parents=True, exist_ok=True)
for file in [*files, *map(Path, ("README.md", "FLASHING.md", "config.example.txt",
                               "THIRD_PARTY_NOTICES.md", "LICENSE"))]:
    shutil.copy(file, package)
merged = output / f"{package_name}-merged.bin"
shutil.copy(firmware / "sky_overhead.ino.merged.bin", merged)

if epoch := os.environ.get("SOURCE_DATE_EPOCH"):
    for path in [package, *package.rglob("*")]:
        path.chmod(0o755 if path.is_dir() else 0o644)
        os.utime(path, (int(epoch), int(epoch)))

archive = output / f"{package_name}-firmware.zip"
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
    for file in sorted(package.rglob("*")):
        if file.is_file():
            bundle.write(file, file.relative_to(output))

with (output / "SHA256SUMS").open("w", encoding="ascii") as sums:
    for file in (merged, archive):
        with file.open("rb") as data:
            sums.write(f"{hashlib.file_digest(data, 'sha256').hexdigest()}  {file.name}\n")
