# You Have Got Pizza: Deluxe Edition, technical documentation

**Abstract.** This cartridge turns a small PRG32 teaching demonstrator into a complete
arcade game for an RV32IMC microcontroller, and documents how. It exercises the
platform's indexed colour, its tile engine and its eight-voice synthesizer inside a
64 KiB package and about 51 KiB of executable RAM. The rules are a pure C core, tested with
sanitizers and studied by Monte Carlo simulation. The renderer is held to a pixel-exact
invariant on every test frame. The soundtrack is rendered through the firmware's own
mixer to measure its stereo image. Every result can be regenerated from text sources.

## Reading guide

| Document | Question it answers |
|---|---|
| [game-design.md](game-design.md) | What is the game, and why does it play this way? |
| [architecture.md](architecture.md) | How is the code organised, and what are its invariants? |
| [graphics.md](graphics.md) | How do indexed colour, tiles and dirty regions work here? |
| [art.md](art.md) | How was the art made, and why does it look consistent? |
| [audio.md](audio.md) | How is the soundtrack composed, encoded and validated? |
| [balance.md](balance.md) | Is it winnable, and is the difficulty curve sound? |
| [testing.md](testing.md) | What was verified, on which host, and what was not? |
| [reproduce.md](reproduce.md) | How do I rebuild every artefact byte for byte? |
| [store_publishing.md](store_publishing.md) | How is the bundle validated and submitted? |
| [teaching.md](teaching.md) | How do I use this in a course? |
| [originality.md](originality.md) | Where did each asset come from? |
| [firmware-notes.md](firmware-notes.md) | What did this project learn about the firmware? |
| [references.md](references.md) | Sources cited |

## Status of the claims

To keep results honest, each one is labelled by how it was obtained:

- **Measured:** produced by a script in this repository, with data in [data/](data/).
- **Observed:** seen on a real firmware under QEMU or on PRG32-QT.
- **Estimated:** computed from a model or from a figure measured by another project.
- **Not verified:** see [testing.md](testing.md); notably, on a physical ESP32-C6 **nobody has yet
  looked at the panel or listened to the audio**, and no person other than its authors has play-tested it.

## How to cite

See [`CITATION.cff`](../CITATION.cff). The game is MIT-licensed.
