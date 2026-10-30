/*
 * pz_core.h - the rules of You Have Got Pizza: Deluxe Edition.
 *
 * Everything that decides what happens in the game lives here, with no PRG32
 * call and no global state, so the same code runs on the cartridge, in the host
 * tests and in the Monte Carlo balance study (docs/balance.md). One call to
 * pz_tick() advances the world by one 33 ms frame. The cartridge reads the
 * state and the events; it never decides anything.
 *
 * Coordinates: pixels of the 320 x 200 viewport. An actor's x is its centre and
 * its y is the line under its feet, so "standing on floor f" is y == floor_y(f).
 * Speeds are Q8 (256 = one pixel per tick), stepped one pixel at a time so that
 * no actor can jump over a junction.
 */
#ifndef PZ_CORE_H
#define PZ_CORE_H

#include <stdint.h>

#define PZ_CELL 8
#define PZ_COLS 40
#define PZ_FLOORS 5
#define PZ_NO_FLOOR 255
#define PZ_MAX_LADDERS 10
#define PZ_MAX_PIZZAS 4
#define PZ_KINDS 4
#define PZ_MAX_LAYERS (PZ_MAX_PIZZAS * PZ_KINDS)
#define PZ_MAX_STUDENTS 5
#define PZ_MAX_EVENTS 16
#define PZ_PIZZA_CELLS 5
#define PZ_LAYER_W (PZ_PIZZA_CELLS * PZ_CELL)
#define PZ_LAYER_H 6
#define PZ_PLATE_Y 192
#define PZ_ALL_PRESSED 0x1Fu

#define PZ_IN_LEFT 1u
#define PZ_IN_RIGHT 2u
#define PZ_IN_UP 4u
#define PZ_IN_DOWN 8u
#define PZ_IN_FLOUR 16u

/* Tuning: every number the balance study varies is in this block. */
#define PZ_PLAYER_SPEED 512       /* 2.0 px per tick: 60 px per second */
#define PZ_PLAYER_FAST 680        /* with an espresso */
#define PZ_CLIMB_SPEED 400
#define PZ_LADDER_ASSIST 7        /* pixels of forgiveness when turning */
#define PZ_FLOOR_ASSIST 5
#define PZ_START_LIVES 3
#define PZ_START_FLOUR 5
#define PZ_FLOUR_PER_LESSON 3
#define PZ_MAX_FLOUR 9
#define PZ_PUFF_TICKS 14
#define PZ_PUFF_COOLDOWN 18
#define PZ_FLOURED_TICKS 90
#define PZ_FED_TICKS 45
#define PZ_RESPAWN_TICKS 150
#define PZ_INVULNERABLE_TICKS 75
#define PZ_DYING_TICKS 50
#define PZ_READY_TICKS 60
#define PZ_CLEAR_TICKS 150
#define PZ_ITEM_TICKS 300
#define PZ_ESPRESSO_TICKS 240
#define PZ_BAKE_TICKS 45
#define PZ_CATCH_X 7               /* smaller than the sprites: near misses feel good */
#define PZ_CATCH_Y 8
#define PZ_FIRST_LIFE_AT 10000u
#define PZ_NEXT_LIFE_EVERY 20000u

enum { PZ_DOUGH, PZ_SAUCE, PZ_CHEESE, PZ_BASIL };
enum { LAYER_REST, LAYER_FALLING, LAYER_STACKED };
enum { ST_WALK, ST_FLOURED, ST_RIDING, ST_FED, ST_AWAY };
enum { KIND_FRESHER, KIND_AMBUSHER, KIND_WANDERER };
enum { PH_READY, PH_PLAY, PH_DYING, PH_CLEAR, PH_GAMEOVER };
enum { ITEM_ESPRESSO, ITEM_LEMON, ITEM_BOOK, ITEM_MOZZARELLA, ITEM_NONE };

enum {
    EV_NONE, EV_PRESS, EV_DROP, EV_LAND, EV_STACK, EV_PIZZA, EV_FLOUR, EV_STUN, EV_FEED,
    EV_ITEM_SPAWN, EV_ITEM_TAKE, EV_DIE, EV_EXTRA_LIFE, EV_CLEAR, EV_GAMEOVER, EV_HURRY,
    EV_READY, EV_NO_FLOUR, EV_RESPAWN
};

typedef struct { uint8_t type, a; int16_t x, y; uint16_t value; } pz_event_t;

typedef struct { uint8_t cell, top, bottom; } pz_ladder_t;

typedef struct {
    int16_t x, y;      /* layer's top-left corner */
    int16_t target_y;
    uint8_t vy, vy_sub;
    uint8_t kind, pizza, floor, target_floor;
    uint8_t state, pressed, chain, extra;
    uint8_t fed_in_drop, bounce;
} pz_layer_t;

typedef struct { uint8_t left, stacked, total, bake; } pz_pizza_t;

typedef struct {
    int16_t x, y;
    int16_t q;
    uint8_t floor;     /* PZ_NO_FLOOR while between floors on a ladder */
    int8_t ladder;     /* ladder index, -1 when walking */
    int8_t dx, dy;
} pz_body_t;

typedef struct {
    pz_body_t b;
    uint8_t state, kind, carrier;
    uint16_t timer;
    uint8_t spawn_cell, spawn_floor;
    uint8_t anim;
} pz_student_t;

typedef struct {
    /* Geometry of the current lesson. */
    uint8_t walk[PZ_FLOORS][PZ_COLS];
    pz_ladder_t ladders[PZ_MAX_LADDERS];
    uint8_t ladder_count;
    pz_layer_t layers[PZ_MAX_LAYERS];
    uint8_t layer_count;
    pz_pizza_t pizzas[PZ_MAX_PIZZAS];
    uint8_t pizza_count;
    uint8_t spawn_cell, spawn_floor;

    /* The Professor. */
    pz_body_t p;
    int8_t facing;
    uint8_t lives, flour;
    uint16_t invulnerable, espresso;
    uint8_t puff, puff_cooldown, puff_dir;
    int16_t puff_x, puff_y;
    uint8_t anim, cheering;

    pz_student_t students[PZ_MAX_STUDENTS];
    uint8_t student_count;

    /* Flow fields, recomputed in turn: distance to the Professor, and to the
     * ambusher's guess of where the Professor is going. */
    uint8_t field_prof[PZ_FLOORS][PZ_COLS];
    uint8_t field_ambush[PZ_FLOORS][PZ_COLS];
    uint8_t field_goal[PZ_FLOORS][PZ_COLS];
    uint8_t bot_goal_floor, bot_goal_cell, bot_valid;

    /* Bonus item. */
    uint8_t item, item_cell, item_floor, item_counter;
    uint16_t item_timer;

    uint8_t phase, lesson, hurry;
    uint16_t phase_timer;
    uint32_t tick, lesson_ticks, par_ticks;
    uint32_t score, next_life;
    uint32_t rng;
    uint8_t lives_lost, fed_count, grade, lode, flour_used;

    pz_event_t events[PZ_MAX_EVENTS];
    uint8_t event_count;

    /* Level data, handed in by the caller. */
    const uint8_t *level_data;
    const uint16_t *level_offsets;
    uint8_t level_count;
} pz_game_t;

