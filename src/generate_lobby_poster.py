"""Embed the Parsectagon site's arena and preset portraits for native room cards."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "lobby"
HEADER = ROOT / "lobby_poster_data.h"
PORTRAITS = ("striker", "grappler", "southpaw", "champion", "counter", "rookie")
PORTRAIT_SIZE = (160, 96)
ARENA_SIZE = (377, 132)


def fitted(path: Path, size: tuple[int, int]) -> Image.Image:
    with Image.open(path) as image:
        return ImageOps.fit(image.convert("RGB"), size, method=Image.Resampling.LANCZOS)


def array(name: str, values: list[int], width: int = 24) -> list[str]:
    lines = [f"static const unsigned short {name}[] = {{"]
    for start in range(0, len(values), width):
        lines.append("    " + ",".join(f"0x{value:04X}u" for value in values[start : start + width]) + ",")
    lines.append("};")
    return lines


def rgb565(image: Image.Image) -> list[int]:
    return [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in image.get_flattened_data()]


def generated_header() -> str:
    sources = [ASSETS / "fight-arena.png", *(ASSETS / f"{name}.jpg" for name in PORTRAITS)]
    for path in sources:
        if not path.exists():
            raise FileNotFoundError(path)
    lines = [
        "/* Generated from the Parsectagon website's arena and six preset fighter portraits. */",
        "/* Source SHA256: " + ", ".join(f"{p.name}={hashlib.sha256(p.read_bytes()).hexdigest()}" for p in sources) + " */",
        "#ifndef UFC6_LOBBY_POSTER_DATA_H",
        "#define UFC6_LOBBY_POSTER_DATA_H",
        f"#define LOBBY_POSTER_WIDTH {ARENA_SIZE[0]}",
        f"#define LOBBY_POSTER_HEIGHT {ARENA_SIZE[1]}",
        f"#define LOBBY_PORTRAIT_WIDTH {PORTRAIT_SIZE[0]}",
        f"#define LOBBY_PORTRAIT_HEIGHT {PORTRAIT_SIZE[1]}",
    ]
    arena = ImageEnhance.Brightness(fitted(sources[0], ARENA_SIZE)).enhance(0.43)
    lines.extend(array("lobby_arena_rgb565", rgb565(arena)))
    for name in PORTRAITS:
        portrait = ImageOps.grayscale(fitted(ASSETS / f"{name}.jpg", PORTRAIT_SIZE)).convert("RGB")
        portrait = ImageEnhance.Contrast(portrait).enhance(1.15)
        pixels = portrait.load()
        for y in range(PORTRAIT_SIZE[1]):
            fade = 0.70 * (1.0 - 0.72 * max(0.0, (y / PORTRAIT_SIZE[1] - 0.48) / 0.52))
            for x in range(PORTRAIT_SIZE[0]):
                r, g, b = pixels[x, y]
                pixels[x, y] = (int(r * fade), int(g * fade), int(b * fade))
        lines.extend(array(f"lobby_portrait_{name}_rgb565", rgb565(portrait)))
    lines.extend(["#endif", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    content = generated_header()
    if args.check:
        if not HEADER.exists() or HEADER.read_text(encoding="utf-8") != content:
            raise SystemExit("Parsectagon poster data is out of date; run generate_lobby_poster.py")
    else:
        HEADER.write_text(content, encoding="utf-8")
        print(f"Wrote {HEADER}")


if __name__ == "__main__":
    main()
