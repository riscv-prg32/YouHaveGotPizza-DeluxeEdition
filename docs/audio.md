# Audio

## 1. The instrument

PRG32 offers eight mixer voices at 22,050 Hz, with SID-like procedural instruments (compare the
MOS 6581 data sheet, [references](references.md)): triangle, saw, pulse or noise, with a 4-bit
pulse width, a resonant filter and an ADSR envelope. Sixteen are defined in
`tools/build_audio.py` and shipped in a 168-byte AUD0 block. No sample and no recording is used.

Three firmware facts shape the sound design:

- **The filter is always a low-pass**, and its highest setting sits near 1.3 kHz at this sample
  rate. Lead and plucks therefore use the top settings with some resonance. This was found by
  measurement: the first mix had under 1% of its energy above 2 kHz. It now has about 2%. The
  sound stays warm by nature.
- **Gain is squared**: `start_voice` also sets the channel volume, and the mixer applies both, so
  effective gain goes as (volume/256)^2, then x master/512 (default master 96).
- **The envelope law:** `time_ms = 1 + v*v*2000/65025`.

## 2. Why a sequencer in the cartridge

The firmware has a tracker, and this game deliberately does not use it:

| Need | Tracker | Cartridge sequencer |
|---|---|---|
| Instrument changes on one channel | No: instrument = channel | Yes: pulse-width sweep by cycling instruments |
| Per-note pan | Costs a whole extra event | Yes: ping-pong arpeggios and hats |
| Reacting to the game | Only tempo and restart | Muting a voice takes one frame |
| Real-time safety | Logs each event to the UART while holding the audio lock ([firmware-notes.md](firmware-notes.md)) | Only `note_on` and `note_off` calls |

The cost: timing resolution is one 33 ms frame. So tempos are a whole number of frames per step,
and no step is ever longer than another. On a slow host, the fixed timestep keeps the average
tempo right.

The format is the classic pattern and order list of the 8-bit trackers. A pattern is 12 bytes
(an eighth note each: two bars of 6/8), where 0 holds, 1 releases, and larger values are MIDI
notes. For each voice, a song lists one (pattern, transposition) pair per bar. Total size: 948
bytes of patterns, 1,167 of order lists, 146 of effects.

## 3. The score

The music is original, in the idiom of a southern Italian 6/8 dance. Only the melodies and chord
charts are hand-written; `tools/build_audio.py` derives the rest by fixed rules, which are short
enough to read and change:

- **Bass:** root and fifth on the two beats of each bar.
- **Plucks (left and right):** after-beats, one on the chord's third and one on its fifth.
- **Arpeggio:** broken chord in eighth notes, panned left and right in turn.
- **Tammorra:** low noise on the beats, high noise between them.
- **Echo (title only):** the lead three steps later, quieter, on the other side.
- **Pad (title only):** a slow triangle on the chord root.

| Song | Key | Frames per step | Dotted-quarter tempo | Voices | Used |
|---|---|---|---:|---:|---|
| Title | A major | 6 | 101 BPM | 8 | Title |
| Piazza | A minor | 5 | 121 BPM | 6 | Odd lessons |
| Mercato | C major | 4 | 151 BPM | 6 | Even lessons |
| Five jingles | various | 3 or 6 | | 5 | Ready, clear, lode, life lost, game over |

**A teaching detail.** The Piazza theme reaches the *Neapolitan* chord: B flat in A minor, resolving to
E7 and then A minor (Aldwell and Schachter, [references](references.md)). The major title theme borrows
it too. A game about Naples gets the chord named after the city.

A checked rule of the style: `chord_tone_check()` refuses to build if a melody's strong beat is not
a chord tone. It caught ten bars during composition.

**Listen:** [title](media/title.m4a), [piazza](media/piazza.m4a), [mercato](media/mercato.m4a). These
are stereo renders through the firmware's mixer.

## 4. Eight voices at work

| Channel | During play | On the title |
|---|---|---|
| 0 | Bass | Bass |
| 1, 2 | Plucks, hard left and right | Plucks |
| 3 | Lead | Lead |
| 4 | Arpeggio, **only when in danger** | Arpeggio |
| 5 | Tammorra | Tammorra |
| 6, 7 | Effects, panned to the event's position | Echo and pad |

**Music that follows the game.** Voice 4 joins only when a student is near the Professor or the bell
has rung, a line of game logic that adds a lot of tension. After the bell, the tempo rises by one
frame per step.

**Effects** are small (instrument, note, frames) scripts on the two free channels, panned by the
event's x position: a layer falling at the right of the screen sounds at the right. Pressing a piece
transposes the effect up a scale, so walking across a layer plays a tune.

## 5. Stereo: what is true, and how it is known

**The default firmware profile is mono** (`CONFIG_PRG32_AUDIO_MODE_MONO=y`), and so is QEMU's audio
stream. Stereo needs a firmware built for PRG32 Audio Plus with two amplifiers. The cartridge plays
correctly on both; pans simply fold down in mono, and the title shows `MONO 8V` or `STEREO 8V` from
`prg32_audio_get_mode()`.

To measure the stereo image without that hardware, `tests/render_music.c` compiles the firmware's
**real** `audio_mixer.c` and `audio_synth.c` natively, and drives them with the cartridge's real
sequencer. Results (`docs/data/audio_metrics.csv`, *measured*):

| Song | Peak (of 32,767) | RMS | L/R correlation | Side/mid energy | L/R balance |
|---|---:|---:|---:|---:|---:|
| Title | 11,910 | 2,494 | 0.856 | 0.077 | 1.00 |
| Piazza | 11,724 | 2,502 | 0.862 | 0.074 | 0.98 |
| Mercato | 12,281 | 2,634 | 0.846 | 0.083 | 0.99 |

There is ample headroom (no clipping, at 37% of full scale at most), the balance is centred, and the
image is clearly wider than the first mix's correlation of 0.98. The render excludes the I2S
hardware, and **nobody has yet listened to it on a Stereo board.**
