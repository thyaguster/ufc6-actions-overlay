"""Embed the browser lobby's Parsectagon SVG raster in the native overlay.

The checked-in PNG is rendered from assets/parsectagon-ufc-6.svg at 3x with
Chrome headless using assets/render-parsectagon.html. The generated run-length
data keeps the native DLL independent of browser and image libraries at runtime.
"""

from __future__ import annotations

import argparse
import hashlib
from itertools import groupby
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
SVG = ROOT / "assets" / "parsectagon-ufc-6.svg"
PNG = ROOT / "assets" / "parsectagon-raster.png"
HEADER = ROOT / "lobby_brand_data.h"


def generated_header() -> str:
    image = Image.open(PNG).convert("RGB")
    if image.size != (984, 204):
        raise ValueError(f"Expected a 984x204 logo, got {image.size}")
    pixels = image.tobytes()
    colors = (
        (pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2]
        for i in range(0, len(pixels), 3)
    )
    runs: list[tuple[int, int]] = []
    for color, values in groupby(colors):
        length = sum(1 for _ in values)
        while length:
            amount = min(length, 65535)
            runs.append((amount, color))
            length -= amount
    svg_hash = hashlib.sha256(SVG.read_bytes()).hexdigest()
    lines = [
        "/* Generated from the exact Parsectagon UFC 6 SVG via Chrome. */",
        f"/* Source SVG SHA256: {svg_hash} */",
        "#ifndef UFC6_LOBBY_BRAND_DATA_H",
        "#define UFC6_LOBBY_BRAND_DATA_H",
        "#define LOBBY_BRAND_WIDTH 984",
        "#define LOBBY_BRAND_HEIGHT 204",
        "typedef struct { unsigned short length; unsigned int rgb; } LobbyBrandRun;",
        "static const LobbyBrandRun lobby_brand_runs[] = {",
    ]
    lines.extend(f"    {{{length},0x{color:06X}u}}," for length, color in runs)
    lines.extend(["};", "#endif", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = generated_header()
    if args.check:
        if not HEADER.exists() or HEADER.read_text(encoding="utf-8") != content:
            raise SystemExit("Parsectagon logo data is out of date; run generate_lobby_brand.py")
    else:
        HEADER.write_text(content, encoding="utf-8")
        print(f"Wrote {HEADER} from {PNG}")


if __name__ == "__main__":
    main()
