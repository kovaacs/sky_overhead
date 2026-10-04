#!/usr/bin/env python3
import hashlib
import os
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    firmware = Path(tmp) / "firmware"
    output = Path(tmp) / "release"
    firmware.mkdir()
    for suffix in ("bin", "bootloader.bin", "merged.bin", "partitions.bin"):
        (firmware / f"sky_overhead.ino.{suffix}").write_bytes(suffix.encode())
    for name in ("README.md", "FLASHING.md", "config.example.txt", "THIRD_PARTY_NOTICES.md", "LICENSE"):
        (Path(tmp) / name).write_text(name)
    command = [sys.executable, str(root / "tools/package_release.py"),
               "v0.0.0", str(firmware), str(output)]
    env = dict(os.environ, SOURCE_DATE_EPOCH="1704067200", TZ="UTC")
    subprocess.run(command, cwd=tmp, env=env, check=True)
    with zipfile.ZipFile(output / "sky-overhead-v0.0.0-firmware.zip") as bundle:
        assert bundle.namelist() == sorted(bundle.namelist())
        assert len(bundle.infolist()) == 9
        assert all(info.date_time == (2024, 1, 1, 0, 0, 0) for info in bundle.infolist())
        assert bundle.read("sky-overhead-v0.0.0/sky_overhead.ino.merged.bin") == b"merged.bin"
    checksums = (output / "SHA256SUMS").read_text()
    for line in checksums.splitlines():
        digest, name = line.split("  ")
        assert hashlib.sha256((output / name).read_bytes()).hexdigest() == digest
    for file in firmware.iterdir():
        os.utime(file, (1800000000, 1800000000))
    subprocess.run(command, cwd=tmp, env=env, check=True)
    assert (output / "SHA256SUMS").read_text() == checksums
    (firmware / "sky_overhead.ino.bin").unlink()
    failed = subprocess.run(command, cwd=tmp, env=env, capture_output=True, text=True)
    assert failed.returncode != 0 and "Missing firmware output:" in failed.stderr
print("release packaging tests passed")
