# Notes on the firmware

Things this project found in PRG32 at `c14d1dd`, from reading the source, from QEMU and from one ESP32-C6. The PRG32 checkout
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
when a cartridge is unloaded. **Status:** observed on an ESP32-C6: Cockroaches Cathisteria
run after this game showed at least 10 colours that differ from the same screen after a reboot, for example
(198,0,0) became (222,48,33).

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

## 8. An RGB LED write and an error after every LCD flush

On a board with a shared RGB LED pin (`CONFIG_PRG32_PIN_SHARED_RGB_LED=8` on prototype1), the end of
`prg32_gfx_present()` calls `prg32_shared_rgb_led_off()` (`prg32_display_ili9341.c:1156`). That builds an RMT
channel, sends one LED frame and waits one tick (`prg32_led.c:140`). The wait often fails, printing
`rmt_tx_wait_all_done(612): flush timeout` to the console.

**Effect here:** this game flushes small regions several times per frame, which is what makes it cheap on
SPI, so it pays this cost more often than other cartridges: about 100 errors per second in play, up to 184,
against about 4 for Cockroaches Cathisteria and 11 for devicedemo (measured). In the same run, windows with
153 to 184 errors per second ran at 26.4 to 28.8 Hz, and windows near 100 ran at about 30 Hz. That is a
correlation, not proof of cause. **Suggestion:** turn the LED off once, not at every flush.

## 9. Status bands outlive a cartridge

After switching from Cockroaches Cathisteria to this game without a reboot, that cartridge's top and bottom
band text stayed on screen, over this game's. Frame rates were also lower in that state ([testing.md](testing.md)).
**Status:** observed on an ESP32-C6. A reboot clears it. This game does not currently reset the band modes itself.

## 10. A screenshot during the scoreboard can stop the device's services

`prg32_scoreboard_show()` is modal and runs inside the cartridge's update, while the main loop holds the
graphics lock. A `/api/screenshot.bmp` request made then stalled, twice. Once, closing the scoreboard let HTTP
answer again. The other time the scoreboard stayed open, HTTP and then the USB protocol stopped answering while
drawing continued, and a reset brought them back. **Status:** observed on an ESP32-C6; that the lock is the
cause is inferred from the source.
