# Balance: a Monte Carlo study

## Question

Is every lesson winnable, and does difficulty rise smoothly? Human testing is the real answer and
has not been done yet. A simulated player can give a repeatable lower-level answer, and catch what
is *broken*.

## Method

`tests/balance.c` runs `pz_core.h` natively. The autoplayer in `pz_bot_input()` uses only button
presses, like a person, and plays 1000 games per lesson, each with a new game's three lives, from a
fixed seed sequence. It walks a breadth-first path to the nearest unpressed piece, throws flour at a
student in reach ahead, and otherwise steps away when one is within a set distance. A second
experiment plays the campaign, with lives carried over.

The study is deterministic: `scripts/make_docs_data.sh` regenerates the same table.

## Results

*Measured*, `docs/data/balance.csv` (win rate has a standard error of at most 1.6 points):

| Lesson | Pizzas | Students | Win rate | Mean time to win (s) | Lives lost when winning | Mean mark when winning |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2 | 2 | 98.8% | 65.5 | 0.59 | 28.2 |
| 2 | 3 | 2 | 93.2% | 98.3 | 1.03 | 25.5 |
| 3 | 3 | 2 | 82.6% | 90.9 | 1.28 | 25.3 |
| 4 | 3 | 3 | 68.1% | 94.6 | 1.72 | 23.9 |
| 5 | 4 | 3 | 56.4% | 105.8 | 1.97 | 23.4 |
| 6 | 4 | 3 | 30.3% | 110.3 | 2.08 | 22.8 |
| 7 (year 2) | 2 | 3 | 81.6% | 65.0 | 1.17 | 26.5 |
| 8 | 3 | 3 | 67.3% | 95.7 | 1.65 | 23.9 |
| 9 | 3 | 4 | 35.2% | 84.8 | 2.17 | 23.2 |
| 10 | 3 | 4 | 31.7% | 89.8 | 2.32 | 22.6 |
| 11 | 4 | 4 | 18.2% | 99.1 | 2.45 | 22.4 |
| 12 | 4 | 5 | 5.7% | 95.1 | 2.60 | 22.2 |

Share of campaigns reaching each lesson (`docs/data/campaign.txt`):

| Lesson | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Reached | 98.5% | 89.4% | 72.3% | 45.8% | 19.0% | 4.0% | 2.6% |

## Reading the results

- **Winnable:** every lesson of both years has been won.
- **Monotonic inside a year:** win rate falls at each step, and the second year restarts easier than
  the first year's end, then ends harder: a sawtooth that gives the player some relief.
- **Lesson length:** 65 to 110 seconds, the short rounds an arcade game wants.
- **An ending within reach:** the bot finishes the first year in 4% of campaigns. Since it plans
  nothing, uses no riders deliberately and cannot learn, this suggests a practised person will do
  better. This is a hypothesis, to be tested.

## What the study found

Its most useful results were bugs and design flaws, which no amount of reading had shown:

1. **Unsolvable play.** The autoplayer stalled alone on two lessons. Cause: where two ladders meet end
   to end, the lookup returned the first, making one direction impossible. A person would have been
   stuck too.
2. **Walking off a floor.** The ladder assist could pull the Professor into a gap beside a ladder that
   half-touched a floor. Fixed in the core, and now rejected by the level lint.
3. **A sensitivity worth knowing.** How close the bot lets a student come before reacting changes the
   outcome sharply (a campaign reaches lesson 6 in about 1% of games with a reaction distance of 22, 20%
   at 30, 2% at 38). Reacting too late gets it caught; too early and it dithers. The table uses 30,
   the best of those three.
4. **Forgiving catch boxes.** Together with the reaction distance, shrinking the catch box from 9 x 10
   to 7 x 8 pixels took the first lesson from a frustrating 64% to 99%.

## Threats to validity

- **A bot is not a person.** Win rates measure this bot. They rank the lessons; they don't predict
  fun. Reaction time, joystick feel and learning are all absent.
- **Selection of the parameter.** The bot's parameter was chosen by looking at these same outcomes,
  so absolute rates are optimistic for this bot.
- **Bot and game were tuned together**, so the smoothness of the curve is partly by construction.
- **Native integer behaviour is assumed equal to RV32.** The core uses fixed-width types and no
  floats, and the same bot ran on real firmware in the attract mode, but the two were not compared
  frame by frame.

A next step is a play-test with students, recording the same variables; see
[teaching.md](teaching.md).
