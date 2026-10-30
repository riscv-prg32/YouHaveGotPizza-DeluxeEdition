/* Rules of the game, tested on the workstation with sanitizers. */
#include <stdio.h>
#include <string.h>
#include "pz_core.h"
#include "gen/art.inc"

static int failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static pz_game_t g;

static void start(int lesson, uint32_t seed) {
    pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, seed, lesson);
    while (g.phase == PH_READY) pz_tick(&g, 0);
}

static void no_students(void) {
    for (int i = 0; i < g.student_count; ++i) { g.students[i].state = ST_AWAY; g.students[i].timer = 60000; }
    g.invulnerable = 60000;
}

/* Every lesson: reachable, and every layer's column ends on a peel. */
static void test_reachability(void) {
    for (int lesson = 0; lesson < LEVEL_COUNT; ++lesson) {
        start(lesson, 1);
        uint8_t field[PZ_FLOORS][PZ_COLS];
        pz_flow_field(&g, field, g.p.floor, g.p.x / PZ_CELL);
        for (int i = 0; i < g.layer_count; ++i) {
            const pz_layer_t *l = &g.layers[i];
            for (int s = 0; s < PZ_PIZZA_CELLS; ++s) CHECK(field[l->floor][l->x / PZ_CELL + s] != 255);
        }
        for (int f = 0; f < PZ_FLOORS; ++f)
            for (int c = 0; c < PZ_COLS; ++c)
                if (g.walk[f][c]) CHECK(field[f][c] != 255);
        CHECK(g.pizza_count >= 2 && g.pizza_count <= PZ_MAX_PIZZAS);
        CHECK(g.ladder_count <= PZ_MAX_LADDERS);
        CHECK(g.layer_count == g.pizza_count * PZ_KINDS);
        CHECK(pz_can_stand(&g, g.spawn_floor, g.p.x));
    }
}

static void test_walking_stays_on_floor(void) {
    start(2, 3);
    no_students();
    for (int t = 0; t < 400; ++t) {
        pz_tick(&g, PZ_IN_LEFT);
        CHECK(g.p.ladder >= 0 || pz_can_stand(&g, g.p.floor, g.p.x));
    }
    int left = g.p.x;
    CHECK(left >= 4);
    for (int t = 0; t < 400; ++t) pz_tick(&g, PZ_IN_RIGHT);
    CHECK(g.p.x > left);
    CHECK(g.p.x < 320);
}

static void test_ladder_assist(void) {
    start(0, 3);
    no_students();
    /* Stand a few pixels away from the ladder at cell 21 and press UP. */
    g.p.x = (int16_t)(21 * 8 + 8 + PZ_LADDER_ASSIST - 1);
    g.p.y = (int16_t)pz_floor_y(4);
    g.p.floor = 4;
    int y = g.p.y;
    for (int t = 0; t < 30; ++t) pz_tick(&g, PZ_IN_UP);
    CHECK(g.p.y < y);
    CHECK(g.p.x == 21 * 8 + 8);
    /* Climb on to the next floor, then leave sideways. */
    for (int t = 0; t < 200 && g.p.floor != 2; ++t) pz_tick(&g, PZ_IN_UP);
    CHECK(g.p.floor == 2);
    for (int t = 0; t < 20; ++t) pz_tick(&g, PZ_IN_LEFT);
    CHECK(g.p.ladder == -1);
    CHECK(g.p.x < 21 * 8 + 8);
}

/* Regression: two ladders meeting end to end on a floor (lesson 2, cell 2) must
 * both work from that floor. A lookup that ignored the direction trapped people. */
static void test_ladders_meeting_end_to_end(void) {
    start(1, 3);
    no_students();
    g.p.x = (int16_t)(2 * 8 + 8);
    g.p.y = (int16_t)pz_floor_y(2);
    g.p.floor = 2;
    g.p.ladder = -1;
    for (int t = 0; t < 12; ++t) pz_tick(&g, PZ_IN_DOWN);
    CHECK(g.p.y > pz_floor_y(2));
    g.p.y = (int16_t)pz_floor_y(2);
    g.p.floor = 2;
    g.p.ladder = -1;
    for (int t = 0; t < 12; ++t) pz_tick(&g, PZ_IN_UP);
    CHECK(g.p.y < pz_floor_y(2));
}

/* Without students, the autoplayer must finish every lesson: nothing is stuck. */
static void test_every_lesson_finishes_alone(void) {
    for (int lesson = 0; lesson < LEVEL_COUNT; ++lesson) {
        pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, 123, lesson);
        for (int t = 0; t < 9000 && g.phase != PH_CLEAR; ++t) {
            no_students();
            pz_tick(&g, pz_bot_input(&g));
        }
        CHECK(g.phase == PH_CLEAR);
        int done = 0;
        for (int i = 0; i < g.pizza_count; ++i) done += g.pizzas[i].stacked;
        CHECK(done == g.layer_count);
    }
}

