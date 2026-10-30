#!/usr/bin/env python3
"""The six lessons of the game: geometry, recipe columns and scene theme.

Cells are 8 x 8 pixels on the 40 x 25 tile screen. Floors sit on tile rows
4, 8, 12, 16 and 20 (floor index 0 is the top one); the serving peels sit on row 24.
A ladder is two cells wide. A pizza column is five cells wide; its layers are
listed top floor first, so the dough, which must land first, is on the lowest
floor. tests/test_core.c proves every level is reachable and solvable.
"""
from __future__ import annotations

FLOOR_ROWS = [4, 8, 12, 16, 20]
PLATE_ROW = 24

# floors: (floor, first cell, last cell). ladders: (cell, top floor, bottom floor).
# pizzas: (left cell, {floor: ingredient}), where 0 dough 1 sauce 2 cheese 3 basil.
LEVELS = [
    {
        "name": "PIAZZA DELL'ATENEO", "theme": "piazza", "scene": "piazza",
        "floors": [(f, 1, 38) for f in range(5)],
        "ladders": [(2, 0, 4), (17, 0, 2), (21, 2, 4), (36, 0, 4), (11, 1, 3), (28, 1, 3)],
        "pizzas": [(5, {0: 3, 1: 2, 3: 1, 4: 0}), (30, {0: 3, 2: 2, 3: 1, 4: 0})],
        "spawn": (19, 4), "students": [(3, 0), (36, 0)],
    },
    {
        "name": "MERCATO DELLA PIGNASECCA", "theme": "mercato", "scene": "mercato",
        "floors": [(0, 1, 38), (1, 1, 24), (1, 28, 38), (2, 1, 38), (3, 10, 38), (4, 1, 38)],
        "ladders": [(2, 0, 2), (2, 2, 4), (12, 0, 1), (10, 2, 4), (25, 2, 4), (36, 0, 4), (20, 0, 3)],
        "pizzas": [(4, {0: 3, 1: 2, 2: 1, 4: 0}), (14, {0: 3, 1: 2, 2: 1, 3: 0}), (30, {0: 3, 1: 2, 2: 1, 3: 0})],
        "spawn": (20, 4), "students": [(3, 0), (37, 0)],
    },
    {
        "name": "LUNGOMARE AL TRAMONTO", "theme": "lungomare", "scene": "lungomare",
        "floors": [(0, 4, 35), (1, 1, 38), (2, 1, 16), (2, 23, 38), (3, 1, 38), (4, 1, 38)],
        "ladders": [(5, 0, 1), (34, 0, 1), (2, 1, 4), (37, 1, 4), (14, 1, 3), (25, 1, 3), (23, 3, 4), (23, 0, 1)],
        "pizzas": [(7, {0: 3, 1: 2, 2: 1, 3: 0}), (27, {0: 3, 1: 2, 2: 1, 3: 0}), (17, {0: 3, 1: 2, 3: 1, 4: 0})],
        "spawn": (31, 4), "students": [(6, 0), (35, 0), (2, 3)],
    },
    {
        "name": "VICOLO DEI PANNI STESI", "theme": "vicolo", "scene": "vicolo",
        "floors": [(0, 1, 38), (1, 1, 11), (1, 15, 38), (2, 1, 27), (2, 30, 38), (3, 1, 38), (4, 1, 38)],
        "ladders": [(2, 0, 1), (9, 1, 2), (2, 2, 4), (13, 0, 3), (24, 1, 3), (28, 0, 1), (36, 1, 4), (20, 3, 4), (36, 0, 1)],
        "pizzas": [(4, {0: 3, 1: 2, 2: 1, 4: 0}), (15, {0: 3, 2: 2, 3: 1, 4: 0}), (31, {0: 3, 1: 2, 2: 1, 3: 0})],
        "spawn": (11, 4), "students": [(3, 0), (37, 0), (37, 4)],
    },
    {
        "name": "LABORATORIO RISC-V", "theme": "laboratorio", "scene": "laboratorio",
        "floors": [(f, 1, 38) for f in range(5)],
        "ladders": [(2, 0, 1), (2, 3, 4), (10, 1, 3), (18, 0, 2), (20, 2, 4), (28, 1, 3), (36, 0, 1), (36, 3, 4)],
        "pizzas": [(4, {0: 3, 1: 2, 2: 1, 3: 0}), (12, {1: 3, 2: 2, 3: 1, 4: 0}), (22, {0: 3, 1: 2, 3: 1, 4: 0}), (30, {0: 3, 2: 2, 3: 1, 4: 0})],
        "spawn": (17, 4), "students": [(3, 0), (37, 0), (3, 4)],
    },
    {
        "name": "IL FORNO DI MEZZANOTTE", "theme": "forno", "scene": "forno",
        "floors": [(0, 1, 38), (1, 1, 17), (1, 22, 38), (2, 1, 38), (3, 1, 15), (3, 23, 38), (4, 1, 38)],
        "ladders": [(2, 0, 2), (37, 0, 2), (9, 1, 2), (29, 1, 2), (17, 2, 4), (21, 2, 4), (2, 2, 4), (37, 2, 4), (16, 0, 1)],
        "pizzas": [(4, {0: 3, 1: 2, 2: 1, 3: 0}), (11, {0: 3, 2: 2, 3: 1, 4: 0}), (23, {0: 3, 2: 2, 3: 1, 4: 0}), (31, {0: 3, 1: 2, 2: 1, 4: 0})],
        "spawn": (20, 4), "students": [(2, 0), (38, 0), (2, 4), (38, 4)],
    },
]


def lint() -> list[str]:
    """Static checks; the deeper reachability proof is in tests/test_core.c."""
    errors = []
    for number, level in enumerate(LEVELS, 1):
        where = f"level {number}"
        floors: dict[int, set[int]] = {f: set() for f in range(5)}
        for f, a, b in level["floors"]:
            floors[f].update(range(a, b + 1))
        ladders = [(c, a, b) for c, a, b in level["ladders"]]
        for c, a, b in ladders:
            for f in (a, b):
                if not {c, c + 1} <= floors[f]:
                    errors.append(f"{where}: ladder {c} has no floor at {f}")
            for f in range(a, b + 1):
                if len({c, c + 1} & floors[f]) == 1:
                    errors.append(f"{where}: ladder {c} half-touches floor {f}")
        for left, recipe in level["pizzas"]:
            if sorted(recipe.values()) != [0, 1, 2, 3]:
                errors.append(f"{where}: pizza {left} is not a full recipe")
            ordered = [recipe[f] for f in sorted(recipe)]
            if ordered != [3, 2, 1, 0]:
                errors.append(f"{where}: pizza {left} would stack out of order")
            for f in recipe:
                span = set(range(left, left + 5))
                if not span <= floors[f]:
                    errors.append(f"{where}: pizza {left} floats on floor {f}")
                for c, a, b in ladders:
                    if a <= f <= b and {c, c + 1} & span:
                        errors.append(f"{where}: ladder {c} crosses pizza {left} on floor {f}")
        lefts = sorted(l for l, _ in level["pizzas"])
        for a, b in zip(lefts, lefts[1:]):
            if b - a < 6:
                errors.append(f"{where}: pizzas {a} and {b} touch")
    return errors


if __name__ == "__main__":
    problems = lint()
    print("\n".join(problems) or f"levels: {len(LEVELS)} lessons OK")
    raise SystemExit(1 if problems else 0)
