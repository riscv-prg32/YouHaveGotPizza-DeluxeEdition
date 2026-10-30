#!/usr/bin/env python3
"""Store art, from the game's own sources and from a real emulator frame.

  python3 tools/make_store_art.py [frame.png]

assets/store/icon.png        64 x 64: the baked pizza under the Professor's cap
assets/store/screenshot.png  320 x 200: a frame captured from the QEMU firmware,
                             palettised so that it stays small in the cartridge
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from PIL import Image  # noqa: E402

import art_sprites as sp  # noqa: E402
import palette as pal  # noqa: E402


def icon() -> Image.Image:
    colors = pal.theme_colors("title")
    rgb = {k: pal.rgb888(v) for k, v in colors.items()}
    img = Image.new("RGB", (64, 64), rgb["ink"])
    # Night sky ramp behind the pizza.
    for y in range(64):
        for x in range(64):
            if (y * 16 // 64) > ((x ^ y) * 7 & 15):
                img.putpixel((x, y), rgb["s0"])
    legend = {k: rgb[v] for k, v in sp.LAYER_LEGEND.items()}
    for y, row in enumerate(sp.PIZZA):
        for x, ch in enumerate(row):
            if ch in legend:
                for sy in range(2):
                    for sx in range(2):
                        img.putpixel((8 + x * 2 + sx, 12 + y * 2 + sy), legend[ch])
    # The mortarboard, tilted by nothing but attitude.
    board = ["....KKKKKKKK....", "..KKKKKKKKKKKKY.", ".....KKKKKK...Y.", "..............Y."]
    for y, row in enumerate(board):
        for x, ch in enumerate(row):
            if ch != ".":
                c = rgb["gown"] if ch == "K" else rgb["lemon"]
                for sy in range(2):
                    for sx in range(2):
                        img.putpixel((16 + x * 2 + sx, 4 + y * 2 + sy), c)
    return img


def main() -> int:
    out = ROOT / "assets/store"
    out.mkdir(parents=True, exist_ok=True)
    icon().quantize(32, dither=Image.Dither.NONE).save(out / "icon.png", optimize=True)
    if len(sys.argv) > 1:
        frame = Image.open(sys.argv[1]).convert("RGB")
        if frame.size == (320, 240):   # strip the firmware bands
            frame = frame.crop((0, 20, 320, 220))
        frame.quantize(32, dither=Image.Dither.NONE).save(out / "screenshot.png", optimize=True)
    print({p.name: p.stat().st_size for p in out.glob("*.png")})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
