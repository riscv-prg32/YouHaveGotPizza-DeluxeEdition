# Art direction

**One rule makes the set consistent: the action is saturated, the world is not.** Characters,
ingredients and interface use a fixed 20-colour set, the bright end of the palette. Each scene
uses 10 colours in a narrow, low-contrast range. Any character is therefore readable on any of
the six scenes, and all six feel like the same game even though their moods differ.

| Screen | Palette mood | Motif |
|---|---|---|
| Piazza dell'Ateneo | Morning, rose stone | University front with arcades |
| Mercato | Noon, ochre | Striped awnings, stalls of round fruit |
| Lungomare | Sunset, purple and orange | A volcano on the bay, sails, sun |
| Vicolo | Dusk, violet | Lit windows, washing lines, a street shrine |
| Laboratorio | Cold teal | Circuit traces, chips, two blackboards |
| Forno | Night, embers | The dome and mouth of a wood oven, logs |

Other shared decisions:

- **Two colours per cell** gives every scene the same texture, and ordered dithering
  (Bayer, [references](references.md)) gives every sky the same grain.
- **Dark outlines** on every sprite separate it from any background.
- **Floors and ladders** share one construction in every theme: a light top and rails over a
  near-black body. The playable structure reads instantly.
- **Original font:** a chunky 7 x 7 alphabet drawn for this game.

**Characters.** The Professor wears a mortarboard with a yellow tassel, a white beard, a purple
gown, a red bow tie and a baker's apron. Students have hoodies and backpacks, an open hungry
mouth, a white flour mask when dazed, a smile and a slice when fed.

Everything is text: sprites in `tools/art_sprites.py` (one character per pixel), scenes in
`tools/art_scenes.py` (integer drawing code), levels in `tools/levels.py`. See
[originality.md](originality.md).
