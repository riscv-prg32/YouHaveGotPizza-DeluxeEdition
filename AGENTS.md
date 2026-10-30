# AGENTS.md

Guidance for automated coding agents and maintainers of You Have Got Pizza: Deluxe Edition.

## Rules that are easy to break

- **Rules live in `src/pz_core.h`, with no PRG32 calls.** Anything that decides a score, a move or a
  collision goes there and gets a test in `tests/test_core.c`.
- **One source per build:** `src/pizza.c` includes the rest. Do not add compiler flags.
- **No pointers in initialised static data.** `tools/check_relocatable.py` enforces it.
- **No libc, no floats, no 64-bit division** in cartridge code.
- **`src/gen/*`, `audio/audio.json` and `assets/png/*` are generated.** Edit `tools/` and rebuild.
- **Text uses only the glyphs in `FONT_ORDER`**, at most 40 characters per line.
- **New colours go through `tools/palette.py`:** two colours on a screen may not share a cube cell.
- **Never publish** to the Cartridge Store from scripts. No credentials in the repository.
- **Do not modify the PRG32 checkout.** Report firmware findings in `docs/firmware-notes.md`.
- **Do not test on an attached board without asking:** it overwrites a cartridge slot.
- **Keep claims honest:** a number in `docs/` is measured, observed or estimated, and says which.

## Workflow

```bash
tests/run_tests.sh shots
```

```bash
scripts/build.sh && scripts/pack-store-bundle.sh
```

```bash
scripts/check_hosts.sh
```

Then `scripts/make_docs_data.sh`, and update the numbers quoted in `docs/`, when gameplay, art or audio change.
