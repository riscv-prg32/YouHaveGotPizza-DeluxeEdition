#!/usr/bin/env python3
"""Hand-drawn sprite sheets of You Have Got Pizza: Deluxe Edition.

Every picture here was drawn for this game, pixel by pixel, as text: one
character per pixel, `.` for transparent. Keeping art in a diff-friendly text
form lets students read, edit and rebuild it. The letters of a sheet name
colours of tools/palette.py through its LEGEND.
"""
from __future__ import annotations

PROF_LEGEND = {"K": "ink", "W": "cream", "S": "skin", "s": "skind", "P": "gown", "p": "gownd",
               "R": "red", "Y": "lemon", "G": "grey", "B": "brown", "D": "dough"}


def _prof(legs: list[str], arm: str = "normal") -> list[str]:
    head = [
        "....KKKKKKKK....",
        "..KKKKKKKKKKKKY.",
        ".....KKKKKK...Y.",
        ".....GSSSSSS..Y.",
        ".....GSKSSKS....",
        ".....GSSSSSs....",
        ".....WWWWWWW....",
        "......WWWWW.....",
    ]
    body = {
        "normal": [
            ".....PPRRPP.....",
            "....PPWWWWPP....",
            "....PsWWWWPs....",
            "....PPWWWWPP....",
            ".....pWWWWp.....",
            ".....pppppp.....",
        ],
        "throw": [
            ".....PPRRPPPPSS.",
            "....PPWWWWPPPSS.",
            "....PPWWWWPP....",
            "....PPWWWWPP....",
            ".....pWWWWp.....",
            ".....pppppp.....",
        ],
    }[arm]
    return head + body + legs


LEGS_STAND = [".....KK..KK.....", ".....BB..BBB...."]
LEGS_A = ["....KK....KK....", "...BB.....BBB..."]
LEGS_B = [".....KKKKK......", ".....BBBBBB....."]

PROF_FRAMES = {
    "stand": _prof(LEGS_STAND),
    "walk1": _prof(LEGS_A),
    "walk2": _prof(LEGS_B),
    "throw": _prof(LEGS_STAND, "throw"),
    "climb1": [
        "....KKKKKKKK....",
        "..KKKKKKKKKKKK..",
        ".....KKKKKK.....",
        ".S...GGGGGG.....",
        ".P...GGGGGG.....",
        ".PP..GGGGGG.....",
        "..PPPPPPPPP.....",
        "...PPPPPPPPP....",
        "...PPPPPPPPPP...",
        "....PPPPPPPPP...",
        "....PPPPPPP.PS..",
        ".....pppppp.PP..",
        ".....pppppp.....",
        ".....KK..pp.....",
        ".....BB..KK.....",
        "..........BB....",
    ],
    "climb2": [
        "....KKKKKKKK....",
        "..KKKKKKKKKKKK..",
        ".....KKKKKK.....",
        ".....GGGGGG...S.",
        ".....GGGGGG...P.",
        ".....GGGGGG..PP.",
        ".....PPPPPPPPP..",
        "....PPPPPPPPP...",
        "...PPPPPPPPPP...",
        "...PPPPPPPPP....",
        "..SP.PPPPPPP....",
        "..PP.pppppp.....",
        ".....pppppp.....",
        ".....pp..KK.....",
        ".....KK..BB.....",
        "....BB..........",
    ],
    "hurt1": [
        "................",
        "..KKKKKKKKKKKK..",
        "....KKKKKKKK..Y.",
        ".....GSSSSSG..Y.",
        ".....SKSSKSS....",
        ".....SSKKSSS....",
        ".S...WWWWWW...S.",
        ".PP...WWWW...PP.",
        "..PPPPPRRPPPPP..",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        ".....pWWWWp.....",
        ".....pppppp.....",
        ".....KK..KK.....",
        "....BBB..BBB....",
    ],
    "hurt2": [
        "..Y.............",
        "..YKKKKKKKKKKK..",
        "....KKKKKKKK....",
        ".....GSSSSSG....",
        ".....SKSSKSS....",
        ".....SSSKKSS....",
        ".....WWWWWW.....",
        "S.....WWWW.....S",
        "PPPPPPPRRPPPPPPP",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        ".....pWWWWp.....",
        ".....pppppp.....",
        "....KK....KK....",
        "...BBB....BBB...",
    ],
    "cheer": [
        ".S..KKKKKKKK..S.",
        ".PKKKKKKKKKKKKP.",
        ".PP..KKKKKK..PP.",
        "..PP.GSSSSG.PP..",
        "...P.SKSSKS.P...",
        "...PPSSSSSSPP...",
        "....PWKKKKWP....",
        ".....WWWWWW.....",
        ".....PPRRPP.....",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        "....PPWWWWPP....",
        ".....pWWWWp.....",
        ".....pppppp.....",
        ".....KK..KK.....",
        ".....BB..BB.....",
    ],
}

