#!/usr/bin/env python3
"""The indexed colour model of You Have Got Pizza: Deluxe Edition.

PRG32's ESP32-C6 framebuffer holds one 8-bit palette index per pixel. Every
RGB565 colour a cartridge passes to the firmware is quantised to a cell of the
system palette's 6x6x6 cube (`prg32_gfx_index_for_rgb565_unlocked`). The entry
of that cell can be overwritten with `prg32_palette_set`, which gives a free
choice of exact colours under one condition: two colours on screen at the same
time must not fall in the same cell. `check()` proves it for every theme.

QEMU keeps a true RGB565 surface, so the same RGB565 value is simply shown
as it is: the two renderers then produce the same picture. See docs/graphics.md.
"""
from __future__ import annotations


def rgb565(hex_rgb: str) -> int:
    v = int(hex_rgb.lstrip("#"), 16)
    r, g, b = (v >> 16) & 255, (v >> 8) & 255, v & 255
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rgb888(value: int) -> tuple[int, int, int]:
    r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
    return ((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6)


NAMED = (0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F)


def cube_index(color: int) -> int:
    """Exact port of prg32_gfx_index_for_rgb565_unlocked()."""
    if color in NAMED:
        return NAMED.index(color)
    r = ((color >> 11) & 31) * 5 // 31
    g = ((color >> 5) & 63) * 5 // 63
    b = (color & 31) * 5 // 31
    return 16 + r * 36 + g * 6 + b


# Colours shared by every level: characters, ingredients, interface. They are
# the identity of the game, so no theme may change them.
FIXED = {
    "ink": "#1a1423",   # outline
    "cream": "#fff3d6", # text, apron, highlights
    "skin": "#f2b48c",
    "skind": "#b9714f",
    "gown": "#4b2f7a",  # the Professor's gown
    "gownd": "#2f1b52",
    "red": "#dc3127",   # tomato
    "redd": "#8f1d22",
    "dough": "#ecd08a",
    "crust": "#b9742f",
    "cheese": "#fbe58c",
    "cheesed": "#dcaa3d",
    "basil": "#46a73d",
    "basild": "#1d5c34",
    "lemon": "#f9d62e",
    "grey": "#9a9aa8",
    "denim": "#2e5fa8",
    "brown": "#5e3a22",
    "cyan": "#3ec1c9",
    "orange": "#f08a24",
}

# Scene colours per theme: sky ramp, far and near architecture, floor, ladder.
# Named "s0".."s9" so that one painter works in every theme.
THEMES = {
    "piazza": ["#7fb7d6", "#a6d3e0", "#dcb6a0", "#b8967a", "#8c7350", "#6c5238", "#c78a4a", "#3d2a1f", "#54606e", "#e8dcae"],
    "mercato": ["#8fc6c2", "#bfe0c9", "#d8a25a", "#b67d3f", "#7f4f2d", "#5a3524", "#c4662e", "#2a2118", "#3f6d4c", "#f0d9a0"],
    "lungomare": ["#f2a65a", "#f7cf7a", "#b0627e", "#7d4a7c", "#4e3a6e", "#2c3f73", "#d08a3c", "#1e1a33", "#3f6f96", "#ffe29a"],
    "vicolo": ["#3a4a7a", "#59609b", "#8a6d7a", "#6b5468", "#4d3f55", "#352c42", "#a0705a", "#17121f", "#7a7f8f", "#f2c76e"],
    "laboratorio": ["#1f3b4d", "#2c5566", "#3a7a7a", "#2a6159", "#1d4a47", "#143338", "#5fb39a", "#0b1a1e", "#8fb8c2", "#d9f2c4"],
    "forno": ["#201a38", "#352647", "#7a2f2a", "#5c2526", "#42201f", "#2c1a1e", "#c2562a", "#120c14", "#6e6470", "#ffbe5c"],
    "title": ["#22345e", "#3a5284", "#6b5d8a", "#4e4a78", "#35355f", "#242645", "#c2562a", "#141027", "#5b6f96", "#ffd98a"],
}
SCENE_KEYS = [f"s{i}" for i in range(10)]


def _cell_bounds(cell: int) -> tuple[tuple[int, int], tuple[int, int], tuple[int, int]]:
    """Inclusive RGB565 channel ranges that quantise to one cube cell."""
    v = cell - 16
    r, g, b = v // 36, (v // 6) % 6, v % 6
    def span(level: int, top: int) -> tuple[int, int]:
        values = [c for c in range(top + 1) if c * 5 // top == level]
        return values[0], values[-1]
    return span(r, 31), span(g, 63), span(b, 31)


def _nearest_in_cell(color: int, cell: int) -> int:
    (r0, r1), (g0, g1), (b0, b1) = _cell_bounds(cell)
    r = min(max((color >> 11) & 31, r0), r1)
    g = min(max((color >> 5) & 63, g0), g1)
    b = min(max(color & 31, b0), b1)
    return (r << 11) | (g << 5) | b


def _distance(a: int, b: int) -> int:
    ra, ga, ba = rgb888(a)
    rb, gb, bb = rgb888(b)
    return 2 * (ra - rb) ** 2 + 4 * (ga - gb) ** 2 + 3 * (ba - bb) ** 2


ALIAS_LIMIT = 4000


def solve(names: list[str], wanted: dict[str, str], taken: dict[int, str] | None = None,
          holders: dict[int, int] | None = None) -> dict[str, int]:
    """Give every colour its own cube cell, moving as little as possible.

    A colour whose natural cell is free keeps its designed value exactly. A
    colour that lands on a very similar colour simply becomes that colour (one
    palette entry fewer, nothing lost). Any other collision moves the later
    colour to the nearest colour of the nearest free cell. The result is
    deterministic: it depends only on the tables above.
    """
    taken = dict(taken or {})
    values = dict(holders or {})
    out: dict[str, int] = {}
    for name in names:
        color = rgb565(wanted[name])
        cell = cube_index(color)
        if cell in taken and _distance(color, values[cell]) <= ALIAS_LIMIT:
            out[name] = values[cell]
            continue
        if color in NAMED or cell in taken:
            best = None
            for candidate in range(16, 232):
                if candidate in taken:
                    continue
                moved = _nearest_in_cell(color, candidate)
                if moved in NAMED or cube_index(moved) != candidate:
                    continue
                cost = _distance(color, moved)
                if best is None or cost < best[0]:
                    best = (cost, moved, candidate)
            assert best is not None
            color, cell = best[1], best[2]
        taken[cell] = name
        values[cell] = color
        out[name] = color
    return out


_FIXED = solve(list(FIXED), FIXED)


def theme_colors(theme: str) -> dict[str, int]:
    """Final RGB565 colour of every name on screen in `theme`."""
    taken = {cube_index(value): name for name, value in _FIXED.items()}
    holders = {cube_index(value): value for value in _FIXED.values()}
    scene = solve(SCENE_KEYS, dict(zip(SCENE_KEYS, THEMES[theme])), taken, holders)
    return {**_FIXED, **scene}


def check(verbose: bool = False) -> int:
    """Distinct cube cells inside each theme: otherwise colours would collide."""
    failures = 0
    for theme in THEMES:
        seen: dict[int, str] = {}
        for name, value in theme_colors(theme).items():
            cell = cube_index(value)
            if cell in seen and seen[cell] != value:
                print(f"palette {theme}: {name} collides in cell {cell}")
                failures += 1
            seen[cell] = value
            if verbose:
                wanted = FIXED.get(name) or THEMES[theme][SCENE_KEYS.index(name)]
                moved = "" if rgb565(wanted) == value else f"  moved, designed {wanted}"
                print(f"{theme:12} {name:8} #{''.join(f'{c:02x}' for c in rgb888(value))} cell {cell}{moved}")
    return failures


if __name__ == "__main__":
    import sys
    n = check("-v" in sys.argv)
    print(f"palette: {len(THEMES)} themes, {len(FIXED)} fixed + {len(SCENE_KEYS)} scene colours, {n} collisions")
    raise SystemExit(1 if n else 0)
