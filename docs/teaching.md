# Teaching with this cartridge

## Audience and aims

Written for a first or second university course in computer architecture or systems programming, as a
follow-up to the original demonstrator and its
[laboratory tutorial](https://github.com/riscv-prg32/YouHaveGotPizza/blob/main/docs/tutorial-create-you-have-got-pizza.md),
which starts from zero. Its premise is Papert's ([references](references.md)): people learn well when
building something they care about, and a game reacts to every change at once.

By the end, using the taxonomy of Anderson and Krathwohl, students should be able to:

| Level | Outcome |
|---|---|
| Understand | Explain why a framebuffer byte is not a colour, and what a palette entry does |
| Apply | Change a level, a sprite, a tune, and rebuild |
| Analyse | Use measurements to find where a frame's time goes |
| Evaluate | Judge a design by evidence, as the flush probe and balance study do |
| Create | Add a mechanic with tests that hold it in place |

## Laboratories

Each is about 90 minutes. Start with the build from [reproduce.md](reproduce.md).

### Lab 1: The colours of a byte

**Do.** Run `python3 tools/palette.py -v`. Pick a colour and compute its cube cell by hand from the
formula in [graphics.md](graphics.md). Change `"red"` in `FIXED`, in `tools/palette.py`, to a colour
of your own, then run `scripts/build.sh qemu` and `scripts/qemu_preview.py --script smoke --out build/lab1`.

**Checkpoint.** The sauce layer has a new colour in `build/lab1/play_start.png`.

**Reflect.** Why must two colours on one screen fall in different cells? What would a collision look
like on the ESP32-C6, and why would you not see it on QEMU?

### Lab 2: Two colours per cell

**Do.** Set `SCENE_BUDGET` in `tools/build_art.py` to 32, then 64, then 256. Run the tool each time, and
compare `assets/png/painted_forno.png` with `screen_forno.png`, noting the ROM bytes it prints.

**Checkpoint.** A small table of budget, bytes and your judgement of quality.

**Reflect.** The budget trades memory against what? Where does the merge hurt most, and why are flat
areas almost free?

### Lab 3: A level of your own

**Do.** Edit a lesson in `tools/levels.py`, then run `python3 tools/levels.py` and `tests/run_tests.sh`.

**Checkpoint.** Your level passes the lint and the reachability test, and is visible in `build/shots/`.

**Reflect.** Break it on purpose: put a ladder half on a floor. Which check catches it, and what would
a player experience if nothing did?

### Lab 4: Find where the time goes

**Do.** In `tests/test_game.c`, `measure_spi()` prints pixels per frame for both flush strategies.
Remove the object-closure rule in `close_regions()` and run the tests.

**Checkpoint.** The render invariant fails, naming the frame. Restore the rule.

**Reflect.** Why does that rule exist? Compute the time for each strategy at 32 MHz with 16 bits per
pixel, from first principles, and compare with the 0.62 us per pixel used in [graphics.md](graphics.md).

### Lab 5: Compose in 6/8

**Do.** Rewrite one bar of `PIAZZA["lead"]` in `tools/build_audio.py`. Render with
`scripts/make_docs_data.sh`, and listen to `docs/media/piazza.m4a`.

**Checkpoint.** It builds and you can hear your bar. Try a wrong note on a strong beat: the build refuses.

**Reflect.** Change the volume of a pluck from 158 to 80. Predict the effect on the correlation in
`docs/data/audio_metrics.csv` before you measure it. Why is gain squared here?

### Lab 6: Be a scientist of your game

**Do.** Change `PZ_CATCH_X`, then run the balance study: `scripts/make_docs_data.sh`.

**Checkpoint.** A plot of win rate by lesson, before and after.

**Reflect.** Read "Threats to validity" in [balance.md](balance.md). Design a five-minute play-test with
three classmates. Which of the bot's numbers can you collect? Does the bot predict them?

### Lab 7: Into the machine (extension)

**Do.** Disassemble the build with `riscv32-esp-elf-objdump -d`, and find `pz_rand`. Map its three shifts
to the C source. Then find a function called through the ABI table.

**Checkpoint.** An annotated listing of `pz_rand`.

**Reflect.** Why can a portable cartridge not contain a pointer in initialised data? What does
`tools/check_relocatable.py` do to prove the point?

## Project ideas

- **Fit in 32 KiB:** make the game run on the classroom RAM profile. What goes first?
- **Two players:** the ABI exposes player 2's input. Who steps where, and whose flour?
- **A new student:** a fourth personality with its own target rule, and tests.
- **Reach out to the assembly:** rewrite `pz_flow_field()` in RV32 assembly and compare size and speed.
- **Run the play-test** proposed above and publish the data.

## Suggested assessment

Grade the evidence, not the fun: a pull request with a change, a test that would have failed without
it, and one paragraph of measurement. Marks out of 30, of course.