# Frames the game needs, with the facing it reads them in. `m:` mirrors.
PROF_SHEET = ["stand", "walk1", "walk2", "throw", "m:stand", "m:walk1", "m:walk2", "m:throw",
              "climb1", "climb2", "hurt1", "hurt2", "cheer"]

STUDENT_LEGEND = {"K": "ink", "H": "hair", "S": "skin", "s": "skind", "T": "top", "t": "topd",
                  "B": "pack", "J": "denim", "W": "cream", "M": "redd", "Y": "lemon",
                  "D": "dough", "R": "red"}


def _student(legs: list[str], face: str = "hungry") -> list[str]:
    head = {
        "hungry": [
            ".....HHHHHH.....",
            "....HHHHHHHH....",
            "....HHSSSSSS....",
            "....HSSKSSKS....",
            "....HSSSSSSs....",
            ".....SSSMMSs....",
            "......SSSSS.....",
        ],
        "happy": [
            ".....HHHHHH.....",
            "....HHHHHHHH....",
            "....HHSSSSSS....",
            "....HSKSSKSS....",
            "....HSSSSSSs....",
            ".....SKSSKSs....",
            "......SKKSS.....",
        ],
        "floured": [
            ".....WWWWWW.....",
            "....WWWWWWWW....",
            "....WWWWWWWW....",
            "....WWKWWKWW....",
            "....WWWWWWWW....",
            ".....WWKKWW.....",
            "......WWWW......",
        ],
    }[face]
    body = [
        "...BBTTTTTT.....",
        "...BBTTTTTTS....",
        "...BBTTTTTTS....",
        "...BBtttttt.....",
    ]
    return ["................"] + head + body + legs


STUDENT_FRAMES = {
    "walk1": _student([".....JJ..JJ.....", ".....JJ..JJ.....", ".....KK..KKK....", "................"]),
    "walk2": _student(["....JJ....JJ....", "...JJ......JJ...", "...KK......KKK..", "................"]),
    "climb1": [
        "................",
        ".....HHHHHH.....",
        "....HHHHHHHH.S..",
        "....HHHHHHHH.T..",
        "....HHHHHHHH.T..",
        ".....HHHHHH.TT..",
        "......SSSS..T...",
        "..BBBBBBBBTTT...",
        "..BBBBBBBBTT....",
        ".TBBBBBBBBT.....",
        ".SBBBBBBBB......",
        "...tttttttt.....",
        ".....JJ..JJ.....",
        ".....JJ..KK.....",
        ".....KK.........",
        "................",
    ],
    "climb2": [
        "................",
        ".....HHHHHH.....",
        ".S..HHHHHHHH....",
        ".T..HHHHHHHH....",
        ".T..HHHHHHHH....",
        ".TT..HHHHHH.....",
        "..T...SSSS......",
        "..TTTBBBBBBBB...",
        "...TTBBBBBBBB...",
        "....TBBBBBBBBT..",
        ".....BBBBBBBBS..",
        ".....tttttttt...",
        ".....JJ..JJ.....",
        ".....KK..JJ.....",
        ".........KK.....",
        "................",
    ],
    "floured1": ["..Y..........Y.."] + _student([".....JJ..JJ.....", ".....JJ..JJ.....", ".....KK..KK.....", "................"], "floured")[1:],
    "floured2": [".......Y........"] + _student([".....JJ..JJ.....", ".....JJ..JJ.....", ".....KK..KK.....", "................"], "floured")[1:],
    "fed1": _student([".....JJ..JJ.....", ".....JJ..JJ.....", ".....KK..KK.....", "................"], "happy"),
    "fed2": [
        "................",
        ".....HHHHHH.....",
        "....HHHHHHHH....",
        "....HHSSSSSS....",
        "....HSKSSKSS....",
        "....HSSSSSSs....",
        ".....SKSSKSs.D..",
        "......SKKSS.DRD.",
        "...BBTTTTTTSDDD.",
        "...BBTTTTTTS.D..",
        "...BBTTTTTT.....",
        "...BBtttttt.....",
        ".....JJ..JJ.....",
        ".....JJ..JJ.....",
        ".....KK..KK.....",
        "................",
    ],
}
STUDENT_SHEET = ["walk1", "walk2", "m:walk1", "m:walk2", "climb1", "climb2", "floured1", "floured2", "fed1", "fed2"]

