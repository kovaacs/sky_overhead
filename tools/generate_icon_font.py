#!/usr/bin/env python3
import subprocess
import tempfile
import urllib.request
from math import pow
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "assets" / "icons" / "lucide"
OUT = ROOT / "IconFont.h"
LUCIDE_RAW = "https://raw.githubusercontent.com/lucide-icons/lucide/main"
LICENSE_FILE = ICON_DIR / "LICENSE"
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
]


def download_if_missing(path: Path, url: str) -> None:
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=20) as response:
        path.write_bytes(response.read())


def ensure_sources() -> None:
    for _, filename, _, _, _ in ICONS:
        download_if_missing(ICON_DIR / filename, f"{LUCIDE_RAW}/icons/{filename}")
    download_if_missing(LICENSE_FILE, f"{LUCIDE_RAW}/LICENSE")


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


def render_icon(svg_path: Path, size: int, stroke_width: str) -> list[list[int]]:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        svg = tmp_path / "icon.svg"
        png = tmp_path / "icon.png"
        svg_with_stroke_width(svg_path, size, stroke_width, svg)
        subprocess.run(
            ["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(png), str(svg)],
            check=True,
        )
        raw = subprocess.check_output(
            ["magick", str(png), "-alpha", "remove", "-colorspace", "Gray", "-depth", "8", "gray:-"]
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
    ensure_sources()

    bitmaps = []
    glyphs = []
    offset = 0

    for name, filename, char, size, stroke_width in ICONS:
        data = pack_bitmap(render_icon(ICON_DIR / filename, size, stroke_width))
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

    OUT.write_text(
        f"""#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace icon {{
{chr(10).join(const_lines)}
}}

// Generated by tools/generate_icon_font.py from Lucide SVG icons.
// Source icons and ISC license live under assets/icons/lucide/.
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
""",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