static void test_press_and_fall(void) {
    start(0, 5);
    no_students();
    pz_layer_t *dough = NULL;
    for (int i = 0; i < g.layer_count; ++i)
        if (g.layers[i].kind == PZ_DOUGH && g.layers[i].floor == 4) { dough = &g.layers[i]; break; }
    CHECK(dough != NULL);
    int pressed_at_end = 0;
    g.p.x = (int16_t)(dough->x - 8);
    g.p.y = (int16_t)pz_floor_y(4);
    g.p.floor = 4;
    int score = (int)g.score;
    for (int t = 0; t < 40 && dough->state == LAYER_REST; ++t) {
        pz_tick(&g, PZ_IN_RIGHT);
        pressed_at_end = dough->pressed;
    }
    CHECK(pressed_at_end == PZ_ALL_PRESSED || dough->state != LAYER_REST);
    CHECK(dough->state == LAYER_FALLING);
    CHECK((int)g.score > score);
    for (int t = 0; t < 120 && dough->state == LAYER_FALLING; ++t) pz_tick(&g, 0);
    CHECK(dough->state == LAYER_STACKED);
    CHECK(dough->y == PZ_PLATE_Y - PZ_LAYER_H);
    CHECK(g.pizzas[dough->pizza].stacked == 1);
}

static void test_cascade_and_recipe_order(void) {
    start(0, 7);
    no_students();
    /* Dropping the top layer pushes the one below it. */
    pz_layer_t *basil = NULL, *cheese = NULL;
    for (int i = 0; i < g.layer_count; ++i) {
        if (g.layers[i].pizza != 0) continue;
        if (g.layers[i].kind == PZ_BASIL) basil = &g.layers[i];
        if (g.layers[i].kind == PZ_CHEESE) cheese = &g.layers[i];
    }
    CHECK(basil && cheese);
    pz_start_fall(&g, basil, 0);
    int pushed = 0;
    for (int t = 0; t < 200; ++t) {
        pz_tick(&g, 0);
        if (cheese->floor != 1 || cheese->state == LAYER_FALLING) pushed = 1;
    }
    CHECK(pushed);
    /* Finish the pizza through falls alone: order must be dough to basil. */
    for (int round = 0; round < 40; ++round) {
        for (int i = 0; i < g.layer_count; ++i)
            if (g.layers[i].pizza == 0 && g.layers[i].state == LAYER_REST) { pz_start_fall(&g, &g.layers[i], 0); break; }
        for (int t = 0; t < 60; ++t) pz_tick(&g, 0);
    }
    CHECK(g.pizzas[0].stacked == g.pizzas[0].total);
    int heights[PZ_KINDS];
    for (int i = 0; i < g.layer_count; ++i)
        if (g.layers[i].pizza == 0) heights[g.layers[i].kind] = g.layers[i].y;
    CHECK(heights[PZ_DOUGH] > heights[PZ_SAUCE]);
    CHECK(heights[PZ_SAUCE] > heights[PZ_CHEESE]);
    CHECK(heights[PZ_CHEESE] > heights[PZ_BASIL]);
}

static void test_riders_and_feeding(void) {
    start(0, 9);
    pz_layer_t *l = NULL;
    for (int i = 0; i < g.layer_count; ++i)
        if (g.layers[i].floor == 0) { l = &g.layers[i]; break; }
    CHECK(l != NULL);
    g.invulnerable = 60000;
    pz_student_t *s = &g.students[0];
    pz_student_reset(&g, s);
    s->b.x = (int16_t)(l->x + 12);
    s->b.y = (int16_t)pz_floor_y(0);
    s->b.floor = 0;
    s->b.ladder = -1;
    g.students[1].state = ST_AWAY;
    g.students[1].timer = 60000;
    int floor0 = l->floor;
    pz_start_fall(&g, l, 0);
    CHECK(s->state == ST_RIDING);
    CHECK(l->extra == 1);
    CHECK(l->target_floor >= floor0 + 2);
    int score = (int)g.score;
    for (int t = 0; t < 200 && s->state == ST_RIDING; ++t) pz_tick(&g, 0);
    CHECK(s->state == ST_FED);
    CHECK((int)g.score >= score + 500);
    CHECK(g.fed_count == 1);
}

