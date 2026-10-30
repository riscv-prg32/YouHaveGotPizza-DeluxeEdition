#!/usr/bin/env python3
"""Procedural backdrops for the six lessons and the title screen.

Each painter draws on a 320 x 200 canvas of palette names with Pillow-free
integer primitives, so the result depends on nothing but this file. The art
direction is the same everywhere: a sky ramp dithered by a fixed pattern, flat
architecture in a narrow low-contrast range behind the action, and warm lights.
The characters, whose colours are the bright and saturated end of the palette,
then always stand out. See docs/art.md.
"""
from __future__ import annotations

import math

W, H = 320, 200
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


class Canvas:
    def __init__(self, fill: str = "s0"):
        self.px = [[fill] * W for _ in range(H)]

    def put(self, x: int, y: int, c: str) -> None:
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = c

    def rect(self, x: int, y: int, w: int, h: int, c: str) -> None:
        for py in range(max(y, 0), min(y + h, H)):
            for px in range(max(x, 0), min(x + w, W)):
                self.px[py][px] = c

    def ramp(self, y0: int, y1: int, a: str, b: str) -> None:
        """Vertical ordered-dither blend from a to b."""
        for y in range(y0, y1):
            level = (y - y0) * 16 // max(y1 - y0, 1)
            for x in range(W):
                self.px[y][x] = b if level > BAYER[y & 3][x & 3] else a

    def poly(self, points: list[tuple[int, int]], c: str) -> None:
        ys = [p[1] for p in points]
        for y in range(max(min(ys), 0), min(max(ys) + 1, H)):
            xs = []
            for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]):
                if (y0 <= y < y1) or (y1 <= y < y0):
                    xs.append(x0 + (y - y0) * (x1 - x0) // (y1 - y0))
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                self.rect(a, y, b - a + 1, 1, c)

    def disc(self, cx: int, cy: int, r: int, c: str) -> None:
        for y in range(-r, r + 1):
            half = int(math.sqrt(r * r - y * y))
            self.rect(cx - half, cy + y, 2 * half + 1, 1, c)

    def arch(self, x: int, y: int, w: int, h: int, c: str) -> None:
        """A round-headed opening: the signature shape of the tuff city."""
        r = w // 2
        self.rect(x, y + r, w, h - r, c)
        for py in range(r):
            half = int(math.sqrt(max(r * r - (r - py) ** 2, 0)))
            self.rect(x + r - half, y + py, 2 * half, 1, c)

    def text(self, x: int, y: int, s: str, c: str, glyph, scale: int = 1) -> None:
        for ch in s:
            for row, bits in enumerate(glyph(ch)):
                for col in range(8):
                    if bits & (0x80 >> col):
                        self.rect(x + col * scale, y + row * scale, scale, scale, c)
            x += 8 * scale


def _building(c: Canvas, x: int, y: int, w: int, wall: str, shade: str, win: str, step: int = 16) -> None:
    c.rect(x, y, w, H - y, wall)
    c.rect(x + w - 2, y, 2, H - y, shade)
    for wy in range(y + 6, H, step):
        for wx in range(x + 4, x + w - 8, 12):
            c.arch(wx, wy, 6, 10, win)


def piazza(c: Canvas) -> None:
    c.ramp(0, 110, "s1", "s0")
    # Far roofs, then the university front with its arcade.
    _building(c, 0, 66, 54, "s3", "s4", "s4")
    _building(c, 266, 58, 54, "s3", "s4", "s4")
    c.rect(54, 38, 212, 162, "s2")
    c.poly([(48, 38), (160, 14), (272, 38)], "s3")
    c.poly([(82, 36), (160, 20), (238, 36)], "s2")
    c.disc(160, 29, 5, "s4")
    c.rect(54, 38, 212, 3, "s3")
    for row, top in enumerate((47, 82, 118, 150)):
        for x in range(62, 256, 24):
            c.arch(x, top, 14, 24, "s4" if row else "s3")
            c.rect(x + 2, top + 12, 10, 12, "s5" if row else "s4")
        c.rect(54, top + 26, 212, 2, "s3")
    for x in (60, 108, 156, 204, 252):
        c.rect(x, 41, 4, 159, "s3")
    c.rect(0, 186, W, 14, "s5")


def mercato(c: Canvas) -> None:
    c.ramp(0, 80, "s1", "s0")
    for i, (x, w, h) in enumerate(((0, 70, 46), (70, 60, 26), (130, 74, 52), (204, 56, 30), (260, 60, 44))):
        _building(c, x, h, w, "s3" if i % 2 else "s2", "s4", "s4")
    for row, y in enumerate((70, 106, 142)):
        for x in range(-8 + 20 * row, W, 72):
            for k in range(0, 48, 8):
                c.poly([(x + k, y), (x + k + 8, y), (x + k + 10, y + 12), (x + k - 2, y + 12)], "s9" if (k // 8) % 2 else "s6")
            c.rect(x - 2, y + 12, 52, 1, "s5")
            c.rect(x + 1, y + 13, 2, 20, "s5")
            c.rect(x + 45, y + 13, 2, 20, "s5")
            for k in range(6, 44, 7):
                c.disc(x + k, y + 25, 3, "s8" if (k + row) % 2 else "s6")
            c.rect(x + 3, y + 27, 42, 6, "s4")
    c.rect(0, 186, W, 14, "s5")


def lungomare(c: Canvas) -> None:
    c.ramp(0, 60, "s1", "s0")
    c.ramp(60, 104, "s0", "s2")
    c.disc(250, 78, 15, "s9")
    # The volcano is unmistakably a volcano, and just a volcano.
    c.poly([(40, 112), (118, 50), (132, 55), (148, 46), (232, 112)], "s3")
    c.poly([(70, 112), (118, 70), (134, 76), (146, 68), (205, 112)], "s4")
    c.rect(0, 104, W, 96, "s5")
    for y in range(104, 200, 6):
        offset = (y * 7) % 23
        for x in range(offset, W, 34):
            c.rect(x, y, 12 + (y % 9), 1, "s8")
            if y < 130:
                c.rect(x + 5, y + 2, 9, 1, "s9")
    for x, y, s in ((36, 126, 3), (268, 148, 4), (172, 132, 2)):
        c.poly([(x, y), (x + 8 * s, y), (x + 6 * s, y + 2 * s), (x + 2 * s, y + 2 * s)], "s7")
        c.poly([(x + 4 * s, y - 1), (x + 4 * s, y - 7 * s), (x + 8 * s, y - 1)], "s9")
    c.rect(0, 190, W, 10, "s7")


def vicolo(c: Canvas) -> None:
    c.ramp(0, 64, "s0", "s1")
    c.poly([(0, 0), (82, 18), (82, 200), (0, 200)], "s4")
    c.poly([(320, 0), (238, 18), (238, 200), (320, 200)], "s4")
    c.rect(82, 22, 156, 178, "s3")
    c.rect(82, 22, 156, 4, "s5")
    c.rect(120, 0, 80, 22, "s1")
    for y in range(34, 190, 36):
        for x in (10, 44, 92, 142, 192, 250, 284):
            lit = (x * 3 + y) % 5 == 0
            c.rect(x, y, 16, 22, "s5")
            c.rect(x + 2, y + 2, 12, 18, "s9" if lit else "s6" if (x + y) % 3 == 0 else "s7")
            c.rect(x - 2, y + 22, 20, 2, "s2")
    # Washing lines: a signature of the alley, in the ink of the cloth colours.
    for y, sag in ((28, 4), (62, 5), (98, 4), (134, 5)):
        for x in range(0, W):
            c.put(x, y + int(sag * math.sin(x / 40.0)), "s8")
        for x in range(12 + y % 21, W, 44):
            ly = y + int(sag * math.sin(x / 40.0)) + 1
            c.rect(x, ly, 14, 12, "s2" if x % 3 else "s8")
            c.rect(x + 14, ly, 6, 8, "s6")
    # A little street shrine with its lamp.
    c.rect(152, 150, 16, 26, "s5")
    c.arch(155, 153, 10, 19, "s9")
    c.rect(0, 190, W, 10, "s5")


def laboratorio(c: Canvas, glyph) -> None:
    c.rect(0, 0, W, H, "s5")
    for y in range(0, H, 16):
        c.rect(0, y, W, 1, "s4")
    for x in range(0, W, 16):
        c.rect(x, 0, 1, H, "s4")
    # Printed-circuit traces, then the chips of the teaching board.
    for k, y in enumerate(range(14, 190, 18)):
        x = 6 + (k * 37) % 50
        while x < 300:
            run = 18 + (x * 13 + k * 7) % 36
            c.rect(x, y, run, 1, "s3")
            c.disc(x + run, y, 1, "s2")
            c.rect(x + run, y, 1, 9 if (x + k) % 2 else 1, "s3")
            x += run + 14
    for x, y, w, h in ((14, 40, 44, 22), (262, 40, 44, 22), (14, 104, 44, 22), (262, 104, 44, 22)):
        c.rect(x, y, w, h, "s7")
        for k in range(x + 2, x + w - 2, 5):
            c.rect(k, y - 3, 2, 3, "s8")
            c.rect(k, y + h, 2, 3, "s8")
    # Two blackboards between the floors; src/pizza.c writes on them at run time
    # with font tiles, which cost no extra ROM and stay perfectly crisp.
    c.rect(72, 40, 176, 24, "ink")
    c.rect(72, 104, 176, 24, "ink")
    for y in (39, 64, 103, 128):
        c.rect(70, y, 180, 1, "s6")
    c.rect(0, 190, W, 10, "s7")


def forno(c: Canvas) -> None:
    c.rect(0, 0, W, H, "s1")
    for row, y in enumerate(range(0, H, 8)):
        for x in range(-16 + 8 * (row % 2), W, 16):
            c.rect(x, y, 15, 7, "s0" if (x + y) % 3 else "s4")
    # The dome of the oven, its mouth and the fire.
    c.disc(160, 116, 86, "s3")
    c.disc(160, 116, 80, "s2")
    for a in range(0, 12):
        angle = math.pi + a * math.pi / 12
        c.rect(160 + int(82 * math.cos(angle)), 116 + int(82 * math.sin(angle)), 4, 3, "s3")
    c.arch(106, 80, 108, 92, "s7")
    c.arch(114, 90, 92, 82, "s4")
    for x, h in ((122, 22), (136, 38), (150, 28), (166, 44), (182, 26), (194, 18)):
        c.poly([(x - 8, 172), (x, 172 - h), (x + 8, 172)], "s6")
    for x, h in ((130, 12), (150, 16), (168, 24), (184, 12)):
        c.poly([(x - 5, 172), (x, 172 - h), (x + 5, 172)], "s9")
    c.rect(96, 172, 128, 6, "s5")
    # Stacked oak logs at both sides.
    for x in (8, 236):
        for row in range(5):
            for k in range(6 - row):
                c.disc(x + 7 + k * 14 + row * 7, 182 - row * 12, 6, "s4")
                c.disc(x + 7 + k * 14 + row * 7, 182 - row * 12, 3, "s6")
    c.rect(0, 190, W, 10, "s7")


def title(c: Canvas) -> None:
    c.ramp(0, 112, "s0", "s1")
    # Stars and moon, then the skyline and the bay.
    for k in range(70):
        c.put((k * 137) % W, (k * 61) % 88, "s9" if k % 3 else "s8")
    c.disc(266, 30, 13, "s9")
    c.disc(260, 26, 11, "s1")
    c.poly([(0, 150), (70, 96), (84, 100), (98, 90), (182, 150)], "s3")
    for x, w, h in ((150, 34, 118), (184, 26, 104), (210, 40, 124), (250, 28, 110), (278, 42, 120)):
        c.rect(x, h, w, 200 - h, "s4")
        for wy in range(h + 5, 170, 10):
            for wx in range(x + 3, x + w - 4, 7):
                c.rect(wx, wy, 3, 5, "s9" if (wx * 7 + wy) % 5 == 0 else "s5")
    c.rect(0, 150, W, 50, "s5")
    for y in range(152, 200, 5):
        for x in ((y * 11) % 27, ):
            for k in range(x, W, 41):
                c.rect(k, y, 14, 1, "s8")
