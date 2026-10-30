# Architecture

## Layers

```text
tools/*.py   text sources -> src/gen/art.inc, src/gen/music.inc, audio/audio.json
src/pz_core.h    the rules. No PRG32 call, no global state.
src/pz_music.h   sequencer and effects. Reaches sound through two macros.
src/pizza.c      the cartridge: screen, sound channels, flow between screens.
```

The builder compiles one source file, so `pizza.c` includes the others. The separation is
one of *dependencies*: `pz_core.h` compiles with nothing but `<stdint.h>`, which is why it
can run millions of checks in `tests/test_core.c` and thousands of simulated games in
`tests/balance.c`.

## The frame

The firmware calls `pizza_deluxe_update()` and `pizza_deluxe_draw()` every 33 ms, then
presents.

**Update** applies a fixed timestep (see Fiedler, [references](references.md)): wall-clock
time accumulates, and the game advances in whole 33 ms ticks, at most three per frame. A
slow host then shows fewer frames but keeps the game's speed and the music's tempo. During
development this mattered: before it, QEMU ran the game at 65% to 95% of real time.

**Draw** turns the state into a list of objects, compares it with the previous frame's, and
repaints only what changed (see [graphics.md](graphics.md)).

The core never touches the screen or the speakers. It emits **events** (`EV_DROP`, `EV_FEED`
and so on); the cartridge turns them into sounds and floating scores. This is the observer
idea of Nystrom's *Game Programming Patterns*, reduced to a fixed array that a student can
print and read.

## Why flow fields

Student navigation and the autoplayer share one algorithm: a breadth-first search from the
target over the graph of floor cells and ladders, giving each cell its distance to the
target (CLRS, [references](references.md)). A student at a junction takes the neighbour with
the lowest number. With at most 200 cells, one search costs a few thousand instructions, and
only one field is recomputed per frame, in turn, so the cost stays flat however many
students are on screen.

## Invariants

Each is enforced by a tool or a test, so breaking it fails the build.

| Invariant | Enforced by |
|---|---|
| No pointer in initialised static data: hosts load the image anywhere, with no relocations | `tools/check_relocatable.py` links at two addresses and compares |
| No libc, no floats, no 64-bit division in cartridge code | the freestanding RV32IMC build; `memset`/`memcpy` are provided |
| Two different colours on one screen never share a palette cell | `tools/palette.py`, `test_palette_exact` |
| The panel equals a clean repaint of the state, on every test frame | `test_render_invariant_all_lessons` |
| A ladder either fully meets a floor or misses it | `tools/levels.py` |
| Every walkable cell and layer is reachable; every lesson can be finished | `test_reachability`, `test_every_lesson_finishes_alone` |
| Melody strong beats are chord tones | `tools/build_audio.py` refuses to build otherwise |
| Same seed and inputs give the same game | `test_determinism` |

## Memory

Measured for release 1.0.0 (`build/*.summary.txt`):

| Item | Bytes |
|---|---|
| Code and constant data (`code`) | 47,244 |
| Executable RAM including zeroed data (`mem`) | 52,484 of 65,536 |
| AUD0 block (16 instruments) | 168 |
| Store package | about 54,500 of 65,536 |

Of the constants, the generated art takes about 22.6 KB and the music 2.3 KB.

The cartridge needs the default 64 KiB RAM profile. It does not fit the 32 KiB classroom
profile; shrinking it is proposed as an exercise in [teaching.md](teaching.md).

## Running on many hosts

Features are declared at build time: `tilemap` and `sprites` are required; `audio` and
`audio_plus` are optional, so the same image runs silently on a host without sound. The
image never asks which host it is on. Where hosts differ, it measures (see "Flush strategy"
in [graphics.md](graphics.md)).
