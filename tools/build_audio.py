#!/usr/bin/env python3
"""Compose and encode the soundtrack of You Have Got Pizza: Deluxe Edition.

  python3 tools/build_audio.py

Outputs (deterministic; both are committed):
  audio/audio.json   the SID-like instruments, for PRG32's AUD0 packer
  src/gen/music.inc  patterns, order lists and effects for src/pz_music.h

All music here is original, written for this game in the idiom of a southern
Italian 6/8 dance: a hand-composed melody on a chord chart, with accompaniment
derived from the chart by the fixed rules below. No existing tune is quoted, no
sample is used: every sound is an oscillator of the PRG32 synth. See docs/audio.md.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STEPS = 12   # one pattern: two bars of 6/8, in eighth notes
HOLD, OFF = 0, 1

NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6,
        "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}

# PRG32_AUDIO_SYNTH_ID: waveform 0 triangle, 1 saw, 2 pulse, 3 noise.
def synth(wave: int, width: int, cutoff: int, resonance: int) -> int:
    return 0x8000 | (resonance << 10) | (cutoff << 6) | (width << 2) | wave

TRI, SAW, PULSE, NOISE = 0, 1, 2, 3

# name, wave, pulse width, cutoff, resonance, attack, decay, sustain, release.
# Envelope bytes follow the firmware law: time_ms = 1 + v * v * 2000 / 65025.
INSTRUMENTS = [
    ("bass", TRI, 8, 9, 1, 0, 70, 90, 24),
    ("pluck_l", PULSE, 5, 14, 2, 0, 56, 0, 28),
    ("pluck_r", PULSE, 10, 14, 2, 0, 56, 0, 28),
    ("lead_a", PULSE, 3, 15, 2, 2, 72, 150, 34),
    ("lead_b", PULSE, 5, 15, 2, 2, 72, 150, 34),
    ("lead_c", PULSE, 7, 15, 2, 2, 72, 150, 34),
    ("arp", SAW, 8, 14, 3, 0, 40, 0, 14),
    ("tammorra", NOISE, 8, 3, 2, 0, 42, 0, 10),
    ("jingle", NOISE, 8, 15, 0, 0, 16, 0, 5),
    ("echo", PULSE, 8, 12, 1, 3, 56, 0, 24),
    ("pad", TRI, 8, 13, 0, 22, 70, 170, 70),
    ("sfx_blip", PULSE, 6, 14, 0, 0, 28, 0, 10),
    ("sfx_thud", NOISE, 8, 2, 2, 0, 36, 0, 12),
    ("sfx_chime", TRI, 8, 15, 0, 0, 62, 0, 40),
    ("sfx_zap", SAW, 8, 11, 3, 0, 34, 0, 12),
    ("sfx_puff", NOISE, 8, 12, 0, 4, 36, 0, 22),
]
INST = {row[0]: i for i, row in enumerate(INSTRUMENTS)}

CHORDS = {"Am": (9, "min"), "E7": (4, "dom"), "E": (4, "maj"), "G": (7, "maj"), "F": (5, "maj"), "Bb": (10, "maj"),
          "C": (0, "maj"), "Dm": (2, "min"), "A": (9, "maj"), "F#m": (6, "min"), "C#m": (1, "min"), "D": (2, "maj"),
          "Bm": (11, "min")}
TONES = {"min": (0, 3, 7), "maj": (0, 4, 7), "dom": (0, 4, 7, 10)}


def midi(token: str) -> int:
    name, octave = token[:-1], int(token[-1])
    return 12 * (octave + 1) + NOTE[name]


def melody(line: str) -> list[int]:
    """'A4 C5 E5:3 -:2' into one byte per step."""
    out: list[int] = []
    for token in line.split():
        name, _, dur = token.partition(":")
        length = int(dur or 1)
        if name == "-":
            out += [OFF] + [HOLD] * (length - 1)
        else:
            out += [midi(name)] + [HOLD] * (length - 1)
    if len(out) != STEPS:
        raise SystemExit(f"pattern {line!r} has {len(out)} steps, not {STEPS}")
    return out


# ---- the themes ---------------------------------------------------------

PIAZZA = {
    "fps": 5, "loop": True,
    "chart": ["Am", "E7", "Am", "G", "F", "Bb", "E7", "Am", "C", "G", "F", "C", "Dm", "Am", "Bb", "E7"],
    "lead": [
        "A4 C5 E5 A5:3 E5 D5 C5 A4:3",
        "G#4 B4 D5 E5:3 D5 C5 B4 G#4:3",
        "A4 C5 E5 A5:3 C6 B5 A5 E5:3",
        "D5 G5 B5 D6:3 B5 A5 G5 D5:3",
        "C5 F5 A5 C6:3 A5 G5 F5 C5:3",
        "D5 F5 Bb5 D6:3 Bb5 A5 F5 D5:3",
        "B4 E5 G#5 B5:2 A5 G#5 E5 D5 B4:2 G#4",
        "A5:3 E5:3 A4:3 -:3",
        "E5:2 G5 C6:3 G5 F5 E5 C5:3",
        "D5:2 G5 B5:3 G5 F5 E5 D5:3",
        "C5:2 F5 A5:3 F5 E5 D5 C5:3",
        "E5 G5 C6 E6:3 C6 G5 E5 C5:3",
        "D5:2 F5 A5:3 F5 E5 D5 A4:3",
        "C5:2 E5 A5:3 E5 D5 C5 A4:3",
        "Bb4:2 D5 F5:3 D5 C5 Bb4 F4:3",
        "G#4 B4 E5 G#5:3 E5 D5 B4 G#4 B4 D5",
    ],
}

MERCATO = {
    "fps": 4, "loop": True,
    "chart": ["C", "Am", "F", "G", "C", "F", "G", "C", "Am", "E", "F", "C", "Dm", "G", "C", "G"],
    "lead": [
        "G5 E5 C5 G4:3 C5 E5 G5 C6:3",
        "A5 E5 C5 A4:3 C5 E5 A5 C6:3",
        "A5 F5 C5 A4:3 C5 F5 A5 C6:3",
        "B5 G5 D5 B4:3 D5 G5 B5 D6:3",
        "C6 G5 E5 C5:3 E5 G5 C6 E6:3",
        "C6 A5 F5 C5:3 F5 A5 C6 F6:3",
        "D6 B5 G5 D5:3 G5 B5 D6:3 B5",
        "C6:3 G5:3 C5:3 -:3",
        "E5:2 A5 C6:3 A5 G5 E5 C5:3",
        "E5:2 G#5 B5:3 G#5 F#5 E5 B4:3",
        "F5:2 A5 C6:3 A5 G5 F5 C5:3",
        "E5:2 G5 C6:3 G5 F5 E5 C5:3",
        "D5:2 F5 A5:3 F5 E5 D5 A4:3",
        "D5:2 G5 B5:3 G5 A5 B5 D6:3",
        "E6 D6 C6 G5:3 E5 F5 G5 C5:3",
        "D5 E5 F5 G5 A5 B5 D6:3 B5:3",
    ],
}

TITLE = {
    "fps": 6, "loop": True,
    "chart": ["A", "E", "F#m", "C#m", "D", "A", "Bm", "E", "A", "E", "F#m", "C#m", "D", "Bb", "E", "A"],
    "lead": [
        "E5:3 A5:2 B5 C#6:3 A5:2 E5",
        "E5:3 G#5:2 A5 B5:3 G#5:2 E5",
        "F#5:3 A5:2 B5 C#6:3 A5:2 F#5",
        "E5:3 G#5:2 A5 G#5:3 E5 C#5 E5",
        "F#5:3 A5:2 B5 D6:3 A5:2 F#5",
        "E5:3 A5:2 C#6 E6:3 C#6 A5 E5",
        "D5:3 F#5:2 A5 B5:3 F#5:2 D5",
        "E5:3 G#5:3 B5:3 E6:3",
        "E5:3 A5:2 B5 C#6:3 A5:2 E5",
        "E5:3 G#5:2 A5 B5:3 G#5:2 E5",
        "F#5:3 A5:2 B5 C#6:3 A5:2 F#5",
        "E5:3 G#5:2 A5 G#5:3 E5 C#5 E5",
        "F#5:3 A5:2 B5 D6:3 A5:2 F#5",
        "D5:3 F5:2 G5 Bb5:3 F5:2 D5",
        "E5:3 G#5:2 A5 B5:6",
        "A5:6 E5:3 A4:3",
    ],
}

JINGLES = {
    "ready": {"fps": 3, "chart": ["Am"], "lead": ["A4 C5 E5 A5:3 E5 A5 C6 E6:3"]},
    "clear": {"fps": 3, "chart": ["C", "F", "G", "C"], "lead": [
        "G4 C5 E5 G5:3 E5 G5 C6 E6:3",
        "F5 A5 C6 F6:3 C6 A5 F5 C6:3",
        "G5 B5 D6 G6:3 D6 B5 G5 D6:3",
        "C6:3 G5:3 C6:3 -:3",
    ]},
    "lode": {"fps": 3, "chart": ["C", "G", "Am", "F", "C", "G", "C"], "lead": [
        "G4 C5 E5 G5:3 C6 E6 G6:3 E6",
        "G5 B5 D6 G6:3 D6 G6 B6:3 G6",
        "A5 C6 E6 A6:3 E6 C6 A5:3 C6",
        "A5 C6 F6 A6:3 F6 C6 A5:3 C6",
        "G5 C6 E6 G6:3 E6 C6 G5:3 C6",
        "D6 G6 B6 D6:3 B5 D6 G6:3 B6",
        "C7:6 G6:3 -:3",
    ]},
    "die": {"fps": 3, "chart": ["Bb", "Am"], "lead": [
        "F6 D6 Bb5 F5:3 D6 Bb5 F5 D5:3",
        "C6 A5 E5 C5:3 A4:3 -:3",
    ]},
    "gameover": {"fps": 6, "chart": ["Am", "Bb", "E7", "Am"], "lead": [
        "E5:3 C5:3 A4:6",
        "F5:3 D5:3 Bb4:6",
        "E5:3 D5:3 B4 G#4 E4:4",
        "A4:6 -:6",
    ]},
}

# ---- derived accompaniment ------------------------------------------------

def chord_tone_check(name: str, song: dict) -> list[str]:
    """Strong beats of a melody must be chord tones: a checked rule of the style."""
    problems = []
    for bar, (chart, line) in enumerate(zip(song["chart"], song["lead"])):
        root, quality = CHORDS[chart]
        allowed = {(root + t) % 12 for t in TONES[quality]}
        steps = melody(line)
        for strong in (0, 3, 6, 9):
            note = steps[strong]
            if note >= 12 and note % 12 not in allowed:
                problems.append(f"{name} bar {bar + 1} step {strong}: {note} not in {chart}")
    return problems


class Encoder:
    def __init__(self) -> None:
        self.patterns: list[tuple[int, ...]] = []

    def add(self, steps: list[int]) -> int:
        key = tuple(steps)
        if key not in self.patterns:
            self.patterns.append(key)
        return self.patterns.index(key)


def bass_pattern() -> list[int]:
    # Root and fifth on the two beats of each 6/8 bar; written on A, transposed.
    a, e = midi("A2"), midi("E3")
    return [a, HOLD, HOLD, e, HOLD, HOLD, a, HOLD, HOLD, e, HOLD, a]


def pluck_pattern(tone: int) -> list[int]:
    # After-beats: the guitar answer of the dance. Written on A, transposed.
    n = midi("A3") + tone
    return [HOLD, n, n, HOLD, n, n, HOLD, n, n, HOLD, n, n]


def arp_pattern(tones: tuple[int, ...]) -> list[int]:
    a = midi("A4")
    t = list(tones)[:3]
    seq = [t[0], t[1], t[2], t[0] + 12, t[2], t[1]] * 2
    return [a + v for v in seq]


def drum_pattern() -> list[int]:
    # Note 36: tammorra skin (instrument below the split); 72: its jingles.
    low, hi = 36, 72
    return [low, hi, hi, low, hi, hi, low, hi, hi, low, hi, low]


def transpose_for(root: int) -> int:
    t = (root - 9) % 12
    return t - 12 if t > 6 else t


def encode_song(enc: Encoder, song: dict, voices: list[str]) -> dict:
    orders: dict[str, list[tuple[int, int]]] = {v: [] for v in voices}
    leads = [melody(line) for line in song["lead"]]
    for bar, chart in enumerate(song["chart"]):
        root, quality = CHORDS[chart]
        t = transpose_for(root)
        tones = TONES[quality]
        for v in voices:
            if v == "bass":
                orders[v].append((enc.add(bass_pattern()), t))
            elif v == "pluck_l":
                orders[v].append((enc.add(pluck_pattern(tones[1])), t))
            elif v == "pluck_r":
                orders[v].append((enc.add(pluck_pattern(tones[2])), t))
            elif v == "lead":
                orders[v].append((enc.add(leads[bar]), 0))
            elif v == "arp":
                orders[v].append((enc.add(arp_pattern(tones)), t))
            elif v == "perc":
                orders[v].append((enc.add(drum_pattern()), 0))
            elif v == "echo":
                prev = leads[bar - 1] if bar else [HOLD] * STEPS
                delayed = prev[-3:] + leads[bar][:-3]
                orders[v].append((enc.add([HOLD if n == OFF else n for n in delayed]), 0))
            elif v == "pad":
                orders[v].append((enc.add([midi("A3") + tones[0]] + [HOLD] * (STEPS - 1)), t))
    return orders


# Voice table: instrument, alternate instrument above `split`, how many
# consecutive instruments its notes cycle through, volume, pan, auto-pan swing.
VOICES = {
    "bass": ("bass", 0, 0, 1, 205, 2, 0),
    "pluck_l": ("pluck_l", 0, 0, 1, 158, -58, 0),
    "pluck_r": ("pluck_r", 0, 0, 1, 158, 58, 0),
    "lead": ("lead_a", 0, 0, 3, 178, 1, 14),
    "arp": ("arp", 0, 0, 1, 112, 1, 50),
    "perc": ("tammorra", "jingle", 60, 1, 150, 1, 36),
    "echo": ("echo", 0, 0, 1, 124, 54, 0),
    "pad": ("pad", 0, 0, 1, 118, -50, 0),
}
GAME_VOICES = ["bass", "pluck_l", "pluck_r", "lead", "arp", "perc"]
TITLE_VOICES = GAME_VOICES + ["echo", "pad"]
JINGLE_VOICES = ["bass", "pluck_l", "pluck_r", "lead", "perc"]

# Effects: (instrument, note, frames) steps, played on the two effect channels.
SFX = {
    "press": [("sfx_blip", 72, 1), ("sfx_blip", 79, 1)],
    "drop": [("sfx_zap", 76, 1), ("sfx_zap", 69, 1), ("sfx_zap", 62, 1), ("sfx_zap", 55, 1), ("sfx_zap", 48, 2)],
    "land": [("sfx_thud", 40, 2), ("sfx_blip", 45, 1)],
    "stack": [("sfx_thud", 44, 1), ("sfx_chime", 79, 2), ("sfx_chime", 84, 2)],
    "pizza": [("sfx_chime", 72, 2), ("sfx_chime", 76, 2), ("sfx_chime", 79, 2), ("sfx_chime", 84, 2), ("sfx_chime", 88, 4)],
    "flour": [("sfx_puff", 60, 3), ("sfx_puff", 72, 3)],
    "stun": [("sfx_zap", 84, 1), ("sfx_zap", 91, 1), ("sfx_zap", 84, 1), ("sfx_zap", 91, 1)],
    "feed": [("sfx_chime", 76, 1), ("sfx_chime", 84, 1), ("sfx_chime", 79, 1), ("sfx_chime", 88, 2)],
    "item": [("sfx_blip", 84, 1), ("sfx_blip", 88, 1), ("sfx_blip", 91, 2)],
    "take": [("sfx_chime", 79, 1), ("sfx_chime", 83, 1), ("sfx_chime", 86, 1), ("sfx_chime", 91, 3)],
    "life": [("sfx_chime", 72, 2), ("sfx_chime", 79, 2), ("sfx_chime", 84, 2), ("sfx_chime", 91, 2), ("sfx_chime", 96, 4)],
    "empty": [("sfx_thud", 50, 1), ("sfx_thud", 44, 2)],
    "enter": [("sfx_blip", 60, 1), ("sfx_blip", 64, 1)],
    "menu": [("sfx_blip", 76, 1)],
}


def bytes_c(data, indent: str = "    ") -> str:
    data = list(data)
    return "\n".join(indent + ",".join(str(v) for v in data[i:i + 24]) + "," for i in range(0, len(data), 24))


def main() -> int:
    problems = []
    for name, song in {"piazza": PIAZZA, "mercato": MERCATO, "title": TITLE, **JINGLES}.items():
        if len(song["chart"]) != len(song["lead"]):
            problems.append(f"{name}: chart and lead lengths differ")
        problems += chord_tone_check(name, song)
    if problems:
        print("\n".join(problems))
        return 1

    enc = Encoder()
    songs = [("TITLE", TITLE, TITLE_VOICES, True), ("PIAZZA", PIAZZA, GAME_VOICES, True), ("MERCATO", MERCATO, GAME_VOICES, True)]
    for name, song in JINGLES.items():
        songs.append((name.upper(), song, JINGLE_VOICES, False))

    blob: list[int] = []
    offsets: list[int] = []
    for name, song, voices, loop in songs:
        orders = encode_song(enc, song, voices)
        offsets.append(len(blob))
        bars = len(song["chart"])
        blob += [song["fps"], bars, len(voices), 1 if loop else 0]
        for v in voices:
            inst, alt, split, cycle, volume, pan, swing = VOICES[v]
            blob += [INST[inst], INST[alt] if alt else INST[inst], split, cycle, volume, pan & 255, swing]
            for pattern, transpose in orders[v]:
                blob += [pattern, transpose & 255]

    sfx_blob: list[int] = []
    sfx_offsets: list[int] = []
    for name, steps in SFX.items():
        sfx_offsets.append(len(sfx_blob))
        for inst, note, frames in steps:
            sfx_blob += [INST[inst], note, frames]
        sfx_blob.append(255)

    out = ["/* Generated by tools/build_audio.py. Do not edit: change the score. */"]
    for i, (name, *_rest) in enumerate(songs):
        out.append(f"#define SONG_{name} {i}")
    out.append(f"#define SONG_COUNT {len(songs)}\n#define MUSIC_STEPS {STEPS}\n#define MUSIC_HOLD {HOLD}\n#define MUSIC_OFF {OFF}")
    for i, name in enumerate(SFX):
        out.append(f"#define SFX_{name.upper()} {i}")
    out.append(f"#define SFX_COUNT {len(SFX)}")
    flat = [b for p in enc.patterns for b in p]
    out.append(f"static const uint8_t music_patterns[{len(flat)}] = {{\n" + bytes_c(flat) + "\n};")
    out.append(f"static const uint16_t song_offsets[{len(offsets)}] = {{" + ",".join(map(str, offsets)) + "};")
    out.append(f"static const uint8_t song_data[{len(blob)}] = {{\n" + bytes_c(blob) + "\n};")
    out.append(f"static const uint8_t sfx_offsets[{len(sfx_offsets)}] = {{" + ",".join(map(str, sfx_offsets)) + "};")
    out.append(f"static const uint8_t sfx_data[{len(sfx_blob)}] = {{\n" + bytes_c(sfx_blob) + "\n};")
    (ROOT / "src/gen").mkdir(parents=True, exist_ok=True)
    (ROOT / "src/gen/music.inc").write_text("\n".join(out) + "\n", encoding="utf-8")

    instruments = []
    for name, wave, width, cutoff, res, a, d, s, r in INSTRUMENTS:
        instruments.append({"name": name, "sample_id": synth(wave, width, cutoff, res), "default_volume": 255,
                            "default_pan": 0, "attack": a, "decay": d, "sustain": s, "release": r})
    audio = {"samples": [], "instruments": instruments, "tracks": []}
    (ROOT / "audio").mkdir(exist_ok=True)
    (ROOT / "audio/audio.json").write_text(json.dumps(audio, indent=2) + "\n", encoding="utf-8")
    print(f"audio: {len(INSTRUMENTS)} instruments, {len(enc.patterns)} patterns ({len(flat)} B), "
          f"{len(songs)} songs ({len(blob)} B), {len(SFX)} effects ({len(sfx_blob)} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