static void test_flour_and_catch(void) {
    start(0, 11);
    g.invulnerable = 0;
    pz_student_t *s = &g.students[0];
    pz_student_reset(&g, s);
    g.students[1].state = ST_AWAY;
    g.students[1].timer = 60000;
    s->b.floor = g.p.floor;
    s->b.y = g.p.y;
    s->b.x = (int16_t)(g.p.x + 18);
    g.facing = 1;
    int flour = g.flour;
    pz_tick(&g, PZ_IN_FLOUR);
    CHECK(g.flour == flour - 1);
    CHECK(s->state == ST_FLOURED);
    /* Dazed students are harmless, even when standing on the Professor. */
    s->b.x = g.p.x;
    pz_tick(&g, 0);
    CHECK(g.phase == PH_PLAY);
    s->state = ST_WALK;
    int lives = g.lives;
    pz_tick(&g, 0);
    CHECK(g.phase == PH_DYING);
    CHECK(g.lives == lives - 1);
    for (int t = 0; t < PZ_DYING_TICKS + 1; ++t) pz_tick(&g, 0);
    CHECK(g.phase == PH_READY);
    CHECK(g.invulnerable > 0);
    /* Out of lives. */
    g.phase = PH_PLAY;
    g.invulnerable = 0;
    g.lives = 1;
    pz_student_reset(&g, s);
    s->b.x = g.p.x;
    s->b.y = g.p.y;
    s->b.floor = g.p.floor;
    pz_tick(&g, 0);
    CHECK(g.phase == PH_DYING);
    for (int t = 0; t < PZ_DYING_TICKS + 1; ++t) pz_tick(&g, 0);
    CHECK(g.phase == PH_GAMEOVER);
}

static void test_extra_life_and_cap(void) {
    start(0, 13);
    int lives = g.lives;
    pz_add_score(&g, PZ_FIRST_LIFE_AT);
    CHECK(g.lives == lives + 1);
    pz_add_score(&g, 999999u);
    CHECK(g.score == 999999u);
    CHECK(g.lives <= 9);
}

static void test_grade(void) {
    start(0, 15);
    g.lesson_ticks = 100;
    g.fed_count = 3;
    g.lives_lost = 0;
    pz_grade(&g);
    CHECK(g.grade == 30 && g.lode == 1);
    start(0, 15);
    g.lesson_ticks = g.par_ticks + 3000;
    g.lives_lost = 2;
    pz_grade(&g);
    CHECK(g.grade == 18 && g.lode == 0);
}

static void test_determinism(void) {
    static pz_game_t a, b;
    pz_new_game(&a, level_data, level_offsets, LEVEL_COUNT, 77, 3);
    pz_new_game(&b, level_data, level_offsets, LEVEL_COUNT, 77, 3);
    for (int t = 0; t < 3000; ++t) {
        uint32_t ia = pz_bot_input(&a), ib = pz_bot_input(&b);
        CHECK(ia == ib);
        pz_tick(&a, ia);
        pz_tick(&b, ib);
    }
    CHECK(a.score == b.score && a.rng == b.rng && a.p.x == b.p.x && a.p.y == b.p.y);
}

/* The invariants of a whole bot-played game, on every lesson and many seeds. */
static void test_invariants(void) {
    for (int lesson = 0; lesson < LEVEL_COUNT; ++lesson) {
        for (uint32_t seed = 1; seed <= 20; ++seed) {
            pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, seed * 6151u, lesson);
            for (int t = 0; t < 6000 && g.phase != PH_GAMEOVER && g.phase != PH_CLEAR; ++t) {
                pz_tick(&g, pz_bot_input(&g));
                if (g.p.ladder < 0 && g.p.floor != PZ_NO_FLOOR) CHECK(pz_can_stand(&g, g.p.floor, g.p.x));
                CHECK(g.p.x >= 0 && g.p.x < 320 && g.p.y >= 0 && g.p.y < 200);
                for (int i = 0; i < g.student_count; ++i) {
                    const pz_student_t *s = &g.students[i];
                    if (s->state == ST_WALK && s->b.ladder < 0) CHECK(pz_can_stand(&g, s->b.floor, s->b.x));
                    if (s->state == ST_WALK && s->b.ladder >= 0) CHECK(s->b.x == pz_ladder_x(&g.ladders[s->b.ladder]));
                }
                for (int i = 0; i < g.pizza_count; ++i) CHECK(g.pizzas[i].stacked <= g.pizzas[i].total);
                for (int i = 0; i < g.layer_count; ++i) {
                    CHECK(g.layers[i].y >= 0 && g.layers[i].y + PZ_LAYER_H <= PZ_PLATE_Y);
                }
            }
        }
    }
}

int main(void) {
    test_reachability();
    test_walking_stays_on_floor();
    test_ladder_assist();
    test_ladders_meeting_end_to_end();
    test_every_lesson_finishes_alone();
    test_press_and_fall();
    test_cascade_and_recipe_order();
    test_riders_and_feeding();
    test_flour_and_catch();
    test_extra_life_and_cap();
    test_grade();
    test_determinism();
    test_invariants();
    printf("core: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
