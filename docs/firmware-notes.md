# Notes on the firmware

Things this project found in PRG32 at `c14d1dd`, from reading the source and from QEMU. The PRG32 checkout
was not modified. These are for the maintainers to judge; none blocks the game.

## 1. The tracker logs while holding the audio lock

`components/prg32_audio/audio_tracker.c` runs `printf("TRACKER EVENT: ...")` for each event, inside
`prg32_audio_tracker_step()`, in the audio task. The default console is a UART at 115200 baud, where a
line of about 45 characters takes about 4 ms. A chord of 6 events could then keep the audio task from
mixing for much longer than its 128-frame buffer lasts (5.8 ms). It looks like leftover debugging.

**Effect here:** none, since this game does not use the tracker (one of the reasons,
[audio.md](audio.md)). **Status:** inferred from the source, not measured on a board.

## 2. A custom palette outlives the cartridge

`palette_init()` runs only at display initialisation, and there is no callback when a cartridge exits. So
palette changes persist into the setup menu and other cartridges until a reboot.

**Effect here:** because this game only writes each colour into its own cube cell, leftovers are a
slight tint, of at most about one cube step, and usually much less. **Suggestion:** reset the palette
when a cartridge is unloaded. **Status:** read from the source, not observed on a board.

## 3. `prg32_palette_set` differs between displays

On the ILI9341 backend it recolours existing pixels and dirties the whole viewport. On QEMU it only
affects later indexed draws. So palette cycling, which the manual mentions, is not portable. This game
therefore sets palettes before painting, and animates by redefining tiles instead.

## 4. QEMU presents the whole panel for a partial box

A dirty box narrower than the screen makes `prg32_display_qemu_rgb.c` send the full 320 x 240 frame. Many
small presents, the right thing on SPI, are then the wrong thing on QEMU: 65% against 95% of real time in
our comparison. Sending the full-width rows of the box would remove the difference.

## 5. Stereo is a build-time profile

The default profile is mono, and the cartridge ABI exposes `prg32_audio_get_mode()` but not a setter. A
cartridge can pan, and the listener hears it only on an Audio Plus build. QEMU has no stereo stream.

## 6. The filter's range

At 22,050 Hz the top cutoff (`12288/32768` per sample) is near 1.3 kHz, so the synth can't produce a truly
bright voice. A higher top value, or a bypass, would make the "SID-like" sound much closer to its model.

## 7. Boot behaviour

The QEMU firmware boots into the setup menu, so scripts must choose *Run cartridge*
(`BOOT` in `scripts/qemu_preview.py`). Also, `g_fb` is now a pointer into the panel's own framebuffer, so
capture scripts from older cartridges that read it as an array get garbage, as ours did at first.
