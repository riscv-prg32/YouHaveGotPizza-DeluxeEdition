# Reproducing every artefact

The build is deterministic: with the same tool versions it reproduces `dist/SHA256SUMS` byte for byte
(verified by building and packing twice on one machine, 2026-10-30; not yet on a second machine).
In the terms of the ACM's artifact badging ([references](references.md)), this page aims at *results
reproduced* by a different team using the artefacts provided.

## 1. Reference environment

| Component | Version | Needed for |
|---|---|---|
| OS | macOS 26.6 (Apple Silicon); Linux should work, untested | everything |
| PRG32 | `riscv-prg32/PRG32` `development-c6` at `c14d1dd` (ABI 1.6, hash `0x260f6136`, same as `main`) | builder, headers, mixer sources |
| RISC-V toolchain | `riscv32-esp-elf-gcc` esp-14.2.0_20241119 (GCC 14.2.0) | building; another compiler changes the bytes |
| Python | 3.12, with Pillow 11.3 | art, scripts |
| C compiler | Apple clang 21 (any C99 compiler with ASan and UBSan) | host tests |
| Espressif QEMU | esp_develop_9.0.0_20240606 | QEMU runs only |
| ffmpeg | 9.0 | preview and audio renders only |
| CMake + Ninja | any recent | PRG32-QT check only |
| PRG32-QT | `riscv-prg32/PRG32-QT` at `b6d7d96` | host check only |
| CartridgeStore | `riscv-prg32/CartridgeStore` at `734653e`, with its Python requirements | strongest bundle validation only |

Only PRG32, the toolchain and Python are needed to build the cartridges.

## 2. Layout

Repositories are siblings. Each location can be overridden with `PRG32_REPO`, `PRG32_QT_REPO` or
`CARTRIDGE_STORE_REPO`.

```bash
mkdir -p riscv-prg32 && cd riscv-prg32
git clone https://github.com/riscv-prg32/YouHaveGotPizza-DeluxeEdition
git clone https://github.com/riscv-prg32/PRG32
git -C PRG32 checkout c14d1dd
git clone https://github.com/riscv-prg32/PRG32-QT
git clone https://github.com/riscv-prg32/CartridgeStore
```

## 3. Test

```bash
cd YouHaveGotPizza-DeluxeEdition
tests/run_tests.sh shots
```

Expected: `core: ... 0 failures`, `game: ... 0 failures`, and screens in `build/shots/`.

## 4. Build and pack

```bash
scripts/build.sh
```

```bash
scripts/pack-store-bundle.sh
```

```bash
cd dist && shasum -a 256 -c SHA256SUMS
```

## 5. Run

The QEMU firmware is needed once. Build it from the PRG32 checkout:

```bash
python3 -m prg32 qemu build
```

Then, from this repository, record a scripted run. The flash image is copied to a temporary
directory, so the PRG32 checkout stays untouched:

```bash
scripts/qemu_preview.py --script smoke --out build/qemu-smoke
```

To play by hand, stage the cartridge and start QEMU from the PRG32 checkout:

```bash
python3 -m prg32 qemu upload ../YouHaveGotPizza-DeluxeEdition/dist/you-have-got-pizza-deluxe-qemu.prg32 --flash build-qemu/qemu_flash.bin
```

```bash
python3 -m prg32 qemu run
```

The firmware boots into its setup menu: choose *Run cartridge*, then the cartridge. Keys: `w a s d`
for the joystick, `j` and `k` for A and B, Enter for START.

## 6. Regenerate the documentation's data

```bash
scripts/make_docs_data.sh
```

This rewrites `docs/data/*` and `docs/media/*`. The numbers in these documents come from there. The
exceptions, marked *observed* where they appear, are the QEMU clock comparison in
[graphics.md](graphics.md) and early-tuning figures in [balance.md](balance.md).

## 7. Store media

```bash
scripts/qemu_preview.py --script preview --video --out build/qemu-preview
```

```bash
python3 tools/make_store_art.py build/qemu-preview/store.png
```

```bash
cp build/qemu-preview/preview.mp4 assets/store/preview.mp4
```

These come from a real time-dependent run, and the attract lesson depends on the clock, so **media are
not bit-reproducible**. They are committed inputs. Since the screenshot goes into the cartridge,
rebuild with `scripts/build.sh` afterwards.

## 8. Known sources of variation

- A different compiler version changes the code bytes, so the checksums differ.
- `build/`-time paths don't reach the image; this was not tested on another machine, which is the
  real test of this page.
- PRG32's builder and headers are inputs: use the same commit.
