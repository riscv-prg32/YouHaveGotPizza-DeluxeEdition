# Game design

## Premise

The earlier *You Have Got Pizza* demonstrator proposed a metaphor: building cool teaching
tools makes students hungry for knowledge, and foolish enough to try hard things with joy.
The Deluxe Edition makes that playable. The Professor has six lessons to feed a class.
Nobody is ever hurt: students are fed, or dazed for a moment by a cloud of flour.

## The core loop

1. **Walk over a layer.** Each of its five pieces sinks under the Professor's feet and rings
   one note higher, so crossing a layer plays a rising scale.
2. **It falls** one floor when all five pieces are down.
3. **It pushes** any layer it lands on, which starts a cascade worth doubled points.
4. **It feeds** any student it reaches on the way down.
5. **Students standing on it ride it.** Each rider makes it fall one floor further: the
   riskiest move is the most rewarding one.
6. **Four layers on a peel** make a pizza. Every pizza served ends the lesson.

The recipe is always dough, sauce, cheese, basil from the bottom up. Levels place the
layers in that vertical order, and a layer cannot overtake the one beneath it, so a
correct pizza is a property of the rules, not something the player has to manage.

The genre (walking across food so that it drops) comes from the arcade tradition
acknowledged in [originality.md](originality.md). Layouts, characters and rule details are
this game's own.

## Decisions that serve playability

| Decision | Reason |
|---|---|
| Ladder assist of 7 px, floor assist of 5 px | Turning onto a ladder on a 30 Hz game with a small joystick must forgive imprecision. |
| Catch box of 7 x 8 px, smaller than the 16 px sprites | Near misses feel exciting; unfair deaths feel broken. This single change raised the autoplayer's first-lesson win rate strongly (see [balance.md](balance.md)). |
| The Professor moves at 2.0 px per tick; students start at 0.92 | The player can always outrun a student. Danger comes from being cut off. |
| Students arrive one by one | The first seconds of a lesson are for reading the board. |
| Three student personalities | Freshers follow the flow field, ambushers aim ahead of the Professor, wanderers only chase when close. Students of one kind would walk in a conga line. |
| Lapses of attention, fading with the lesson number | The same student logic gives both the gentle first lesson and the hard last one. |
| 2.5 invulnerable seconds after a life is lost | Respawning into a student is never the player's fault. |
| A soft deadline: the bell | Running late speeds up the students and the music but never kills. Pressure, not punishment. |

## Lessons

| # | Place | Pizzas | Students | Idea |
|---|---|---|---|---|
| 1 | Piazza dell'Ateneo | 2 | 2 | Full floors, many ladders: learn the rules |
| 2 | Mercato della Pignasecca | 3 | 2 | Gaps: a layer may skip a floor |
| 3 | Lungomare al tramonto | 3 | 2 | A narrower top floor, a central gap |
| 4 | Vicolo dei panni stesi | 3 | 3 | Split floors make routes matter |
| 5 | Laboratorio RISC-V | 4 | 3 | Four pizzas, few long ladders |
| 6 | Il forno di mezzanotte | 4 | 3 | Gaps on two floors, and the final exam |

After lesson 6 the ending plays and a new academic year starts on lesson 7, with the same
six places, faster students and more of them, up to five.

## Mark and score

Each lesson ends with an Italian university mark: 30, minus 3 for each life lost and 1 for
every 5 seconds past the bell, never below 18. A clean, punctual lesson with at least 3
students fed earns **30 e lode**, with a fanfare and 3000 extra points.

| Event | Points |
|---|---|
| A piece pressed | 10 |
| A layer dropped | 50, doubled per step of a cascade |
| A layer landed on a peel | 100 |
| A pizza completed | 1000 |
| A student dazed | 100 |
| A student fed | 500, doubled per student in the same drop |
| End of lesson | mark x 100, flour x 100, unused time, lode bonus |

Extra lives come at 10,000 points and every 20,000 after that. Bonus items appear after
every third landing: espresso (speed), lemon (points), book (+2 flour), mozzarella (a pause
for everyone).

## Controls

| Input | Action |
|---|---|
| Joystick | Walk and climb |
| A or B | Throw flour |
| START | Pause; on the title, the high scores |

The title's attract mode starts after 15 idle seconds, and is the game's own autoplayer.