# The three kinds of student share one sheet: only the palette changes.
STUDENT_KINDS = {
    "fresher": {"hair": "brown", "top": "red", "topd": "redd", "pack": "denim"},
    "ambusher": {"hair": "ink", "top": "orange", "topd": "crust", "pack": "basild"},
    "wanderer": {"hair": "crust", "top": "basil", "topd": "basild", "pack": "gown"},
}

ITEM_LEGEND = {"K": "ink", "W": "cream", "L": "lemon", "l": "cheesed", "G": "basil", "g": "basild",
               "B": "brown", "c": "crust", "R": "red", "r": "redd", "D": "denim", "C": "cyan",
               "d": "dough", "O": "orange", "A": "grey"}

ITEM_FRAMES = {
    "espresso": [
        "................",
        "......W...W.....",
        ".......W...W....",
        "......W...W.....",
        "................",
        "...KKKKKKKKKK...",
        "..KWWWWWWWWWWK..",
        "..KWBBBBBBBBWKK.",
        "..KWWWWWWWWWWK.K",
        "..KWWWWWWWWWWK.K",
        "...KWWWWWWWWKKK.",
        "...KWWWWWWWWK...",
        "....KKKKKKKK....",
        ".KKKKKKKKKKKKKK.",
        ".KAAAAAAAAAAAAK.",
        "..KKKKKKKKKKKK..",
    ],
    "lemon": [
        "................",
        "..........Gg....",
        ".........GGg....",
        "........GGg.....",
        ".....KKKKKK.....",
        "...KKLLLLLLKK...",
        "..KLLWLLLLLLlK..",
        ".KLLWLLLLLLLLlK.",
        ".KLLLLLLLLLLLlK.",
        "KLLLLLLLLLLLLllK",
        ".KLLLLLLLLLLllK.",
        ".KLLLLLLLLLlllK.",
        "..KLLLLLLLllK...",
        "...KKllllllKK...",
        ".....KKKKKK.....",
        "................",
    ],
    "book": [
        "................",
        "..KKKKKKKKKKKK..",
        ".KRRRRRRRRRRRRK.",
        ".KRRRRRRRRRRRRK.",
        ".KRRWWWWWWWWRRK.",
        ".KRRWKKKKKKWRRK.",
        ".KRRWWWWWWWWRRK.",
        ".KRRRRRRRRRRRRK.",
        ".KRRRRRRRRRRRRK.",
        ".KRRRLLLLLLRRRK.",
        ".KRRRRRRRRRRRRK.",
        ".KrrrrrrrrrrrrK.",
        ".KWWWWWWWWWWWWK.",
        ".KWWWWWWWWWWWWK.",
        "..KKKKKKKKKKKK..",
        "................",
    ],
    "mozzarella": [
        "................",
        "................",
        ".....KKKKKK.....",
        "...KKWWWWWWKK...",
        "..KWWWWWWWWWWK..",
        ".KWWWWWWWWWWWdK.",
        ".KWWWWWWWWWWWdK.",
        ".KWWWWWWWWWWddK.",
        ".KWWWWWWWWWWddK.",
        ".KWWWWWWWWWdddK.",
        "..KWWWWWWdddgK..",
        "...KKddddGGGgK..",
        ".....KKKGGGGGK..",
        ".......KKGGgK...",
        ".........KKK....",
        "................",
    ],
}
ITEM_SHEET = ["espresso", "lemon", "book", "mozzarella"]

PUFF_FRAMES = {
    "puff1": [
        "................",
        "................",
        "................",
        ".....WW.........",
        "....WWWW.WW.....",
        "...WWWWWWWWW....",
        "..WWWWWWWWWWW...",
        "..WWWWWWWWWWWW..",
        "...WWWWWWWWWW...",
        "....WWWWWWWW....",
        ".....WW.WWW.....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    "puff2": [
        "................",
        ".......W........",
        "..W........W....",
        "......W.........",
        "...W.WWW..WW....",
        ".....WWWW.......",
        ".W..WWWWWW..W...",
        "....WWW.WWW.....",
        "..W..WWWW....W..",
        ".....W.WW.W.....",
        "...W.......W....",
        "......W.........",
        ".W.........W....",
        ".......W........",
        "................",
        "................",
    ],
}
PUFF_SHEET = ["puff1", "puff2", "m:puff1"]
PUFF_LEGEND = {"W": "cream"}

