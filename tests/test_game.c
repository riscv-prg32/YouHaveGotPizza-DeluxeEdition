/*
 * The whole cartridge on the workstation: flow, rendering and sound.
 *
 *   test_game [shots-dir]   also writes screens as PPM (tests/run_tests.sh converts)
 *
 * The heart of it is the render invariant. After every frame the panel model must
 * equal a clean repaint of the entire scene from the game state. A ghost pixel left by
 * the dirty-region renderer, or a tile the firmware path and the cartridge path draw
 * differently, fails this test at the exact frame it appears.
 */
#include "host/host_prg32.h"
#include "../src/pizza.c"

#include <stdlib.h>

static int failures, checks;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d %s (frame %lu)\n", __FILE__, __LINE__, #cond, frames); } } while (0)

static unsigned long frames;
static const char *shots;

static void frame(uint32_t buttons) {
    host_input = buttons;
    host_ms += 33;
    host_frame_pixels = 0;
    pizza_deluxe_update();
    pizza_deluxe_draw();
    prg32_gfx_present();   /* the firmware's own present at the end of a frame */
    ++frames;
}

static void wait_frames(int n) {
    for (int i = 0; i < n; ++i) frame(0);
}

static void press(uint32_t b) {
    frame(b);
    frame(0);
}

static void shot(const char *name) {
    if (!shots) return;
    char path[300];
    snprintf(path, sizeof path, "%s/%s.ppm", shots, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n320 200\n255\n");
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x) {
            uint16_t c = host_panel[y][x];
            unsigned char rgb[3] = {(unsigned char)(((c >> 11) * 527 + 23) >> 6),
                                    (unsigned char)((((c >> 5) & 63) * 259 + 33) >> 6),
                                    (unsigned char)(((c & 31) * 527 + 23) >> 6)};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

/* Repaint everything into a private copy and compare with the panel. */
static long render_mismatch(void) {
    static uint8_t keep[PRG32_GAME_H][PRG32_GAME_W];
    int dx0 = host_dx0, dy0 = host_dy0, dx1 = host_dx1, dy1 = host_dy1;
    memcpy(keep, host_fb, sizeof keep);
    region_t all = {0, 0, PRG32_GAME_W, PRG32_GAME_H};
    for (int cy = 0; cy < PRG32_TILE_ROWS; ++cy)
        for (int cx = 0; cx < PRG32_TILE_COLS; ++cx) paint_cell(cx, cy);
    draw_objects_in(&all);
    long bad = 0;
    for (int y = 0; y < PRG32_GAME_H; ++y)
        for (int x = 0; x < PRG32_GAME_W; ++x)
            if (host_palette[host_fb[y][x]] != host_panel[y][x]) ++bad;
    memcpy(host_fb, keep, sizeof keep);
    host_dx0 = dx0;
    host_dy0 = dy0;
    host_dx1 = dx1;
    host_dy1 = dy1;
    return bad;
}

/* The firmware and cartridge paths must paint a tile identically. */
static void test_palette_exact(void) {
    for (int n = 0; n < SCREEN_COUNT; ++n) {
        screen_t s = screen_info(n);
        for (int i = 0; i < s.palette_count; ++i) {
            for (int k = 0; k < i; ++k) {
                if (s.palette[i] != s.palette[k]) CHECK(cube_index(s.palette[i]) != cube_index(s.palette[k]));
            }
            CHECK(cube_index(s.palette[i]) == host_quantise(s.palette[i]));
        }
        CHECK(s.tile_count + SCENE_BASE <= 256);
    }
}

/* On the indexed panel, every pixel of every screen must show the exact RGB565 the
 * tile asks for. This is what prg32_palette_set() is for: without it, or with a
 * collision, the colour shown would be the cube's and this test would fail. */
static void test_colours_exact_on_panel(void) {
    pizza_deluxe_init();
    for (int n = 0; n < SCREEN_COUNT; ++n) {
        screen_load(n);
        clear_game_objects();
        for (int i = 0; i < OBJ_COUNT; ++i) v.cur[i].w = 0;
        prg32_tile_present();
        long wrong = 0;
        for (int y = 0; y < PRG32_GAME_H; ++y) {
            for (int x = 0; x < PRG32_GAME_W; ++x) {
                uint8_t id = v.map[y / 8][x / 8];
                const uint8_t *bits = &tile_dict[v.tile_dict_id[id] * 8u];
                uint16_t want = (bits[y % 8] & (0x80u >> (x % 8))) ? v.tile_fg[id] : v.tile_bg[id];
                if (host_panel[y][x] != want) ++wrong;
            }
        }
        CHECK(wrong == 0);
    }
    /* Sprite colours too: the Professor's palette, after a full repaint. */
    screen_load(0);
    prg32_tile_present();
    prg32_sprite_draw_indexed(100, 100, &v.prof, 0);
    prg32_gfx_present();
    int seen = 0;
    for (int y = 100; y < 116; ++y)
        for (int x = 100; x < 116; ++x)
            for (int c = 1; c < PROF_COLORS; ++c)
                if (host_panel[y][x] == prof_palette[c]) ++seen;
    CHECK(seen >= 16 * 16 / 3);
}

static void test_title_to_lesson(void) {
    pizza_deluxe_init();
    wait_frames(40);
    CHECK(v.app == APP_TITLE);
    CHECK(v.music.playing);
    CHECK(render_mismatch() == 0);
    shot("title");
    press(PRG32_BTN_START);
    CHECK(host_scoreboards == 1);
    CHECK(v.app == APP_TITLE);
    press(PRG32_BTN_A);
    CHECK(v.app == APP_PLAY);
    frame(0);
    CHECK(render_mismatch() == 0);
    shot("ready");
}

/* The bot plays; after every frame the screen must match the game state. */
static void play_with_bot_checking(int max_frames, long *bad_frames) {
    for (int i = 0; i < max_frames && v.app == APP_PLAY; ++i) {
        uint32_t in = pz_bot_input(&game);
        uint32_t b = 0;
        if (in & PZ_IN_LEFT) b |= PRG32_BTN_LEFT;
        if (in & PZ_IN_RIGHT) b |= PRG32_BTN_RIGHT;
        if (in & PZ_IN_UP) b |= PRG32_BTN_UP;
        if (in & PZ_IN_DOWN) b |= PRG32_BTN_DOWN;
        if (in & PZ_IN_FLOUR) b |= PRG32_BTN_A;
        frame(b);
        long bad = render_mismatch();
        if (bad) {
            if (!*bad_frames) printf("first mismatch: frame %lu, %ld pixels, phase %d\n", frames, bad, game.phase);
            ++*bad_frames;
        }
    }
}

static void test_render_invariant_all_lessons(void) {
    long bad_frames = 0;
    for (int lesson = 0; lesson < LEVEL_COUNT; ++lesson) {
        v.demo = 0;
        start_lesson(lesson, 1, 1000u + (uint32_t)lesson);
        play_with_bot_checking(1800, &bad_frames);
        char name[32];
        snprintf(name, sizeof name, "lesson%d", lesson + 1);
        if (v.app == APP_PLAY) shot(name);
    }
    CHECK(bad_frames == 0);
}

static void test_flow_card_gameover(void) {
    /* Win lesson 1 by force, see the card and the next lesson. */
    start_lesson(0, 1, 11);
    wait_frames(PZ_READY_TICKS + 2);
    CHECK(game.phase == PH_PLAY);
    for (int i = 0; i < game.layer_count; ++i) {
        game.layers[i].state = LAYER_STACKED;
    }
    for (int i = 0; i < game.pizza_count; ++i) game.pizzas[i].stacked = game.pizzas[i].total;
    wait_frames(5);
    CHECK(game.phase == PH_CLEAR);
    shot("clear");
    wait_frames(PZ_CLEAR_TICKS + 2);
    CHECK(v.app == APP_CARD);
    CHECK(render_mismatch() == 0);
    shot("card");
    press(PRG32_BTN_A);
    CHECK(v.app == APP_PLAY);
    CHECK(game.lesson == 1);

    /* Pause hides nothing and resumes cleanly. */
    wait_frames(PZ_READY_TICKS + 2);
    press(PRG32_BTN_START);
    CHECK(v.app == APP_PAUSE);
    shot("pause");
    uint32_t t = game.tick;
    wait_frames(30);
    CHECK(game.tick == t);
    press(PRG32_BTN_START);
    CHECK(v.app == APP_PLAY);
    wait_frames(3);
    CHECK(render_mismatch() == 0);

    /* Last lesson of the year: the ending, then the harder class. */
    start_lesson(LEVEL_COUNT - 1, 1, 13);
    wait_frames(PZ_READY_TICKS + 2);
    for (int i = 0; i < game.pizza_count; ++i) game.pizzas[i].stacked = game.pizzas[i].total;
    wait_frames(PZ_CLEAR_TICKS + 8);
    CHECK(v.app == APP_CARD);
    press(PRG32_BTN_A);
    CHECK(v.app == APP_ENDING);
    CHECK(render_mismatch() == 0);
    shot("ending");
    press(PRG32_BTN_A);
    CHECK(v.app == APP_PLAY);
    CHECK(game.lesson == LEVEL_COUNT);
    CHECK(game.student_count >= 3);
    wait_frames(5);
    CHECK(render_mismatch() == 0);

    /* Game over: the score goes to the firmware exactly once. */
    start_lesson(0, 1, 17);
    wait_frames(PZ_READY_TICKS + 2);
    game.lives = 1;
    game.invulnerable = 0;
    game.score = 4242;
    pz_student_t *s = &game.students[0];
    pz_student_reset(&game, s);
    s->b.x = game.p.x;
    s->b.y = game.p.y;
    s->b.floor = game.p.floor;
    int before = host_score_submits;
    wait_frames(PZ_DYING_TICKS + 4);
    CHECK(v.app == APP_GAMEOVER);
    CHECK(host_score_submits == before + 1);
    CHECK(host_score_last == 4242);
    shot("gameover");
    CHECK(render_mismatch() == 0);
    press(PRG32_BTN_A);
    CHECK(v.app == APP_TITLE);
}

static void test_demo_mode(void) {
    enter_title();
    wait_frames(IDLE_TO_DEMO_TICKS + 2);
    CHECK(v.app == APP_PLAY);
    CHECK(v.demo == 1);
    int submits = host_score_submits;
    wait_frames(600);
    shot("demo");
    CHECK(host_score_submits == submits);
    press(PRG32_BTN_A);
    CHECK(v.app == APP_TITLE);
}

/* A host that draws slowly must not change the speed of the game. */
static void test_fixed_timestep(void) {
    start_lesson(0, 1, 19);
    uint32_t t0 = game.tick;
    for (int i = 0; i < 100; ++i) {
        host_ms += 66;    /* half the frame rate */
        host_input = 0;
        pizza_deluxe_update();
        pizza_deluxe_draw();
        prg32_gfx_present();
    }
    CHECK(game.tick - t0 == 200);
    /* A long stall replays only a few ticks. */
    t0 = game.tick;
    host_ms += 20000;
    pizza_deluxe_update();
    CHECK(game.tick - t0 <= MAX_CATCH_UP);
    wait_frames(1);
}

/* Every note in range; effects stay on their channels during play. */
static void test_sound(void) {
    int on = 0;
    for (int i = 0; i < host_note_count; ++i) {
        const host_note_t *n = &host_notes[i];
        CHECK(n->channel < 8);
        if (n->kind) {
            ++on;
            CHECK(n->instrument < 16);
            CHECK(n->note >= 24 && n->note <= 108);
            CHECK(n->volume > 0);
        }
    }
    CHECK(on > 500);
}

/* Which display cost does the renderer really produce? Reported, and bounded. */
static void measure_spi(void) {
    v.demo = 0;
    uint64_t totals[2] = {0, 0};
    uint32_t peaks[2] = {0, 0};
    long counted[2] = {0, 0};
    for (int mode = 0; mode < 2; ++mode) {
        for (int lesson = 0; lesson < LEVEL_COUNT; ++lesson) {
            start_lesson(lesson, 1, 31337u + (uint32_t)lesson);
            for (int i = 0; i < 1500 && v.app == APP_PLAY; ++i) {
                v.probe_decided = 1;
                v.split_flush = (uint8_t)(mode == 0);
                frame(game.phase == PH_PLAY ? 0 : 0);
                uint32_t in = pz_bot_input(&game);
                host_input = 0;
                if (in & PZ_IN_LEFT) host_input |= PRG32_BTN_LEFT;
                if (in & PZ_IN_RIGHT) host_input |= PRG32_BTN_RIGHT;
                if (in & PZ_IN_UP) host_input |= PRG32_BTN_UP;
                if (in & PZ_IN_DOWN) host_input |= PRG32_BTN_DOWN;
                if (in & PZ_IN_FLOUR) host_input |= PRG32_BTN_A;
                host_ms += 33;
                host_frame_pixels = 0;
                pizza_deluxe_update();
                pizza_deluxe_draw();
                prg32_gfx_present();
                ++frames;
                if (game.phase == PH_PLAY) {
                    totals[mode] += host_frame_pixels;
                    if (host_frame_pixels > peaks[mode]) peaks[mode] = host_frame_pixels;
                    ++counted[mode];
                }
            }
        }
    }
    printf("spi: pixels per frame in play: split mean %llu peak %u; one box mean %llu peak %u\n",
           (unsigned long long)(totals[0] / (uint64_t)counted[0]), peaks[0],
           (unsigned long long)(totals[1] / (uint64_t)counted[1]), peaks[1]);
    CHECK(totals[0] < totals[1]);
}

static void test_stereo_label(void) {
    host_audio_mode = PRG32_AUDIO_MODE_STEREO;
    enter_title();
    wait_frames(2);
    shot("title_stereo");
    CHECK(v.music.mute_mask == 0);   /* a song starts with all its voices */
    host_audio_mode = PRG32_AUDIO_MODE_MONO;
    CHECK(v.app == APP_TITLE);
}

/* Optional: the note log for tools/render_audio.py. */
static void dump_notes(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "ms,channel,kind,instrument,note,volume,pan\n");
    for (int i = 0; i < host_note_count; ++i) {
        const host_note_t *n = &host_notes[i];
        fprintf(f, "%u,%u,%u,%u,%u,%u,%d\n", n->ms, n->channel, n->kind, n->instrument, n->note, n->volume, n->pan);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    shots = argc > 1 ? argv[1] : NULL;
    test_palette_exact();
    test_colours_exact_on_panel();
    test_title_to_lesson();
    test_render_invariant_all_lessons();
    test_flow_card_gameover();
    test_demo_mode();
    test_fixed_timestep();
    test_sound();
    test_stereo_label();
    measure_spi();
    CHECK(host_bad_tile_calls == 0);
    if (shots) {
        char path[300];
        snprintf(path, sizeof path, "%s/notes.csv", shots);
        dump_notes(path);
    }
    printf("game: %d checks, %d failures, %lu frames, %llu ABI calls\n", checks, failures, frames, (unsigned long long)host_abi_calls);
    return failures ? 1 : 0;
}
