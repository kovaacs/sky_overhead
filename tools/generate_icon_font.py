#!/usr/bin/env python3
import hashlib
import json
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from math import pow
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "IconFont.h"
CACHE_DIR = ROOT / ".build" / "cache"
STATE_FILE = ROOT / ".build" / "icon-font" / "state.json"
LUCIDE_VERSION = "1.33.0"
LUCIDE_COMMIT = "59978cecf84986af59f1f9f503bcebdc89c6d166"
LUCIDE_ARCHIVE = CACHE_DIR / f"lucide-icons-{LUCIDE_VERSION}.zip"
LUCIDE_ARCHIVE_URL = (
    f"https://github.com/lucide-icons/lucide/releases/download/{LUCIDE_VERSION}/"
    f"lucide-icons-{LUCIDE_VERSION}.zip"
)
LUCIDE_ARCHIVE_SHA256 = "53831c8def65621f88cae315cdb38ac70db1d937062df35c93546efb00260a98"
DEFAULT_STROKE_WIDTH = "2"
STROKE_SCALE_EXPONENT = 0.7
STROKE_SCALE_BREAKPOINT = 2.0

ICONS = [
    ("PLANE", "plane.svg", "A", 24, DEFAULT_STROKE_WIDTH),
    ("SUN", "sun.svg", "B", 144, DEFAULT_STROKE_WIDTH),
    ("MOON_STAR", "moon-star.svg", "C", 144, DEFAULT_STROKE_WIDTH),
    ("THERMOMETER", "thermometer.svg", "D", 64, DEFAULT_STROKE_WIDTH),
    ("DROPLET", "droplet.svg", "E", 64, DEFAULT_STROKE_WIDTH),
    ("ARROW_RIGHT", "arrow-right.svg", "F", 48, DEFAULT_STROKE_WIDTH),
    ("BATTERY_EMPTY", "battery.svg", "G", 32, DEFAULT_STROKE_WIDTH),
    ("BATTERY_LOW", "battery-low.svg", "H", 32, DEFAULT_STROKE_WIDTH),
    ("BATTERY_MEDIUM", "battery-medium.svg", "I", 32, DEFAULT_STROKE_WIDTH),
    ("BATTERY_FULL", "battery-full.svg", "J", 32, DEFAULT_STROKE_WIDTH),
    ("PLANE_LARGE", "plane.svg", "K", 144, DEFAULT_STROKE_WIDTH),
    ("HELICOPTER_LARGE", "helicopter.svg", "L", 144, DEFAULT_STROKE_WIDTH),
    ("CLOUDY_LARGE", "cloudy.svg", "M", 144, DEFAULT_STROKE_WIDTH),
    ("DOT", "dot.svg", "N", 32, "4"),
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def generation_key() -> str:
    return sha256(Path(__file__))


def output_is_current() -> bool:
    if not OUT.exists() or not STATE_FILE.exists():
        return False
    try:
        state = json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    return state == {"generation_key": generation_key(), "output_sha256": sha256(OUT)}


def ensure_archive() -> None:
    if LUCIDE_ARCHIVE.exists() and sha256(LUCIDE_ARCHIVE) == LUCIDE_ARCHIVE_SHA256:
        return

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    partial = LUCIDE_ARCHIVE.with_suffix(".zip.part")
    partial.unlink(missing_ok=True)
    print(f"Downloading Lucide {LUCIDE_VERSION} ({LUCIDE_COMMIT})...")
    try:
        request = urllib.request.Request(LUCIDE_ARCHIVE_URL, headers={"User-Agent": "sky-overhead-build"})
        with urllib.request.urlopen(request, timeout=30) as response, partial.open("wb") as destination:
            shutil.copyfileobj(response, destination)
        actual_sha256 = sha256(partial)
        if actual_sha256 != LUCIDE_ARCHIVE_SHA256:
            raise RuntimeError(
                f"Lucide archive checksum mismatch: expected {LUCIDE_ARCHIVE_SHA256}, got {actual_sha256}"
            )
        partial.replace(LUCIDE_ARCHIVE)
    finally:
        partial.unlink(missing_ok=True)


def extract_sources(icon_dir: Path) -> None:
    with zipfile.ZipFile(LUCIDE_ARCHIVE) as archive:
        for filename in {icon[1] for icon in ICONS}:
            member = f"icons/{filename}"
            try:
                data = archive.read(member)
            except KeyError as error:
                raise RuntimeError(f"Lucide archive is missing {member}") from error
            (icon_dir / filename).write_bytes(data)


def rendering_commands() -> tuple[str, str]:
    rsvg = shutil.which("rsvg-convert")
    imagemagick = shutil.which("magick") or shutil.which("convert")
    if not rsvg or not imagemagick:
        raise SystemExit(
            "Icon generation requires rsvg-convert and ImageMagick; "
            "see README.md for installation instructions"
        )
    return rsvg, imagemagick


def viewbox_size(root: ElementTree.Element) -> float:
    viewbox = root.get("viewBox")
    if not viewbox:
        width = root.get("width")
        if not width:
            raise ValueError("SVG is missing viewBox and width")
        return float(width)
    parts = viewbox.replace(",", " ").split()
    if len(parts) != 4:
        raise ValueError(f"Unsupported SVG viewBox: {viewbox}")
    width = float(parts[2])
    height = float(parts[3])
    if width != height:
        raise ValueError(f"Expected square SVG viewBox, got {viewbox}")
    return width


def svg_with_stroke_width(svg_path: Path, size: int, stroke_width: str, out_path: Path) -> None:
    ElementTree.register_namespace("", "http://www.w3.org/2000/svg")
    tree = ElementTree.parse(svg_path)
    root = tree.getroot()
    viewbox = viewbox_size(root)
    scale = size / viewbox
    if scale <= STROKE_SCALE_BREAKPOINT:
        output_stroke = float(stroke_width) * scale
    else:
        output_stroke = (
            float(stroke_width)
            * STROKE_SCALE_BREAKPOINT
            * pow(scale / STROKE_SCALE_BREAKPOINT, STROKE_SCALE_EXPONENT)
        )
    stroke_units = output_stroke / scale
    root.set("stroke-width", f"{stroke_units:g}")
    tree.write(out_path, encoding="unicode", xml_declaration=False)


def render_icon(
    svg_path: Path, size: int, stroke_width: str, rsvg: str, imagemagick: str
) -> list[list[int]]:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        svg = tmp_path / "icon.svg"
        png = tmp_path / "icon.png"
        svg_with_stroke_width(svg_path, size, stroke_width, svg)
        subprocess.run(
            [rsvg, "-w", str(size), "-h", str(size), "-o", str(png), str(svg)],
            check=True,
        )
        raw = subprocess.check_output(
            [imagemagick, str(png), "-alpha", "remove", "-colorspace", "Gray", "-depth", "8", "gray:-"]
        )

    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            shade = raw[y * size + x]
            row.append(1 if shade < 192 else 0)
        pixels.append(row)
    return pixels


def pack_bitmap(pixels: list[list[int]]) -> list[int]:
    data = []
    width = len(pixels[0])
    for row in pixels:
        for x0 in range(0, width, 8):
            byte = 0
            for bit in range(8):
                x = x0 + bit
                if x < width and row[x]:
                    byte |= 0x80 >> bit
            data.append(byte)
    return data


def main() -> None:
    if output_is_current():
        print("IconFont.h is up to date.")
        return

    rsvg, imagemagick = rendering_commands()
    ensure_archive()

    bitmaps = []
    glyphs = []
    offset = 0

    with tempfile.TemporaryDirectory() as tmp:
        icon_dir = Path(tmp)
        extract_sources(icon_dir)
        for name, filename, char, size, stroke_width in ICONS:
            data = pack_bitmap(render_icon(icon_dir / filename, size, stroke_width, rsvg, imagemagick))
            bitmaps.extend(data)
            y_offset = -min(size, 127)
            glyphs.append((offset, size, size, size + 2, 0, y_offset, char, name))
            offset += len(data)

    reserved_char = chr(ord(ICONS[-1][2]) + 1)
    glyphs.append((offset, 0, 0, 0, 0, 0, reserved_char, "reserved"))

    bitmap_lines = []
    for i in range(0, len(bitmaps), 12):
        bitmap_lines.append("  " + ", ".join(f"0x{b:02X}" for b in bitmaps[i : i + 12]) + ",")

    const_lines = [f"  constexpr char {name} = '{char}';" for name, _, char, _, _ in ICONS]
    const_lines.extend(f"  constexpr uint8_t {name}_SIZE = {size};" for name, _, _, size, _ in ICONS)
    glyph_lines = [
        f"  {{ {off}, {w}, {h}, {adv}, {xo}, {yo} }},  // 0x{ord(char):02X} '{char}': {name.lower()}"
        for off, w, h, adv, xo, yo, char, name in glyphs
    ]

    output = f"""#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace icon {{
{chr(10).join(const_lines)}
}}

// Generated by tools/generate_icon_font.py from Lucide {LUCIDE_VERSION} SVG icons.
// See THIRD_PARTY_NOTICES.md for license information.
const uint8_t SkyIcon24Bitmaps[] PROGMEM = {{
{chr(10).join(bitmap_lines)}
}};

const GFXglyph SkyIcon24Glyphs[] PROGMEM = {{
{chr(10).join(glyph_lines)}
}};

const GFXfont SkyIcon24 PROGMEM = {{
  (uint8_t*)SkyIcon24Bitmaps,
  (GFXglyph*)SkyIcon24Glyphs,
  0x{ord(ICONS[0][2]):02X}, 0x{ord(reserved_char):02X}, 24
}};
"""
    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = STATE_FILE.parent / "IconFont.h.tmp"
    temporary_output.write_text(output, encoding="utf-8")
    temporary_output.replace(OUT)

    STATE_FILE.write_text(
        json.dumps({"generation_key": generation_key(), "output_sha256": sha256(OUT)}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Generated {OUT.name} from Lucide {LUCIDE_VERSION}.")


if __name__ == "__main__":
    main()