# Ingredient layers: 8 x 6 pieces. A layer is left + 3 x middle + right.
# Letters: dough, sauce, cheese, basil.
LAYER_LEGEND = {"K": "ink", "D": "dough", "C": "crust", "W": "cream", "R": "red", "r": "redd",
                "Y": "cheese", "y": "cheesed", "G": "basil", "g": "basild", "L": "lemon"}
LAYERS = {
    "dough": {
        "left": ["..KKKKKK", ".KDDWDDD", "KDDDDDDD", "KDDDDDDC", "KCCDDCCC", ".KKKKKKK"],
        "mid": ["KKKKKKKK", "DDDDWDDD", "DWDDDDDD", "DDDDDDDC", "CCDCCCCC", "KKKKKKKK"],
        "right": ["KKKKKK..", "DDDWDDK.", "DDDDDDDK", "DDDDDCCK", "CCCCCCCK", "KKKKKKK."],
    },
    "sauce": {
        "left": ["..KKKKKK", ".KRRRRRR", "KRRWRRRR", "KRRRRRRR", "KrRRrrRR", ".KKKKKrK"],
        "mid": ["KKKKKKKK", "RRRRRRRR", "RRRRRWRR", "RRRRRRRR", "rrRRRrrR", "KKrKKKKK"],
        "right": ["KKKKKK..", "RRRRRRK.", "RRRRRRRK", "RRRRRRrK", "RRrrrRrK", "KKKKKKK."],
    },
    "cheese": {
        "left": ["..KKKKKK", ".KWYYYYY", "KYYYYYyY", "KYYYYYYY", "KyYYyyYy", ".KKyKKyK"],
        "mid": ["KKKKKKKK", "YYYWYYYY", "YyYYYYYY", "YYYYYyYY", "yYyYYyYY", "KyKKyKKy"],
        "right": ["KKKKKK..", "YYYYWYK.", "YYYYYYYK", "YYyYYYYK", "yYYyYYyK", "KyKKKKK."],
    },
    "basil": {
        "left": ["...KKKK.", "..KGGGGK", ".KGGgGGG", "KKGGGGGg", "KRRRRRRR", ".KKKKKKK"],
        "mid": [".KKK.KKK", "KGGGKGGG", "GGgGGGgG", "GGGGGGGG", "RRRRRRRR", "KKKKKKKK"],
        "right": [".KKKK...", "KGGGGK..", "GGgGGK..", "gGGGGGKK", "RRRRRRRK", "KKKKKKK."],
    },
}
LAYER_ORDER = ["dough", "sauce", "cheese", "basil"]

# A whole baked pizza, seen from above, 24 x 24: the prize of a full stack.
PIZZA = [
    "........KKKKKKKK........",
    ".....KKKCCCCCCCCKKK.....",
    "...KKCCCCDDDDDDCCCCKK...",
    "..KCCCDDDYYYYYYDDDCCCK..",
    "..KCCDDYYYYYYYYYYDDCCK..",
    ".KCCDYYYRRYYYYYYYYYDCCK.",
    ".KCDYYYRRRRYYYGGYYYYDCK.",
    "KCCDYYYRRRRYYGGgGYYYDCCK",
    "KCDYYYYYRRYYYYGGYYYYYDCK",
    "KCDYYYYYYYYYYYYYYYRRYDCK",
    "KCDYYGGYYYYYYYYYYRRRRDCK",
    "KCDYGGgGYYYRRYYYYRRRRDCK",
    "KCDYYGGYYYRRRRYYYYRRYDCK",
    "KCDYYYYYYYRRRRYYYYYYYDCK",
    "KCDYYYYYYYYRRYYYYYYYYDCK",
    "KCCDYYRRYYYYYYYYGGYYDCCK",
    ".KCDYRRRRYYYYYYGGgGYDCK.",
    ".KCCDRRRRYYYYYYYGGYDCCK.",
    "..KCCDYRRYYYYYYYYYDCCK..",
    "..KCCCDDYYYYYYYYDDCCCK..",
    "...KKCCCDDDDDDDDCCCKK...",
    ".....KKKCCCCCCCCKKK.....",
    "........KKKKKKKK........",
    "........................",
]
PIZZA_LEGEND = LAYER_LEGEND

