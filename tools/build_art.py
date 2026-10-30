#!/usr/bin/env python3
"""Build every picture of the cartridge from the text sources in tools/.

  python3 tools/build_art.py

Outputs (deterministic; both are committed):
  src/gen/art.inc      palettes, sprites, tile dictionary, per-screen tile maps
  assets/png/*.png     previews of the sheets and of each screen as the firmware
                       tile engine will show it (two colours per 8 x 8 cell)

The key step is tileize(): it reduces a free 320 x 200 painting to at most
SCENE_TILES (bitmap, foreground, background) tiles, the format of
prg32_tile_define(). See docs/graphics.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from PIL import Image  # noqa: E402

import art_scenes as sc  # noqa: E402
import art_sprites as sp  # noqa: E402
import levels as lv  # noqa: E402
import palette as pal  # noqa: E402

# Tile ids. 0 is the firmware's clear tile; the fonts and the animated tiles
# are fixed so that HUD code does not depend on the scene.
FONT_BASE = 1                     # cream on ink
FONT2_BASE = FONT_BASE + len(sp.FONT_ORDER)   # lemon on ink (same glyphs)
ANIM_BASE = FONT2_BASE + len(sp.FONT_ORDER)
ANIM_COUNT = 4                    # the animated cells of the bottom row
SCENE_BASE = ANIM_BASE + ANIM_COUNT
SCENE_TILES = 256 - SCENE_BASE    # upper bound; the budget below is what counts
SCENE_BUDGET = 128                # (bitmap, fg, bg) tiles per screen: the ROM budget
TITLE_BUDGET = 150                # the logo needs a few more


def glyph(ch: str) -> list[int]:
    return sp.glyph_bytes(ch)


def make_floor_tile_painter(c: sc.Canvas, level: dict) -> None:
    """Draw platforms, ladders and peels over the scene."""
    for f, a, b in level["floors"]:
        y = lv.FLOOR_ROWS[f] * 8
        for cell in range(a, b + 1):
            x = cell * 8
            c.rect(x, y, 8, 8, "s7")
            c.rect(x, y, 8, 3, "s9")
            c.rect(x + (0 if cell % 2 else 4), y + 3, 1, 5, "s6")
            c.rect(x, y + 6, 8, 1, "s6")
    for cell, top, bottom in level["ladders"]:
        x = cell * 8
        y0 = lv.FLOOR_ROWS[top] * 8
        y1 = lv.FLOOR_ROWS[bottom] * 8 + 8
        c.rect(x + 1, y0, 14, y1 - y0, "s7")
        c.rect(x + 2, y0, 2, y1 - y0, "s9")
        c.rect(x + 12, y0, 2, y1 - y0, "s9")
        for y in range(y0 + 3, y1, 4):
            c.rect(x + 2, y, 12, 1, "s9")
    for left, _ in level["pizzas"]:
        x = left * 8 - 4
        y = lv.PLATE_ROW * 8
        c.rect(x, y, 48, 8, "s7")
        c.rect(x, y, 48, 2, "s6")
        c.rect(x + 6, y + 2, 36, 1, "s9")
        c.rect(x + 2, y + 2, 2, 6, "s6")
        c.rect(x + 44, y + 2, 2, 6, "s6")


def cell_reduce(px: list[list[str]], cx: int, cy: int, colors: dict[str, int]) -> tuple[int, int, tuple[int, ...]]:
    """Two most frequent colours of one 8 x 8 cell, every pixel to the nearest."""
    counts: dict[int, int] = {}
    for y in range(8):
        for x in range(8):
            value = colors[px[cy * 8 + y][cx * 8 + x]]
            counts[value] = counts.get(value, 0) + 1
    ranked = sorted(counts, key=lambda v: (-counts[v], v))
    a = ranked[0]
    if len(ranked) == 1:
        return a, a, (0,) * 8
    # Second colour: the one that loses the least when the others snap to it.
    def loss(b: int) -> int:
        total = 0
        for v, n in counts.items():
            total += n * min(pal._distance(v, a), pal._distance(v, b))
        return total
    b = min(ranked[1:], key=lambda v: (loss(v), v))
    bits = []
    for y in range(8):
        row = 0
        for x in range(8):
            v = colors[px[cy * 8 + y][cx * 8 + x]]
            if pal._distance(v, b) < pal._distance(v, a):
                row |= 0x80 >> x
        bits.append(row)
    return b, a, tuple(bits)


def normalise(fg: int, bg: int, bits: tuple[int, ...]) -> tuple[int, int, tuple[int, ...]]:
    """One bitmap and its inverse are one tile: fg is the larger colour value."""
    if fg < bg:
        fg, bg, bits = bg, fg, tuple(b ^ 0xFF for b in bits)
    if fg == bg:
        bits = (0,) * 8
    return fg, bg, bits


def tile_distance(a, b) -> int:
    """Perceptual cost of showing tile a where tile b was painted."""
    total = 0
    for row_a, row_b in zip(a[2], b[2]):
        for k in range(8):
            ca = a[0] if row_a & (0x80 >> k) else a[1]
            cb = b[0] if row_b & (0x80 >> k) else b[1]
            total += pal._distance(ca, cb)
    return total


def tileize(px: list[list[str]], colors: dict[str, int], first_row: int, budget: int):
    """Reduce a painting to `budget` tiles by greedy nearest-tile merging."""
    grid: dict[tuple[int, int], tuple] = {}
    counts: dict[tuple, int] = {}
    for cy in range(first_row, 25):
        for cx in range(40):
            tile = normalise(*cell_reduce(px, cx, cy, colors))
            grid[(cx, cy)] = tile
            counts[tile] = counts.get(tile, 0) + 1
    replacement: dict[tuple, tuple] = {}
    while len(counts) > budget:
        # Merge the rarest tile into its nearest neighbour; ties break on value
        # so that the result never depends on hash order.
        rare = min(counts, key=lambda t: (counts[t], t))
        others = [t for t in counts if t != rare]
        near = min(others, key=lambda t: (tile_distance(rare, t), t))
        replacement[rare] = near
        counts[near] += counts.pop(rare)
    def final(t: tuple) -> tuple:
        while t in replacement:
            t = replacement[t]
        return t
    return {k: final(t) for k, t in grid.items()}, sorted(counts)


def rle(values: list[int]) -> list[int]:
    """(count, value) pairs: tile maps are mostly long horizontal runs."""
    out: list[int] = []
    i = 0
    while i < len(values):
        run = 1
        while i + run < len(values) and values[i + run] == values[i] and run < 255:
            run += 1
        out += [run, values[i]]
        i += run
    return out


def c_bytes(data, indent: str = "    ") -> str:
    lines = []
    data = list(data)
    for i in range(0, len(data), 20):
        lines.append(indent + ",".join(str(v) for v in data[i:i + 20]) + ",")
    return "\n".join(lines)


def pack_4bpp(rows: list[str], order: dict[str, int], w: int) -> list[int]:
    nibbles = [order.get(ch, 0) for row in rows for ch in row]
    if len(nibbles) % 2:
        nibbles.append(0)
    return [(nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles), 2)]


def sheet(names: list[str], frames: dict[str, list[str]]) -> list[list[str]]:
    out = []
    for name in names:
        if name.startswith("m:"):
            out.append([row[::-1] for row in frames[name[2:]]])
        else:
            out.append(frames[name])
    return out


def sprite_palette(legend: dict[str, str], colors: dict[str, int]) -> tuple[dict[str, int], list[int]]:
    """Index 0 is transparent; the others follow the legend order."""
    order = {".": 0}
    values = [0]
    for ch, name in legend.items():
        order[ch] = len(values)
        values.append(colors[name])
    assert len(values) <= 16, legend
    return order, values


def preview(frames: list[list[str]], order: dict[str, int], values: list[int], name: str, scale: int = 4) -> None:
    h = len(frames[0])
    w = len(frames[0][0])
    img = Image.new("RGB", (len(frames) * (w + 1) * scale, h * scale), (40, 40, 52))
    for k, frame in enumerate(frames):
        for y, row in enumerate(frame):
            for x, ch in enumerate(row):
                if order.get(ch, 0):
                    color = pal.rgb888(values[order[ch]])
                    for sy in range(scale):
                        for sx in range(scale):
                            img.putpixel((k * (w + 1) * scale + x * scale + sx, y * scale + sy), color)
    img.save(ROOT / f"assets/png/{name}.png", optimize=True)


def anim_tiles() -> list[list[int]]:
    """Four frames of a sparkle/wave cell, the character-animated bottom row."""
    frames = []
    for k in range(ANIM_COUNT):
        rows = []
        for y in range(8):
            row = 0
            for x in range(8):
                if ((x + k * 2) & 7) in (0, 1) and y == 2 + (x + k) % 2:
                    row |= 0x80 >> x
                if ((x + k * 2 + 4) & 7) in (2, 3) and y == 5 + (x + k) % 2:
                    row |= 0x80 >> x
            rows.append(row)
        frames.append(rows)
    return frames


def main() -> int:
    errors = lv.lint()
    if errors or pal.check():
        print("\n".join(errors))
        return 1
    (ROOT / "assets/png").mkdir(parents=True, exist_ok=True)
    (ROOT / "src/gen").mkdir(parents=True, exist_ok=True)
    screens = []
    for level in lv.LEVELS:
        screens.append({"name": level["scene"], "theme": level["theme"], "level": level})
    screens.append({"name": "title", "theme": "title", "level": None})

    dictionary: dict[tuple[int, ...], int] = {}
    for ch in sp.FONT_ORDER:
        dictionary[tuple(glyph(ch))] = len(dictionary)
    for frame in anim_tiles():
        dictionary.setdefault(tuple(frame), len(dictionary))

    out = []
    out.append("/* Generated by tools/build_art.py. Do not edit: change the text sources. */")
    out.append(f"#define FONT_BASE {FONT_BASE}\n#define FONT2_BASE {FONT2_BASE}\n#define ANIM_BASE {ANIM_BASE}")
    out.append(f"#define ANIM_COUNT {ANIM_COUNT}\n#define SCENE_BASE {SCENE_BASE}\n#define SCREEN_COUNT {len(screens)}")
    out.append(f"#define FONT_COUNT {len(sp.FONT_ORDER)}")
    out.append("/* ASCII code - 32 -> index in the font, 0 (blank) if the game has no such glyph. */")
    table = [sp.FONT_ORDER.index(chr(c)) if chr(c) in sp.FONT_ORDER else 0 for c in range(32, 128)]
    out.append("static const uint8_t ascii_to_font[96] = {\n" + c_bytes(table) + "\n};")

    screen_tables, total_scene = [], 0
    for scr in screens:
        canvas = sc.Canvas()
        painter = {
            "piazza": lambda: sc.piazza(canvas), "mercato": lambda: sc.mercato(canvas),
            "lungomare": lambda: sc.lungomare(canvas), "vicolo": lambda: sc.vicolo(canvas),
            "laboratorio": lambda: sc.laboratorio(canvas, glyph), "forno": lambda: sc.forno(canvas),
            "title": lambda: sc.title(canvas),
        }[scr["name"]]
        painter()
        colors = pal.theme_colors(scr["theme"])
        if scr["level"]:
            make_floor_tile_painter(canvas, scr["level"])
            first_row = 2
        else:
            paint_title(canvas)
            first_row = 0
        grid, tiles = tileize(canvas.px, colors, first_row, TITLE_BUDGET if scr["level"] is None else SCENE_BUDGET)
        index = {t: i for i, t in enumerate(tiles)}
        for t in tiles:
            dictionary.setdefault(t[2], len(dictionary))
        scr.update(grid=grid, tiles=tiles, index=index, colors=colors, first_row=first_row)
        # Preview exactly what the two-colour tile screen will show.
        img = Image.new("RGB", (320, 200), pal.rgb888(colors["ink"]))
        for (cx, cy), t in grid.items():
            for y in range(8):
                for x in range(8):
                    v = t[0] if t[2][y] & (0x80 >> x) else t[1]
                    img.putpixel((cx * 8 + x, cy * 8 + y), pal.rgb888(v))
        img.save(ROOT / f"assets/png/screen_{scr['name']}.png", optimize=True)
        painted = Image.new("RGB", (320, 200))
        for y in range(200):
            for x in range(320):
                painted.putpixel((x, y), pal.rgb888(colors[canvas.px[y][x]]))
        painted.save(ROOT / f"assets/png/painted_{scr['name']}.png", optimize=True)
        total_scene += len(tiles)

    out.append(f"#define TILE_DICT_COUNT {len(dictionary)}")
    out.append("static const uint16_t anim_dict[ANIM_COUNT] = {" + ",".join(str(dictionary[tuple(f)]) for f in anim_tiles()) + "};")
    out.append("static const uint8_t tile_dict[TILE_DICT_COUNT * 8] __attribute__((aligned(4))) = {")
    for bits in dictionary:
        out.append("    " + ",".join(str(b) for b in bits) + ",")
    out.append("};")

    rom = {"dict": len(dictionary) * 8}
    # Screen colour tables, tile tables and run-length maps.
    for n, scr in enumerate(screens):
        colors = scr["colors"]
        names = sorted(set(colors.values()))
        slot = {v: i for i, v in enumerate(names)}
        assert len(names) <= 32
        out.append(f"static const uint16_t screen{n}_palette[32] = {{ " + ",".join(str(v) for v in names + [names[0]] * (32 - len(names))) + " };")
        out.append(f"static const uint8_t screen{n}_palette_count = {len(names)};")
        entries = []
        for t in scr["tiles"]:
            d = dictionary[t[2]]
            entries += [d & 255, d >> 8, slot[t[0]], slot[t[1]]]
        out.append(f"static const uint8_t screen{n}_tiles[{len(entries)}] = {{\n" + c_bytes(entries) + "\n};")
        values = []
        for cy in range(scr["first_row"], 25):
            for cx in range(40):
                values.append(scr["index"][scr["grid"][(cx, cy)]])
        packed = rle(values)
        out.append(f"static const uint8_t screen{n}_map[{len(packed)}] = {{\n" + c_bytes(packed) + "\n};")
        rom[scr["name"]] = len(entries) + len(packed) + 2 * len(names)
        out.append(f"#define SCREEN{n}_ANIM_FG {colors['s8']}\n#define SCREEN{n}_ANIM_BG {colors['s7']}")
        out.append(f"#define SCREEN{n}_TILES {len(scr['tiles'])}\n#define SCREEN{n}_FIRST_ROW {scr['first_row']}\n#define SCREEN{n}_MAP_LEN {len(packed)}")

    ctheme = pal.theme_colors("piazza")
    out.append("/* Colours for code that draws without a tile: fixed across themes. */")
    for name, value in pal._FIXED.items():
        out.append(f"#define COL_{name.upper()} {value}")

    # Sprites. Sheets are 4 bits per pixel, palette index 0 transparent.
    order, values = sprite_palette(sp.PROF_LEGEND, ctheme)
    frames = sheet(sp.PROF_SHEET, sp.PROF_FRAMES)
    preview(frames, order, values, "sheet_professor")
    data = [b for f in frames for b in pack_4bpp(f, order, 16)]
    out.append(f"#define PROF_FRAMES {len(frames)}\nstatic const uint16_t prof_palette[{len(values)}] = {{" + ",".join(map(str, values)) + "};")
    out.append(f"#define PROF_COLORS {len(values)}\nstatic const uint8_t prof_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["professor"] = len(data)

    legend = dict(sp.STUDENT_LEGEND)
    keys = list(legend)
    order = {".": 0, **{ch: i + 1 for i, ch in enumerate(keys)}}
    frames = sheet(sp.STUDENT_SHEET, sp.STUDENT_FRAMES)
    data = [b for f in frames for b in pack_4bpp(f, order, 16)]
    out.append(f"#define STUDENT_FRAMES {len(frames)}\n#define STUDENT_COLORS {len(keys) + 1}\nstatic const uint8_t student_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["students"] = len(data)
    palettes = []
    for kind, mapping in sp.STUDENT_KINDS.items():
        row = [0]
        for ch in keys:
            name = mapping.get(legend[ch], legend[ch])
            row.append(ctheme[name])
        palettes.append(row)
        preview(frames, order, row, f"sheet_student_{kind}")
    out.append(f"static const uint16_t student_palettes[{len(palettes)}][{len(keys) + 1}] = {{")
    for row in palettes:
        out.append("    {" + ",".join(map(str, row)) + "},")
    out.append("};")

    order, values = sprite_palette(sp.ITEM_LEGEND, ctheme)
    frames = sheet(sp.ITEM_SHEET, sp.ITEM_FRAMES)
    preview(frames, order, values, "sheet_items")
    data = [b for f in frames for b in pack_4bpp(f, order, 16)]
    out.append(f"#define ITEM_FRAMES {len(frames)}\n#define ITEM_COLORS {len(values)}\nstatic const uint16_t item_palette[{len(values)}] = {{" + ",".join(map(str, values)) + "};")
    out.append(f"static const uint8_t item_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["items"] = len(data)

    order, values = sprite_palette(sp.PUFF_LEGEND, ctheme)
    frames = sheet(sp.PUFF_SHEET, sp.PUFF_FRAMES)
    data = [b for f in frames for b in pack_4bpp(f, order, 16)]
    out.append(f"#define PUFF_FRAMES {len(frames)}\nstatic const uint16_t puff_palette[{len(values)}] = {{" + ",".join(map(str, values)) + "};")
    out.append(f"#define PUFF_COLORS {len(values)}\nstatic const uint8_t puff_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["puff"] = len(data)

    order, values = sprite_palette(sp.LAYER_LEGEND, ctheme)
    pieces = []
    for kind in sp.LAYER_ORDER:
        pieces += [sp.LAYERS[kind]["left"], sp.LAYERS[kind]["mid"], sp.LAYERS[kind]["right"]]
    preview(pieces, order, values, "sheet_layers")
    data = [b for f in pieces for b in pack_4bpp(f, order, 8)]
    out.append(f"#define LAYER_PIECES {len(pieces)}\n#define LAYER_COLORS {len(values)}\nstatic const uint16_t layer_palette[{len(values)}] = {{" + ",".join(map(str, values)) + "};")
    out.append(f"static const uint8_t layer_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["layers"] = len(data)
    data = pack_4bpp(sp.PIZZA, order, 24)
    preview([sp.PIZZA], order, values, "sprite_pizza")
    out.append(f"static const uint8_t pizza_pixels[{len(data)}] = {{\n" + c_bytes(data) + "\n};")
    rom["pizza"] = len(data)

    # Level geometry: the bytes src/pz_core.h decodes.
    out.append(f"#define LEVEL_COUNT {len(lv.LEVELS)}")
    blob: list[int] = []
    offsets = []
    for level in lv.LEVELS:
        offsets.append(len(blob))
        blob += [len(level["floors"])]
        for f, a, b in level["floors"]:
            blob += [f, a, b]
        blob += [len(level["ladders"])]
        for cell, a, b in level["ladders"]:
            blob += [cell, a, b]
        blob += [len(level["pizzas"])]
        for left, recipe in level["pizzas"]:
            blob += [left, len(recipe)]
            for f, kind in recipe.items():
                blob += [f, kind]
        blob += list(level["spawn"]) + [len(level["students"])]
        for cell, f in level["students"]:
            blob += [cell, f]
    out.append("static const uint16_t level_offsets[LEVEL_COUNT] = {" + ",".join(map(str, offsets)) + "};")
    out.append(f"static const uint8_t level_data[{len(blob)}] = {{\n" + c_bytes(blob) + "\n};")
    rom["levels"] = len(blob)
    names = [lvl["name"] for lvl in lv.LEVELS]
    width = max(len(n) for n in names) + 1
    flat = "".join(n.ljust(width, "\0") for n in names)
    out.append(f"#define LEVEL_NAME_LEN {width}")
    out.append(f"static const char level_names[{len(flat)}] = {{\n" + c_bytes(ord(c) for c in flat) + "\n};")

    (ROOT / "src/gen/art.inc").write_text("\n".join(out) + "\n", encoding="utf-8")
    total = sum(rom.values())
    print(f"art: {len(dictionary)} dictionary tiles, {total_scene} screen tiles, {total} bytes of ROM")
    print("     " + ", ".join(f"{k} {v}" for k, v in rom.items()))
    return 0


def paint_title(c: sc.Canvas) -> None:
    """Logo: row-coloured giant letters, the way an 8-bit logo was made."""
    ramp = ["lemon", "lemon", "orange", "orange", "red", "red", "redd", "redd"]
    def big(x: int, y: int, s: str, scale: int, colors: list[str], shadow: str | None = None) -> None:
        for ch in s:
            for row, bits in enumerate(glyph(ch)[:7]):
                for col in range(8):
                    if bits & (0x80 >> col):
                        if shadow:
                            c.rect(x + col * scale + scale // 2, y + row * scale + scale // 2, scale, scale, shadow)
            x += 8 * scale
    def big_top(x: int, y: int, s: str, scale: int, colors: list[str]) -> None:
        for ch in s:
            for row, bits in enumerate(glyph(ch)[:7]):
                for col in range(8):
                    if bits & (0x80 >> col):
                        color = colors[min(row * len(colors) // 7, len(colors) - 1)]
                        c.rect(x + col * scale, y + row * scale, scale, scale, color)
            x += 8 * scale
    c.text(64, 12, "YOU HAVE GOT", "cream", glyph, 2)
    big(40, 34, "PIZZA", 6, ramp, "ink")
    big_top(40, 34, "PIZZA", 6, ramp)


if __name__ == "__main__":
    raise SystemExit(main())
