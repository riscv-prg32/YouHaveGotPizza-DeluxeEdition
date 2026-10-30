# Graphics

## 1. Indexed colour with exact palettes

On the ESP32-C6, PRG32's game surface is 64,000 bytes of 8-bit palette indices, expanded to
RGB565 only while a dirty strip is sent to the ILI9341. A colour passed as RGB565 is first
*quantised* to a cell of the system palette's 6x6x6 cube
(`prg32_gfx_index_for_rgb565_unlocked`).

That rule gives a technique. If a colour **C** quantises to cell **q(C)**, then

```c
prg32_palette_set(q(C), C);
```

makes that cell show exactly **C**, and all later drawing with C stays exact, on any drawing
path. On QEMU's direct-colour surface the value is simply shown as it is. Both displays thus
get the same picture from the same code. The one condition is that **two different colours
on screen at once must not share a cell**.

`tools/palette.py` carries an exact port of the firmware's quantiser and *solves* that
constraint, deterministically:

1. A colour whose cell is free keeps its designed value.
2. A colour landing on a nearly identical one becomes that colour (one entry saved).
3. Otherwise it moves to the nearest colour in the nearest free cell.

Each screen uses at most 30 colours: 20 fixed for characters and interface, 10 for its
scene. `python3 tools/palette.py -v` prints every colour with its cell, and which ones moved.

**Palette swap.** The three kinds of student are one 1,280-byte sheet with three 16-colour
palettes, the classic economy of indexed hardware.

## 2. Background tiles

PRG32 tiles are 8 x 8, one bit per pixel, with a foreground and a background colour: two
colours per cell, as on the home computers of the 1980s. There are 256 ids and a 40 x 25 map.

Scenes are painted freely at 320 x 200 (`tools/art_scenes.py`), then `tileize()` in
`tools/build_art.py` reduces them:

1. **Per cell**, keep the most frequent colour, then the second colour that loses least when
   all other pixels snap to the nearest of the two.
2. **Normalise** so that a bitmap and its inverse are one tile.
3. **Merge**: while over budget (128 tiles per scene, 150 for the title), merge the rarest
   tile into its perceptually nearest neighbour.

`assets/png/painted_*.png` and `screen_*.png` show each painting before and after. Bitmaps
are shared across all screens in one dictionary (597 entries), and each map is run-length
encoded, for about 1.9 KB per scene against the 64,000 bytes of a raw indexed picture.

**Where the firmware's tile engine is used.** `prg32_tile_put` and `prg32_tile_present` paint
whole screens and the HUD, with the firmware's own per-cell dirty flags.

**Character animation.** The moving row at the bottom never touches the map:
`prg32_tile_define` redefines four tile ids, and the firmware marks every cell using them as
dirty. That is how 8-bit machines animated water with almost no CPU.

**Text** uses the same tile font, in two colourways. Because the font is at the start of the
dictionary, the same bytes also work as a 1-bit transparent sprite for floating scores.

## 3. Why a moving sprite is expensive, and what to do

The firmware keeps **one** dirty bounding box, and sending it costs about 0.62 us per pixel
at 32 MHz SPI (*estimated*, a figure measured by SpaceBeltMadness and quoted by Galleria
2007). Two students in opposite corners would make that box nearly the whole screen. So:

**Dirty regions.** Each object has an id, a rectangle and a "look". Comparing with the last
frame gives rectangles to repair, grown to whole cells because a cell is what can be
repainted. They are then *closed* under two rules until stable:

- touching regions merge;
- an object that a region touches must lie wholly inside it (otherwise its pixels outside
  the region would be painted over another region's result).

Each closed region is repaired in painter's order: its tile cells, then the objects in it.

**Repainting a cell without the firmware.** `prg32_tile_present()` also flushes, which would
show the background without the sprite for a moment. So the cartridge keeps a copy of the map
and paints cells itself, as a 1-bit indexed sprite with a two-entry palette. Both paths must
draw identically; the render invariant checks it ([testing.md](testing.md)).

**Flush strategy.** Calling `prg32_gfx_present()` after each region turns one big box into
several small ones. Measured over bot-played lessons, in play (`docs/data/spi.txt`, in the
model of the firmware's dirty box):

| Strategy | Mean pixels per frame | Peak |
|---|---:|---:|
| One flush per region | 2,569 | 10,240 |
| One box per frame | 15,190 | 48,768 |

That is **5.9 times fewer** pixels, an *estimated* 1.6 ms of bus time per frame rather than
9.4 ms.

But QEMU copies the whole panel on every present, so there the trade reverses: during
development the split flush ran the game at 65% of real time, against 95% with one box
(*observed* from the HUD clock over 20 s). Rather than detect the host, **each lesson times
both strategies for 32 frames of real play and keeps the faster**. It is a small experiment
run by the program itself, and it is a useful thing to show students.

## 4. Estimated cost of a frame on the ESP32-C6

| Part | Mean | Peak | Basis |
|---|---:|---:|---|
| Cartridge instructions | 0.31 ms | 0.92 ms | *Modelled*: PRG32-QT instruction count, about 25k and 76k per frame, at 1.95 cycles per instruction and 160 MHz |
| Display transfer | 1.6 ms | 6.3 ms | *Estimated*: measured pixels x 0.62 us |
| Firmware drawing | not measured | not measured | Not counted by the model above |

The budget is 33 ms, so a comfortable 30 Hz is expected, but **this has not been measured on
a board**. The firmware's `/api/performance.json` tooling is the way to do it
([testing.md](testing.md)).