# Original 8 x 8 chunky alphabet (7 x 7 glyph, one blank column and row).
FONT = {
    " ": "       |       |       |       |       |       |       ",
    "A": "  ###  | ## ## |##   ##|##   ##|#######|##   ##|##   ##",
    "B": "###### |##   ##|##   ##|###### |##   ##|##   ##|###### ",
    "C": " ##### |##   ##|##     |##     |##     |##   ##| ##### ",
    "D": "#####  |##  ## |##   ##|##   ##|##   ##|##  ## |#####  ",
    "E": "#######|##     |##     |#####  |##     |##     |#######",
    "F": "#######|##     |##     |#####  |##     |##     |##     ",
    "G": " ##### |##   ##|##     |## ####|##   ##|##   ##| ######",
    "H": "##   ##|##   ##|##   ##|#######|##   ##|##   ##|##   ##",
    "I": " ######|   ##  |   ##  |   ##  |   ##  |   ##  | ######",
    "J": "  #####|    ## |    ## |    ## |##  ## |##  ## | ####  ",
    "K": "##   ##|##  ## |## ##  |####   |## ##  |##  ## |##   ##",
    "L": "##     |##     |##     |##     |##     |##     |#######",
    "M": "##   ##|### ###|#######|## # ##|##   ##|##   ##|##   ##",
    "N": "##   ##|###  ##|#### ##|## ####|##  ###|##   ##|##   ##",
    "O": " ##### |##   ##|##   ##|##   ##|##   ##|##   ##| ##### ",
    "P": "###### |##   ##|##   ##|###### |##     |##     |##     ",
    "Q": " ##### |##   ##|##   ##|##   ##|## # ##|##  ## | #### #",
    "R": "###### |##   ##|##   ##|###### |## ##  |##  ## |##   ##",
    "S": " ##### |##   ##|##     | ##### |     ##|##   ##| ##### ",
    "T": "#######|   ##  |   ##  |   ##  |   ##  |   ##  |   ##  ",
    "U": "##   ##|##   ##|##   ##|##   ##|##   ##|##   ##| ##### ",
    "V": "##   ##|##   ##|##   ##|##   ##| ## ## |  ###  |   #   ",
    "W": "##   ##|##   ##|##   ##|## # ##|#######|### ###|##   ##",
    "X": "##   ##| ## ## |  ###  |  ###  |  ###  | ## ## |##   ##",
    "Y": "##   ##|##   ##| ## ## |  ###  |   ##  |   ##  |   ##  ",
    "Z": "#######|    ## |   ##  |  ##   | ##    |##     |#######",
    "0": " ##### |##  ###|## ####|#### ##|###  ##|##   ##| ##### ",
    "1": "  ###  | ####  |   ##  |   ##  |   ##  |   ##  | ######",
    "2": " ##### |##   ##|     ##|  #### | ##    |##     |#######",
    "3": " ##### |##   ##|     ##|  #### |     ##|##   ##| ##### ",
    "4": "   ### |  #### | ## ## |##  ## |#######|    ## |    ## ",
    "5": "#######|##     |###### |     ##|     ##|##   ##| ##### ",
    "6": " ##### |##     |##     |###### |##   ##|##   ##| ##### ",
    "7": "#######|     ##|    ## |   ##  |  ##   |  ##   |  ##   ",
    "8": " ##### |##   ##|##   ##| ##### |##   ##|##   ##| ##### ",
    "9": " ##### |##   ##|##   ##| ######|     ##|     ##| ##### ",
    ":": "       |  ##   |  ##   |       |  ##   |  ##   |       ",
    "!": "   ##  |   ##  |   ##  |   ##  |   ##  |       |   ##  ",
    "?": " ##### |##   ##|    ## |   ##  |   ##  |       |   ##  ",
    ".": "       |       |       |       |       |  ##   |  ##   ",
    ",": "       |       |       |       |  ##   |  ##   | ##    ",
    "-": "       |       |       | ##### |       |       |       ",
    "+": "       |   ##  |   ##  | ######|   ##  |   ##  |       ",
    "/": "     ##|    ## |   ##  |  ##   | ##    |##     |       ",
    "'": "   ##  |   ##  |  ##   |       |       |       |       ",
    "x": "       |##   ##| ## ## |  ###  | ## ## |##   ##|       ",
}
FONT_ORDER = list(" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:!?.,-+/'x")


def glyph_bytes(ch: str) -> list[int]:
    rows = FONT[ch].split("|")
    out = []
    for row in rows:
        value = 0
        for i, c in enumerate(row):
            if c == "#":
                value |= 0x80 >> i
        out.append(value)
    return out + [0]