/* ---- small helpers ------------------------------------------------------ */

static inline int pz_floor_y(int f) { return (4 + 4 * f) * PZ_CELL; }
static inline int pz_abs(int v) { return v < 0 ? -v : v; }
static inline int pz_ladder_x(const pz_ladder_t *l) { return l->cell * PZ_CELL + PZ_CELL; }

static uint32_t pz_rand(pz_game_t *g) {
    /* xorshift32: small, and deterministic for a given seed. */
    uint32_t x = g->rng ? g->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static void pz_emit(pz_game_t *g, uint8_t type, uint8_t a, int x, int y, uint16_t value) {
    if (g->event_count < PZ_MAX_EVENTS) {
        pz_event_t *e = &g->events[g->event_count++];
        e->type = type;
        e->a = a;
        e->x = (int16_t)x;
        e->y = (int16_t)y;
        e->value = value;
    }
}

static void pz_add_score(pz_game_t *g, uint32_t points) {
    g->score += points;
    if (g->score > 999999u) g->score = 999999u;
    while (g->score >= g->next_life) {
        g->next_life += PZ_NEXT_LIFE_EVERY;
        if (g->lives < 9) g->lives++;
        pz_emit(g, EV_EXTRA_LIFE, 0, g->p.x, g->p.y, 0);
    }
}

static int pz_can_stand(const pz_game_t *g, int floor, int x) {
    int a = (x - 4) / PZ_CELL, b = (x + 3) / PZ_CELL;
    if (x < 4 || a < 0 || b >= PZ_COLS) return 0;
    return g->walk[floor][a] && g->walk[floor][b];
}

/* Is this ladder rung a place to step off? Only where a floor really exists. */
static int pz_floor_at_ladder(const pz_game_t *g, const pz_ladder_t *l, int floor) {
    return floor >= l->top && floor <= l->bottom && g->walk[floor][l->cell] && g->walk[floor][l->cell + 1];
}

static int pz_ladder_goes(const pz_game_t *g, int ladder, int floor, int dy) {
    const pz_ladder_t *l = &g->ladders[ladder];
    for (int f = floor + dy; f >= l->top && f <= l->bottom; f += dy) {
        if (pz_floor_at_ladder(g, l, f)) return 1;
    }
    return 0;
}

/* The nearest ladder within `reach` pixels that leads on in direction dy. Two
 * ladders may meet end to end on one floor, so the direction matters. */
static int pz_ladder_toward(const pz_game_t *g, int floor, int x, int reach, int dy) {
    int best = -1, best_d = reach + 1;
    for (int i = 0; i < g->ladder_count; ++i) {
        if (!pz_floor_at_ladder(g, &g->ladders[i], floor)) continue;
        if (!pz_ladder_goes(g, i, floor, dy)) continue;
        int d = pz_abs(pz_ladder_x(&g->ladders[i]) - x);
        if (d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

/* ---- flow fields -------------------------------------------------------- */

/* Breadth-first distance from one cell over the floor-and-ladder graph. Two
 * hundred cells at most, so this is cheap enough for a game frame. */
static void pz_flow_field(const pz_game_t *g, uint8_t field[PZ_FLOORS][PZ_COLS], int floor, int cell) {
    uint8_t queue[PZ_FLOORS * PZ_COLS];
    int head = 0, tail = 0;
    for (int f = 0; f < PZ_FLOORS; ++f)
        for (int c = 0; c < PZ_COLS; ++c) field[f][c] = 255;
    if (floor >= PZ_FLOORS || cell < 0 || cell >= PZ_COLS) return;
    field[floor][cell] = 0;
    queue[tail++] = (uint8_t)(floor * PZ_COLS + cell);
    while (head < tail) {
        int f = queue[head] / PZ_COLS, c = queue[head] % PZ_COLS;
        ++head;
        int next = field[f][c] + 1;
        if (next > 250) continue;
        for (int side = -1; side <= 1; side += 2) {
            int n = c + side;
            if (n >= 0 && n < PZ_COLS && g->walk[f][n] && field[f][n] == 255) {
                field[f][n] = (uint8_t)next;
                queue[tail++] = (uint8_t)(f * PZ_COLS + n);
            }
        }
        for (int i = 0; i < g->ladder_count; ++i) {
            const pz_ladder_t *l = &g->ladders[i];
            if (c != l->cell && c != l->cell + 1) continue;
            if (f < l->top || f > l->bottom) continue;
            for (int dir = -1; dir <= 1; dir += 2) {
                for (int nf = f + dir; nf >= l->top && nf <= l->bottom; nf += dir) {
                    if (pz_floor_at_ladder(g, l, nf)) {
                        if (field[nf][c] == 255) {
                            field[nf][c] = (uint8_t)next;
                            queue[tail++] = (uint8_t)(nf * PZ_COLS + c);
                        }
                        break;
                    }
                }
            }
        }
    }
}

static int pz_nearest_floor(const pz_body_t *b) {
    if (b->floor != PZ_NO_FLOOR) return b->floor;
    int best = 0, best_d = 9999;
    for (int f = 0; f < PZ_FLOORS; ++f) {
        int d = pz_abs(pz_floor_y(f) - b->y);
        if (d < best_d) {
            best = f;
            best_d = d;
        }
    }
    return best;
}

/* ---- lesson set-up ------------------------------------------------------- */

static void pz_body_place(pz_body_t *b, int cell, int floor) {
    b->x = (int16_t)(cell * PZ_CELL + PZ_CELL / 2);
    b->y = (int16_t)pz_floor_y(floor);
    b->q = 0;
    b->floor = (uint8_t)floor;
    b->ladder = -1;
    b->dx = 1;
    b->dy = 0;
}

static void pz_student_reset(pz_game_t *g, pz_student_t *s) {
    pz_body_place(&s->b, s->spawn_cell, s->spawn_floor);
    s->b.dx = (s->spawn_cell < PZ_COLS / 2) ? 1 : -1;
    s->state = ST_WALK;
    s->timer = 0;
    s->anim = 0;
    (void)g;
}

static void pz_actors_reset(pz_game_t *g) {
    pz_body_place(&g->p, g->spawn_cell, g->spawn_floor);
    g->facing = 1;
    g->puff = 0;
    g->puff_cooldown = 0;
    g->espresso = 0;
    g->invulnerable = PZ_INVULNERABLE_TICKS;
    for (int i = 0; i < g->student_count; ++i) {
        pz_student_t *s = &g->students[i];
        pz_student_reset(g, s);
        /* They enter one after the other, so that a lesson starts gently. */
        s->state = ST_AWAY;
        s->timer = (uint16_t)(60 + i * 120);
    }
}

/* Student speed rises with the lesson number and saturates: fair even at 99. */
static int pz_student_speed(const pz_game_t *g) {
    int v = 236 + 10 * g->lesson;
    if (v > 420) v = 420;
    if (g->hurry) v += v / 6;
    return v;
}

static int pz_active_students(const pz_game_t *g, int available) {
    int n = 2 + g->lesson / 3;
    return n < available ? n : available;
}

void pz_load_lesson(pz_game_t *g, int lesson) {
    const uint8_t *d = g->level_data + g->level_offsets[lesson % g->level_count];
    g->lesson = (uint8_t)lesson;
    for (int f = 0; f < PZ_FLOORS; ++f)
        for (int c = 0; c < PZ_COLS; ++c) g->walk[f][c] = 0;
    int n = *d++;
    for (int i = 0; i < n; ++i, d += 3)
        for (int c = d[1]; c <= d[2]; ++c) g->walk[d[0]][c] = 1;
    g->ladder_count = *d++;
    for (int i = 0; i < g->ladder_count; ++i, d += 3) {
        g->ladders[i].cell = d[0];
        g->ladders[i].top = d[1];
        g->ladders[i].bottom = d[2];
    }
    g->pizza_count = *d++;
    g->layer_count = 0;
    for (int i = 0; i < g->pizza_count; ++i) {
        pz_pizza_t *pz = &g->pizzas[i];
        pz->left = *d++;
        pz->total = *d++;
        pz->stacked = 0;
        pz->bake = 0;
        for (int k = 0; k < pz->total; ++k, d += 2) {
            pz_layer_t *l = &g->layers[g->layer_count++];
            l->pizza = (uint8_t)i;
            l->floor = d[0];
            l->kind = d[1];
            l->x = (int16_t)(pz->left * PZ_CELL);
            l->y = (int16_t)(pz_floor_y(l->floor) - PZ_LAYER_H);
            l->state = LAYER_REST;
            l->pressed = 0;
            l->chain = 0;
            l->extra = 0;
            l->vy = 0;
            l->vy_sub = 0;
            l->fed_in_drop = 0;
            l->bounce = 0;
            l->target_floor = 0;
            l->target_y = l->y;
        }
    }
    g->spawn_cell = d[0];
    g->spawn_floor = d[1];
    d += 2;
    /* Each new year brings more students than there are doors: they share them. */
    int available = *d++;
    int wanted = pz_active_students(g, available + g->lesson / g->level_count);
    g->student_count = (uint8_t)(wanted > PZ_MAX_STUDENTS ? PZ_MAX_STUDENTS : wanted);
    for (int i = 0; i < g->student_count; ++i) {
        pz_student_t *s = &g->students[i];
        s->spawn_cell = d[(i % available) * 2];
        s->spawn_floor = d[(i % available) * 2 + 1];
        s->kind = (uint8_t)(i % 3);
    }
    g->item = ITEM_NONE;
    g->item_counter = 0;
    g->item_timer = 0;
    g->hurry = 0;
    g->lesson_ticks = 0;
    g->par_ticks = 1500u + 450u * g->pizza_count;
    g->lives_lost = 0;
    g->fed_count = 0;
    g->flour_used = 0;
    g->cheering = 0;
    g->lode = 0;
    g->grade = 0;
    int flour = g->flour + PZ_FLOUR_PER_LESSON;
    g->flour = (uint8_t)(flour > PZ_MAX_FLOUR ? PZ_MAX_FLOUR : flour);
    pz_actors_reset(g);
    g->phase = PH_READY;
    g->phase_timer = PZ_READY_TICKS;
    pz_flow_field(g, g->field_prof, g->p.floor, g->p.x / PZ_CELL);
    pz_flow_field(g, g->field_ambush, g->p.floor, g->p.x / PZ_CELL);
    g->bot_valid = 0;
    pz_emit(g, EV_READY, 0, 0, 0, 0);
}

void pz_new_game(pz_game_t *g, const uint8_t *level_data, const uint16_t *offsets, int level_count,
                 uint32_t seed, int first_lesson) {
    uint8_t *raw = (uint8_t *)g;
    for (unsigned i = 0; i < sizeof(*g); ++i) raw[i] = 0;
    g->level_data = level_data;
    g->level_offsets = offsets;
    g->level_count = (uint8_t)level_count;
    g->rng = seed ? seed : 1u;
    g->lives = PZ_START_LIVES;
    g->flour = PZ_START_FLOUR - PZ_FLOUR_PER_LESSON;
    g->next_life = PZ_FIRST_LIFE_AT;
    pz_load_lesson(g, first_lesson);
}

/* ---- ingredient layers -------------------------------------------------- */

static int pz_column_has_floor(const pz_game_t *g, int floor, int left) {
    for (int c = left; c < left + PZ_PIZZA_CELLS; ++c)
        if (!g->walk[floor][c]) return 0;
    return 1;
}

static void pz_set_target(pz_game_t *g, pz_layer_t *l, int floors_down) {
    const pz_pizza_t *pz = &g->pizzas[l->pizza];
    int floor = l->floor;
    while (floors_down > 0) {
        int f = floor + 1;
        while (f < PZ_FLOORS && !pz_column_has_floor(g, f, pz->left)) ++f;
        if (f >= PZ_FLOORS) {
            floor = PZ_FLOORS;
            break;
        }
        floor = f;
        --floors_down;
    }
    l->target_floor = (uint8_t)floor;
    if (floor >= PZ_FLOORS)
        l->target_y = (int16_t)(PZ_PLATE_Y - PZ_LAYER_H * (pz->stacked + 1));
    else
        l->target_y = (int16_t)(pz_floor_y(floor) - PZ_LAYER_H);
}

static void pz_start_fall(pz_game_t *g, pz_layer_t *l, int chain) {
    if (l->state != LAYER_REST) return;
    l->state = LAYER_FALLING;
    l->chain = (uint8_t)chain;
    l->vy = 1;
    l->vy_sub = 0;
    l->fed_in_drop = 0;
    l->extra = 0;
    /* Students standing on the layer ride it down, and each one makes it fall
     * one floor further: the player's best trick. */
    for (int i = 0; i < g->student_count; ++i) {
        pz_student_t *s = &g->students[i];
        if (s->state != ST_WALK && s->state != ST_FLOURED) continue;
        if (s->b.floor != l->floor || s->b.ladder >= 0) continue;
        if (s->b.x < l->x - 4 || s->b.x >= l->x + PZ_LAYER_W + 4) continue;
        s->state = ST_RIDING;
        s->carrier = (uint8_t)(l - g->layers);
        l->extra++;
    }
    pz_set_target(g, l, 1 + l->extra);
    pz_add_score(g, 50u << (chain > 4 ? 4 : chain));
    pz_emit(g, EV_DROP, (uint8_t)chain, l->x + PZ_LAYER_W / 2, l->y, (uint16_t)(50u << (chain > 4 ? 4 : chain)));
}

static void pz_feed_student(pz_game_t *g, pz_student_t *s, pz_layer_t *l) {
    uint32_t points = 500u;
    if (l) {
        points <<= (l->fed_in_drop > 4 ? 4 : l->fed_in_drop);
        l->fed_in_drop++;
    }
    s->state = ST_FED;
    s->timer = PZ_FED_TICKS;
    g->fed_count++;
    pz_add_score(g, points);
    pz_emit(g, EV_FEED, s->kind, s->b.x, s->b.y - 16, (uint16_t)points);
}

static void pz_spawn_item(pz_game_t *g) {
    /* A floor cell away from the Professor, with nothing else on it. */
    for (int attempt = 0; attempt < 24; ++attempt) {
        int floor = (int)(pz_rand(g) % PZ_FLOORS);
        int cell = 3 + (int)(pz_rand(g) % (PZ_COLS - 6));
        if (!g->walk[floor][cell] || !g->walk[floor][cell + 1]) continue;
        if (pz_abs(cell * PZ_CELL - g->p.x) < 64 && floor == g->p.floor) continue;
        if (pz_ladder_toward(g, floor, cell * PZ_CELL + 8, 16, -1) >= 0 || pz_ladder_toward(g, floor, cell * PZ_CELL + 8, 16, 1) >= 0) continue;
        int clear = 1;
        for (int i = 0; i < g->layer_count; ++i) {
            const pz_layer_t *l = &g->layers[i];
            if (l->state == LAYER_REST && l->floor == floor &&
                cell * PZ_CELL + 16 > l->x && cell * PZ_CELL < l->x + PZ_LAYER_W) clear = 0;
        }
        if (!clear) continue;
        g->item = (uint8_t)(pz_rand(g) % 4u);
        g->item_cell = (uint8_t)cell;
        g->item_floor = (uint8_t)floor;
        g->item_timer = PZ_ITEM_TICKS;
        pz_emit(g, EV_ITEM_SPAWN, g->item, cell * PZ_CELL + 8, pz_floor_y(floor) - 8, 0);
        return;
    }
}

static void pz_land(pz_game_t *g, pz_layer_t *l) {
    pz_pizza_t *pz = &g->pizzas[l->pizza];
    l->y = l->target_y;
    l->pressed = 0;
    l->bounce = 3;
    if (l->target_floor >= PZ_FLOORS) {
        l->state = LAYER_STACKED;
        l->floor = PZ_FLOORS;
        pz->stacked++;
        pz_add_score(g, 100);
        pz_emit(g, EV_STACK, l->kind, l->x + PZ_LAYER_W / 2, l->y, 100);
        if (pz->stacked == pz->total) {
            pz->bake = PZ_BAKE_TICKS;
            pz_add_score(g, 1000);
            pz_emit(g, EV_PIZZA, l->pizza, l->x + PZ_LAYER_W / 2, l->y - 12, 1000);
        }
    } else {
        l->state = LAYER_REST;
        l->floor = l->target_floor;
        pz_emit(g, EV_LAND, l->kind, l->x + PZ_LAYER_W / 2, l->y, 0);
    }
    for (int i = 0; i < g->student_count; ++i) {
        pz_student_t *s = &g->students[i];
        if (s->state == ST_RIDING && s->carrier == (uint8_t)(l - g->layers)) {
            int floor = l->state == LAYER_STACKED ? PZ_FLOORS - 1 : l->floor;
            s->b.y = (int16_t)pz_floor_y(floor);
            s->b.floor = (uint8_t)floor;
            pz_feed_student(g, s, l);
        }
    }
    if (++g->item_counter >= 3 && g->item == ITEM_NONE) {
        g->item_counter = 0;
        pz_spawn_item(g);
    }
}

static void pz_layers_tick(pz_game_t *g) {
    for (int i = 0; i < g->layer_count; ++i) {
        pz_layer_t *l = &g->layers[i];
        if (l->bounce) l->bounce--;
        if (l->state != LAYER_FALLING) continue;
        if (++l->vy_sub >= 2) {
            l->vy_sub = 0;
            if (l->vy < 6) l->vy++;
        }
        /* The stack may have grown since the fall began. */
        if (l->target_floor >= PZ_FLOORS)
            l->target_y = (int16_t)(PZ_PLATE_Y - PZ_LAYER_H * (g->pizzas[l->pizza].stacked + 1));
        int y = l->y + l->vy;
        if (y > l->target_y) y = l->target_y;
        l->y = (int16_t)y;
        /* A layer it reaches is pushed off its floor: the cascade. */
        for (int k = 0; k < g->layer_count; ++k) {
            pz_layer_t *o = &g->layers[k];
            if (k == i || o->pizza != l->pizza || o->state != LAYER_REST) continue;
            if (l->y + PZ_LAYER_H >= o->y && l->y < o->y) pz_start_fall(g, o, l->chain + 1);
        }
        /* Students it reaches are fed: nobody is ever hurt in this game. */
        for (int k = 0; k < g->student_count; ++k) {
            pz_student_t *s = &g->students[k];
            if (s->state != ST_WALK && s->state != ST_FLOURED) continue;
            if (s->b.x + 5 < l->x || s->b.x - 5 >= l->x + PZ_LAYER_W) continue;
            if (l->y + PZ_LAYER_H > s->b.y - 14 && l->y < s->b.y) pz_feed_student(g, s, l);
        }
        for (int k = 0; k < g->student_count; ++k) {
            pz_student_t *s = &g->students[k];
            if (s->state == ST_RIDING && s->carrier == (uint8_t)i) {
                s->b.y = (int16_t)(l->y + PZ_LAYER_H);
                s->b.floor = PZ_NO_FLOOR;
            }
        }
        if (l->y >= l->target_y) pz_land(g, l);
    }
}

/* ---- the Professor ------------------------------------------------------ */

static void pz_press_layers(pz_game_t *g) {
    if (g->p.ladder >= 0 || g->p.floor == PZ_NO_FLOOR) return;
    for (int i = 0; i < g->layer_count; ++i) {
        pz_layer_t *l = &g->layers[i];
        if (l->state != LAYER_REST || l->floor != g->p.floor) continue;
        int offset = g->p.x - l->x;
        if (offset < 0 || offset >= PZ_LAYER_W) continue;
        uint8_t bit = (uint8_t)(1u << (offset / PZ_CELL));
        if (l->pressed & bit) continue;
        l->pressed |= bit;
        int count = 0;
        for (uint8_t m = l->pressed; m; m &= (uint8_t)(m - 1)) ++count;
        pz_add_score(g, 10);
        pz_emit(g, EV_PRESS, (uint8_t)count, g->p.x, l->y, 10);
        if (l->pressed == PZ_ALL_PRESSED) pz_start_fall(g, l, 0);
    }
}

static void pz_player_step(pz_game_t *g, int dx, int dy) {
    pz_body_t *p = &g->p;
    if (p->ladder >= 0) {
        const pz_ladder_t *l = &g->ladders[p->ladder];
        int top = pz_floor_y(l->top), bottom = pz_floor_y(l->bottom);
        if (dy) {
            int y = p->y + dy;
            if (y >= top && y <= bottom) p->y = (int16_t)y;
            p->dy = (int8_t)dy;
        } else if (dx) {
            /* Pressed sideways near a floor: glide onto it. */
            for (int f = l->top; f <= l->bottom; ++f) {
                if (!pz_floor_at_ladder(g, l, f)) continue;
                int d = pz_floor_y(f) - p->y;
                if (d != 0 && pz_abs(d) <= PZ_FLOOR_ASSIST) {
                    p->y = (int16_t)(p->y + (d > 0 ? 1 : -1));
                    break;
                }
            }
        }
        p->floor = PZ_NO_FLOOR;
        for (int f = l->top; f <= l->bottom; ++f)
            if (pz_floor_y(f) == p->y && pz_floor_at_ladder(g, l, f)) p->floor = (uint8_t)f;
        if (p->floor != PZ_NO_FLOOR && dx && !dy) {
            p->ladder = -1;  /* step off; the next step walks */
            p->dx = (int8_t)dx;
            g->facing = (int8_t)dx;
        } else if (p->floor != PZ_NO_FLOOR && !dy) {
            /* Standing at a floor level on the ladder counts as walking. */
            p->ladder = -1;
        }
        return;
    }
    if (dy) {
        int i = pz_ladder_toward(g, p->floor, p->x, PZ_LADDER_ASSIST, dy);
        if (i >= 0) {
            int lx = pz_ladder_x(&g->ladders[i]);
            if (p->x != lx) {
                p->x = (int16_t)(p->x + (lx > p->x ? 1 : -1));
                return;
            }
            p->ladder = (int8_t)i;
            p->y = (int16_t)(p->y + dy);
            p->floor = PZ_NO_FLOOR;
            p->dy = (int8_t)dy;
            return;
        }
    }
    if (dx) {
        g->facing = (int8_t)dx;
        p->dx = (int8_t)dx;
        if (pz_can_stand(g, p->floor, p->x + dx)) p->x = (int16_t)(p->x + dx);
    }
}

static void pz_player_tick(pz_game_t *g, uint32_t input) {
    pz_body_t *p = &g->p;
    int dx = (input & PZ_IN_LEFT) ? -1 : (input & PZ_IN_RIGHT) ? 1 : 0;
    int dy = (input & PZ_IN_UP) ? -1 : (input & PZ_IN_DOWN) ? 1 : 0;
    int speed = p->ladder >= 0 ? PZ_CLIMB_SPEED : (g->espresso ? PZ_PLAYER_FAST : PZ_PLAYER_SPEED);
    if (dx || dy) g->anim++;
    p->q = (int16_t)(p->q + speed);
    while (p->q >= 256) {
        p->q = (int16_t)(p->q - 256);
        pz_player_step(g, dx, dy);
    }
    pz_press_layers(g);
    if (g->invulnerable) g->invulnerable--;
    if (g->espresso) g->espresso--;

    /* Flour: a short-lived cloud that leaves students dazed. */
    if (g->puff) g->puff--;
    if (g->puff_cooldown) g->puff_cooldown--;
    if ((input & PZ_IN_FLOUR) && !g->puff_cooldown) {
        if (g->flour) {
            g->flour--;
            g->flour_used++;
            g->puff = PZ_PUFF_TICKS;
            g->puff_cooldown = PZ_PUFF_COOLDOWN;
            g->puff_dir = (uint8_t)(g->facing < 0);
            g->puff_x = (int16_t)(p->x + g->facing * 18);
            g->puff_y = (int16_t)(p->y);
            pz_emit(g, EV_FLOUR, 0, g->puff_x, g->puff_y, 0);
        } else {
            g->puff_cooldown = PZ_PUFF_COOLDOWN;
            pz_emit(g, EV_NO_FLOUR, 0, p->x, p->y, 0);
        }
    }
    if (g->puff) {
        for (int i = 0; i < g->student_count; ++i) {
            pz_student_t *s = &g->students[i];
            if (s->state != ST_WALK) continue;
            if (pz_abs(s->b.x - g->puff_x) <= 14 && pz_abs(s->b.y - g->puff_y) <= 12) {
                s->state = ST_FLOURED;
                s->timer = PZ_FLOURED_TICKS;
                pz_add_score(g, 100);
                pz_emit(g, EV_STUN, s->kind, s->b.x, s->b.y - 16, 100);
            }
        }
    }
    /* Bonus item. */
    if (g->item != ITEM_NONE) {
        int ix = g->item_cell * PZ_CELL + 8, iy = pz_floor_y(g->item_floor);
        if (p->ladder < 0 && p->floor == g->item_floor && pz_abs(p->x - ix) <= 10 && p->y == iy) {
            uint16_t value = 0;
            switch (g->item) {
            case ITEM_ESPRESSO:
                g->espresso = PZ_ESPRESSO_TICKS;
                value = 200;
                break;
            case ITEM_LEMON:
                value = 1000;
                break;
            case ITEM_BOOK:
                g->flour = (uint8_t)(g->flour + 2 > PZ_MAX_FLOUR ? PZ_MAX_FLOUR : g->flour + 2);
                value = 300;
                break;
            default:
                value = 500;
                for (int i = 0; i < g->student_count; ++i) {
                    pz_student_t *s = &g->students[i];
                    if (s->state == ST_WALK) {
                        s->state = ST_FLOURED;
                        s->timer = PZ_FLOURED_TICKS;
                    }
                }
                break;
            }
            pz_add_score(g, value);
            pz_emit(g, EV_ITEM_TAKE, g->item, ix, iy - 16, value);
            g->item = ITEM_NONE;
        } else if (--g->item_timer == 0) {
            g->item = ITEM_NONE;
        }
    }
}

/* ---- the students ------------------------------------------------------- */

static int pz_field_at(const uint8_t field[PZ_FLOORS][PZ_COLS], int floor, int x) {
    int cell = x / PZ_CELL;
    if (floor >= PZ_FLOORS || cell < 0 || cell >= PZ_COLS) return 255;
    return field[floor][cell];
}

/* At a junction: list what is possible, then follow the field, or a whim. */
static void pz_student_decide(pz_game_t *g, pz_student_t *s) {
    pz_body_t *b = &s->b;
    const uint8_t (*field)[PZ_COLS] = s->kind == KIND_AMBUSHER ? g->field_ambush : g->field_prof;
    int floor = b->floor;
    if (floor == PZ_NO_FLOOR) return;
    int8_t odx[4], ody[4];
    int cost[4], options = 0;
    int8_t oladder[4];
    /* Sideways. */
    for (int side = -1; side <= 1; side += 2) {
        if (!pz_can_stand(g, floor, b->x + side * PZ_CELL / 2)) continue;
        odx[options] = (int8_t)side;
        ody[options] = 0;
        oladder[options] = -1;
        cost[options] = pz_field_at(field, floor, b->x + side * PZ_CELL);
        ++options;
    }
    /* Up and down, only from a ladder's exact centre. */
    for (int dy = -1; dy <= 1; dy += 2) {
        int ladder = pz_ladder_toward(g, floor, b->x, 0, dy);
        if (ladder < 0) continue;
        const pz_ladder_t *l = &g->ladders[ladder];
        int nf = floor + dy;
        while (!pz_floor_at_ladder(g, l, nf)) nf += dy;
        odx[options] = 0;
        ody[options] = (int8_t)dy;
        oladder[options] = (int8_t)ladder;
        cost[options] = pz_field_at(field, nf, b->x);
        ++options;
    }
    /* How much will this student follow the plan? Freshers mostly do, the
     * wanderer only when close: variety is what makes a crowd feel alive. */
    int whim = 0;
    uint32_t r = pz_rand(g) % 100u;
    int lapse = 45 - 5 * g->lesson;   /* first lessons: they get distracted a lot */
    if (lapse < 20) lapse = 20;
    switch (s->kind) {
    case KIND_FRESHER:
        whim = (int)r < lapse;
        break;
    case KIND_AMBUSHER:
        whim = (int)r < lapse - 8;
        break;
    default:
        whim = pz_field_at(g->field_prof, floor, b->x) > 14 || r < 30u;
        break;
    }
    int best = -1, best_cost = 1000;
    for (int i = 0; i < options; ++i) {
        int c = whim ? (int)(pz_rand(g) % 64u) : cost[i];
        if (odx[i] == -b->dx && ody[i] == -b->dy) c += whim ? 64 : 6; /* dislike U-turns */
        if (c < best_cost) {
            best_cost = c;
            best = i;
        }
    }
    b->dx = odx[best];
    b->dy = ody[best];
    if (b->dy) {
        b->ladder = oladder[best];
        b->floor = PZ_NO_FLOOR;
    }
}

static void pz_student_step(pz_game_t *g, pz_student_t *s) {
    pz_body_t *b = &s->b;
    if (b->ladder >= 0) {
        const pz_ladder_t *l = &g->ladders[b->ladder];
        b->y = (int16_t)(b->y + b->dy);
        b->dx = 0;
        for (int f = l->top; f <= l->bottom; ++f) {
            if (pz_floor_y(f) == b->y && pz_floor_at_ladder(g, l, f)) {
                b->floor = (uint8_t)f;
                b->ladder = -1;
                b->dy = 0;
                pz_student_decide(g, s);
                if (b->dx == 0 && b->dy == 0) b->dx = 1;
                return;
            }
        }
        return;
    }
    int x = b->x + b->dx;
    if (!pz_can_stand(g, b->floor, x)) {
        /* End of the floor: turn round, or take whatever is here. */
        b->dx = (int8_t)-b->dx;
        pz_student_decide(g, s);
        return;
    }
    b->x = (int16_t)x;
    if (pz_ladder_toward(g, b->floor, b->x, 0, -1) >= 0 || pz_ladder_toward(g, b->floor, b->x, 0, 1) >= 0) {
        pz_student_decide(g, s);
    } else if ((b->x & 7) == 4 && g->p.floor == b->floor && s->kind != KIND_WANDERER) {
        /* Same floor as the Professor: they notice, sooner or later. */
        int want = g->p.x < b->x ? -1 : 1;
        if (want != b->dx && (pz_rand(g) & 3u) == 0 && pz_can_stand(g, b->floor, b->x + want)) b->dx = (int8_t)want;
    }
}

static void pz_students_tick(pz_game_t *g) {
    int speed = pz_student_speed(g);
    for (int i = 0; i < g->student_count; ++i) {
        pz_student_t *s = &g->students[i];
        switch (s->state) {
        case ST_AWAY:
            if (s->timer) {
                s->timer--;
            } else {
                pz_student_reset(g, s);
                pz_emit(g, EV_RESPAWN, s->kind, s->b.x, s->b.y, 0);
            }
            break;
        case ST_FLOURED:
            if (--s->timer == 0) s->state = ST_WALK;
            s->anim++;
            break;
        case ST_FED:
            s->anim++;
            if (--s->timer == 0) {
                s->state = ST_AWAY;
                s->timer = PZ_RESPAWN_TICKS;
            }
            break;
        case ST_RIDING:
            s->anim++;
            break;
        default: {
            s->anim++;
            int v = s->b.ladder >= 0 ? speed * 3 / 4 : speed;
            s->b.q = (int16_t)(s->b.q + v);
            while (s->b.q >= 256 && s->state == ST_WALK) {
                s->b.q = (int16_t)(s->b.q - 256);
                pz_student_step(g, s);
            }
            break;
        }
        }
    }
}

static void pz_check_caught(pz_game_t *g) {
    if (g->invulnerable) return;
    for (int i = 0; i < g->student_count; ++i) {
        const pz_student_t *s = &g->students[i];
        if (s->state != ST_WALK) continue;
        if (pz_abs(s->b.x - g->p.x) < PZ_CATCH_X && pz_abs(s->b.y - g->p.y) < PZ_CATCH_Y) {
            g->phase = PH_DYING;
            g->phase_timer = PZ_DYING_TICKS;
            g->lives_lost++;
            if (g->lives) g->lives--;
            pz_emit(g, EV_DIE, 0, g->p.x, g->p.y, 0);
            return;
        }
    }
}

/* Final mark of a lesson, the Italian way: 18 to 30, and "30 e lode". */
static void pz_grade(pz_game_t *g) {
    int late = g->lesson_ticks > g->par_ticks ? (int)((g->lesson_ticks - g->par_ticks) / 150u) : 0;
    int mark = 30 - 3 * g->lives_lost - late;
    if (mark < 18) mark = 18;
    g->grade = (uint8_t)mark;
    g->lode = mark == 30 && g->fed_count >= 3;
    /* Bonus: unused time and unused flour, then the mark itself. */
    uint32_t bonus = (uint32_t)mark * 100u + g->flour * 100u;
    if (g->lesson_ticks < g->par_ticks) bonus += (g->par_ticks - g->lesson_ticks) / 3u;
    if (g->lode) bonus += 3000u;
    pz_add_score(g, bonus);
}

/* ---- one frame ----------------------------------------------------------- */

void pz_tick(pz_game_t *g, uint32_t input) {
    g->event_count = 0;
    g->tick++;
    /* One flow field per frame, in turn, so the cost stays flat. */
    if (g->phase == PH_PLAY) {
        if ((g->tick & 7u) == 0) {
            pz_flow_field(g, g->field_prof, pz_nearest_floor(&g->p), g->p.x / PZ_CELL);
        } else if ((g->tick & 7u) == 4) {
            /* Ambush: a point ahead of the Professor; on a gap, just him. */
            int cell = g->p.x / PZ_CELL + g->facing * 6;
            int floor = pz_nearest_floor(&g->p);
            if (cell < 0 || cell >= PZ_COLS || !g->walk[floor][cell]) cell = g->p.x / PZ_CELL;
            pz_flow_field(g, g->field_ambush, floor, cell);
        }
    }
    switch (g->phase) {
    case PH_READY:
        if (--g->phase_timer == 0) g->phase = PH_PLAY;
        break;
    case PH_PLAY: {
        g->lesson_ticks++;
        if (!g->hurry && g->lesson_ticks > g->par_ticks) {
            g->hurry = 1;
            pz_emit(g, EV_HURRY, 0, 0, 0, 0);
        }
        pz_player_tick(g, input);
        pz_students_tick(g);
        pz_layers_tick(g);
        pz_check_caught(g);
        for (int i = 0; i < g->pizza_count; ++i)
            if (g->pizzas[i].bake) g->pizzas[i].bake--;
        int done = 1;
        for (int i = 0; i < g->pizza_count; ++i)
            if (g->pizzas[i].stacked < g->pizzas[i].total) done = 0;
        if (done && g->phase == PH_PLAY) {
            g->phase = PH_CLEAR;
            g->phase_timer = PZ_CLEAR_TICKS;
            g->cheering = 1;
            pz_grade(g);
            pz_emit(g, EV_CLEAR, g->grade, 0, 0, 0);
        }
        break;
    }
    case PH_DYING:
        pz_layers_tick(g);
        if (--g->phase_timer == 0) {
            if (g->lives == 0) {
                g->phase = PH_GAMEOVER;
                pz_emit(g, EV_GAMEOVER, 0, 0, 0, 0);
            } else {
                pz_actors_reset(g);
                g->phase = PH_READY;
                g->phase_timer = PZ_READY_TICKS;
                pz_emit(g, EV_READY, 1, 0, 0, 0);
            }
        }
        break;
    case PH_CLEAR:
        for (int i = 0; i < g->pizza_count; ++i)
            if (g->pizzas[i].bake) g->pizzas[i].bake--;
        pz_layers_tick(g);
        if (g->phase_timer) g->phase_timer--;
        break;
    default:
        break;
    }
}

/* ---- the autoplayer -----------------------------------------------------
 * It drives the title screen's demo, and the same code plays thousands of
 * games in tests/balance.c: it is the evidence that the lessons are winnable.
 * It only ever produces button presses, exactly like a person.
 */

static void pz_bot_pick_goal(pz_game_t *g) {
    int floor = pz_nearest_floor(&g->p), best = 1000;
    pz_flow_field(g, g->field_goal, floor, g->p.x / PZ_CELL);
    g->bot_valid = 0;
    for (int i = 0; i < g->layer_count; ++i) {
        const pz_layer_t *l = &g->layers[i];
        if (l->state != LAYER_REST) continue;
        for (int s = 0; s < PZ_PIZZA_CELLS; ++s) {
            if (l->pressed & (1u << s)) continue;
            int cell = l->x / PZ_CELL + s;
            int d = g->field_goal[l->floor][cell];
            /* Prefer finishing what is started, and the lowest layer first. */
            if (l->pressed) d -= 8;
            d -= l->floor;
            if (d < best) {
                best = d;
                g->bot_goal_floor = l->floor;
                g->bot_goal_cell = (uint8_t)cell;
                g->bot_valid = 1;
            }
        }
    }
    if (g->item != ITEM_NONE && g->field_goal[g->item_floor][g->item_cell] < 16) {
        g->bot_goal_floor = g->item_floor;
        g->bot_goal_cell = g->item_cell;
        g->bot_valid = 1;
    }
    if (g->bot_valid) pz_flow_field(g, g->field_goal, g->bot_goal_floor, g->bot_goal_cell);
}

#ifndef PZ_BOT_WARY
#define PZ_BOT_WARY 30
#endif

uint32_t pz_bot_input(pz_game_t *g) {
    if (g->phase != PH_PLAY) return 0;
    if ((g->tick & 7u) == 2 || !g->bot_valid) pz_bot_pick_goal(g);
    if (!g->bot_valid) return 0;
    const pz_body_t *p = &g->p;
    uint32_t out = 0;

    /* Danger: a person watches the nearest student. Flour if it is in reach
     * ahead; otherwise take the move that leaves the most room. */
    int threat = -1, threat_d = 1000;
    for (int i = 0; i < g->student_count; ++i) {
        const pz_student_t *s = &g->students[i];
        if (s->state != ST_WALK) continue;
        int d = pz_abs(s->b.x - p->x) + 2 * pz_abs(s->b.y - p->y);
        if (d < threat_d) {
            threat = i;
            threat_d = d;
        }
    }
    if (threat >= 0 && threat_d < PZ_BOT_WARY) {
        const pz_student_t *s = &g->students[threat];
        int dx = s->b.x - p->x, dy = s->b.y - p->y;
        if (pz_abs(dy) < 12 && (dx > 0) == (g->facing > 0) && pz_abs(dx) < 32 && g->flour && !g->puff_cooldown)
            return PZ_IN_FLOUR;
        if (p->ladder < 0 && p->floor != PZ_NO_FLOOR) {
            /* Leave the floor if a ladder is at hand and the student is level. */
            if (pz_abs(dy) < 12) {
                if (pz_ladder_toward(g, p->floor, p->x, PZ_LADDER_ASSIST, -1) >= 0) return PZ_IN_UP;
                if (pz_ladder_toward(g, p->floor, p->x, PZ_LADDER_ASSIST, 1) >= 0) return PZ_IN_DOWN;
                int away = dx > 0 ? -1 : 1;
                if (pz_can_stand(g, p->floor, p->x + away * 4)) return away < 0 ? PZ_IN_LEFT : PZ_IN_RIGHT;
                /* Cornered: turn to face it, ready with the flour. */
                if (g->flour) return dx > 0 ? PZ_IN_RIGHT : PZ_IN_LEFT;
            }
        } else if (p->ladder >= 0 && pz_abs(dx) < 12) {
            return dy > 0 ? PZ_IN_UP : PZ_IN_DOWN;
        }
    }

    if (p->ladder >= 0 && p->floor == PZ_NO_FLOOR) {
        /* Between floors: head for the better of the two floors of this rung,
         * and use the same sideways glide a person uses to step off. */
        const pz_ladder_t *l = &g->ladders[p->ladder];
        int up = -1, down = -1, best_up = 255, best_down = 255;
        for (int f = l->top; f <= l->bottom; ++f) {
            if (!pz_floor_at_ladder(g, l, f)) continue;
            if (pz_floor_y(f) < p->y) up = f;
            if (pz_floor_y(f) > p->y && down < 0) down = f;
        }
        if (up >= 0) best_up = g->field_goal[up][l->cell];
        if (down >= 0) best_down = g->field_goal[down][l->cell];
        if (best_up <= best_down) {
            if (p->y - pz_floor_y(up) <= PZ_FLOOR_ASSIST) return PZ_IN_RIGHT;
            return PZ_IN_UP;
        }
        if (pz_floor_y(down) - p->y <= PZ_FLOOR_ASSIST) return PZ_IN_RIGHT;
        return PZ_IN_DOWN;
    }
    int floor = p->floor;
    int cell = p->x / PZ_CELL;
    int here = g->field_goal[floor][cell], best = here;
    if (floor == g->bot_goal_floor && here == 0) {
        /* On the goal cell: keep walking, the next goal is picked very soon. */
        return g->facing > 0 ? PZ_IN_RIGHT : PZ_IN_LEFT;
    }
    for (int dy = -1; dy <= 1; dy += 2) {
        int near = pz_ladder_toward(g, floor, p->x, PZ_CELL, dy);
        if (near < 0) continue;
        const pz_ladder_t *l = &g->ladders[near];
        int lx = pz_ladder_x(l);
        int nf = floor + dy;
        while (!pz_floor_at_ladder(g, l, nf)) nf += dy;
        int d = g->field_goal[nf][l->cell];
        if (g->field_goal[nf][l->cell + 1] < d) d = g->field_goal[nf][l->cell + 1];
        if (d < best) {
            best = d;
            if (pz_abs(p->x - lx) <= PZ_LADDER_ASSIST) out = dy < 0 ? PZ_IN_UP : PZ_IN_DOWN;
            else out = lx > p->x ? PZ_IN_RIGHT : PZ_IN_LEFT;
        }
    }
    if (cell > 0 && g->walk[floor][cell - 1] && g->field_goal[floor][cell - 1] < best) {
        best = g->field_goal[floor][cell - 1];
        out = PZ_IN_LEFT;
    }
    if (cell + 1 < PZ_COLS && g->walk[floor][cell + 1] && g->field_goal[floor][cell + 1] < best) {
        best = g->field_goal[floor][cell + 1];
        out = PZ_IN_RIGHT;
    }
    return out;
}

#endif
