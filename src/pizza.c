/*
 * You Have Got Pizza: Deluxe Edition - PRG32 portable cartridge.
 *
 * The rules are in pz_core.h and the sequencer in pz_music.h. This file shows
 * the game: it owns the screen, the sound channels and the flow between screens.
 * docs/architecture.md explains the design; the short version of the renderer is:
 *
 *   1. Backgrounds are firmware tiles (1 bit per pixel, two colours per tile).
 *      prg32_tile_put()/prg32_tile_present() paint whole screens and the HUD.
 *   2. Sprites are 4 bits per pixel, indexed, with local palettes. Three kinds of
 *      student are one sheet with three palettes.
 *   3. A moving sprite repaints only the tile cells under it, and every group of
 *      touching objects is flushed on its own: the display then receives a few
 *      small boxes instead of one box that spans the whole screen.
 *
 * Portable cartridges have no relocations: no pointer may appear in initialised
 * static data (tools/check_relocatable.py proves it), so descriptors are filled in
 * at run time.
 */
#include "prg32.h"
#include <stddef.h>

/* The cartridge is linked with -nostdlib; the compiler may still emit calls
 * to these two for structure copies and clears. */
#ifndef PZ_HOST
void *memset(void *dst, int value, size_t n) __attribute__((optimize("no-tree-loop-distribute-patterns")));
void *memcpy(void *dst, const void *src, size_t n) __attribute__((optimize("no-tree-loop-distribute-patterns")));
void *memset(void *dst, int value, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)value;
    return dst;
}
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
#endif

#define PZ_NOTE_ON(ch, inst, note, vol, pan) \
    prg32_audio_note_on_pan((uint8_t)(ch), (uint8_t)(inst), (uint8_t)(note), (uint8_t)(vol), (int8_t)(pan))
#define PZ_NOTE_OFF(ch) prg32_audio_note_off((uint8_t)(ch))

#include "gen/art.inc"
#include "gen/music.inc"
#include "pz_core.h"
#include "pz_music.h"

#define SCORE_GAME "PIZZA-DELUXE"
#define TITLE_SCREEN 6
#define IDLE_TO_DEMO_TICKS 450
#define DEMO_TICKS 1500
#define TICK_MS 33u
#define MAX_CATCH_UP 3u
#define PROBE_FRAMES 32

enum { APP_TITLE, APP_PLAY, APP_PAUSE, APP_CARD, APP_ENDING, APP_GAMEOVER };

/* Object ids are also the identity used to detect change between frames. */
enum {
    OBJ_PROF = 0, OBJ_STUDENT = 1, OBJ_PUFF = 6, OBJ_ITEM = 7, OBJ_LAYER = 8,
    OBJ_PIZZA = OBJ_LAYER + PZ_MAX_LAYERS, OBJ_POPUP = OBJ_PIZZA + PZ_MAX_PIZZAS,
    OBJ_TITLE = OBJ_POPUP + 4, OBJ_COUNT = OBJ_TITLE + 4
};

typedef struct { int16_t x, y, w, h; uint16_t look; } obj_t;
typedef struct { int16_t x0, y0, x1, y1; } region_t;
typedef struct { int16_t x, y; uint16_t value; uint8_t ttl; } popup_t;

#define MAX_REGIONS 20

static struct {
    /* Everything the renderer needs to repaint one cell without the firmware. */
    uint8_t map[PRG32_TILE_ROWS][PRG32_TILE_COLS];
    uint16_t tile_dict_id[256];
    uint16_t tile_fg[256];
    uint16_t tile_bg[256];
    uint16_t cell_palette[2];
    uint16_t glyph_palette[2];
    prg32_indexed_sprite_t cell, glyph, prof, student[3], item, puff, layer, pizza;

    obj_t cur[OBJ_COUNT], prev[OBJ_COUNT];
    region_t regions[MAX_REGIONS];
    int region_count;
    popup_t popups[4];

    uint8_t app, screen, demo, anim_phase, hud_dirty, card_shown;
    uint16_t anim_fg, anim_bg;
    uint32_t last_input, app_tick, best;
    uint32_t hud_score, hud_best, hud_bell;
    uint8_t hud_lives, hud_flour, hud_lesson;
    int16_t touch_x0, touch_y0, touch_x1, touch_y1;  /* cells changed this frame */
    int8_t mask_y0, mask_y1;                         /* rows of a text panel, or -1 */
    uint8_t split_flush, probe_frames, probe_decided;
    uint32_t last_ms, tick_accum, probe_mark, probe_split_ms;
    int16_t title_x;
    pz_music_t music;
} v;

static pz_game_t game;

/* ---- colours ------------------------------------------------------------ */

/* The same quantiser as the firmware (and tools/palette.py): the palette cell
 * that an RGB565 value will land in on the indexed framebuffer. */
static uint8_t cube_index(uint16_t color) {
    static const uint16_t named[8] = {0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f};
    for (uint8_t i = 0; i < 8; ++i)
        if (color == named[i]) return i;
    unsigned r = ((color >> 11) & 31u) * 5u / 31u;
    unsigned g = ((color >> 5) & 63u) * 5u / 63u;
    unsigned b = (color & 31u) * 5u / 31u;
    return (uint8_t)(16u + r * 36u + g * 6u + b);
}

/* ---- tiles and text ------------------------------------------------------ */

static void set_cell(int x, int y, uint8_t id) {
    if (x < 0 || y < 0 || x >= PRG32_TILE_COLS || y >= PRG32_TILE_ROWS) return;
    if (v.map[y][x] != id) {
        v.map[y][x] = id;
        v.hud_dirty = 1;
        /* The firmware will repaint this cell, over whoever stands on it. */
        if (x < v.touch_x0) v.touch_x0 = (int16_t)x;
        if (y < v.touch_y0) v.touch_y0 = (int16_t)y;
        if (x > v.touch_x1) v.touch_x1 = (int16_t)x;
        if (y > v.touch_y1) v.touch_y1 = (int16_t)y;
    }
    prg32_tile_put((uint8_t)x, (uint8_t)y, id);
}

static void define_tile(uint8_t id, unsigned dict, uint16_t fg, uint16_t bg) {
    v.tile_dict_id[id] = (uint16_t)dict;
    v.tile_fg[id] = fg;
    v.tile_bg[id] = bg;
    prg32_tile_define(id, &tile_dict[dict * 8u], fg, bg);
}

static int font_of(char c) {
    int k = (int)(unsigned char)c - 32;
    return (k < 0 || k >= 96) ? 0 : ascii_to_font[k];
}

static void print_at(int x, int y, const char *s, int base) {
    for (; *s; ++s, ++x) set_cell(x, y, (uint8_t)(base + font_of(*s)));
}

static int text_len(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}

static void print_center(int y, const char *s, int base) {
    print_at((PRG32_TILE_COLS - text_len(s)) / 2, y, s, base);
}

static void print_number(int x, int y, uint32_t value, int digits, int base) {
    for (int i = digits - 1; i >= 0; --i) {
        set_cell(x + i, y, (uint8_t)(base + font_of((char)('0' + value % 10u))));
        value /= 10u;
    }
}

static void fill_cells(int x0, int y0, int x1, int y1, uint8_t id) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) set_cell(x, y, id);
}

/* The data of one screen. A switch and not a table: no pointers in static data. */
typedef struct {
    const uint16_t *palette;
    const uint8_t *tiles, *map;
    int palette_count, tile_count, map_len, first_row;
    uint16_t anim_fg, anim_bg;
} screen_t;

#define SCREEN_CASE(n) case n: \
    s.palette = screen##n##_palette; s.palette_count = screen##n##_palette_count; \
    s.tiles = screen##n##_tiles; s.tile_count = SCREEN##n##_TILES; \
    s.map = screen##n##_map; s.map_len = SCREEN##n##_MAP_LEN; s.first_row = SCREEN##n##_FIRST_ROW; \
    s.anim_fg = SCREEN##n##_ANIM_FG; s.anim_bg = SCREEN##n##_ANIM_BG; break

static screen_t screen_info(int n) {
    screen_t s;
    switch (n) {
        SCREEN_CASE(0);
        SCREEN_CASE(1);
        SCREEN_CASE(2);
        SCREEN_CASE(3);
        SCREEN_CASE(4);
        SCREEN_CASE(5);
        default:
        SCREEN_CASE(6);
    }
    return s;
}

/* Put rows y0..y1 of the screen's own picture back, by re-reading its map. */
static void scene_rows(int n, int y0, int y1) {
    screen_t s = screen_info(n);
    int cell = 0;
    for (int i = 0; i < s.map_len; i += 2) {
        for (int run = s.map[i]; run > 0; --run, ++cell) {
            int x = cell % PRG32_TILE_COLS;
            int y = s.first_row + cell / PRG32_TILE_COLS;
            if (y > y1) return;
            if (y >= y0) set_cell(x, y, (uint8_t)(SCENE_BASE + s.map[i + 1]));
        }
    }
}

/* Row 24 outside the peels: animated by redefining tiles, the classic trick. */
static void anim_row(void) {
    for (int x = 0; x < PRG32_TILE_COLS; ++x) {
        int plate = 0;
        for (int i = 0; i < game.pizza_count; ++i) {
            int left = game.pizzas[i].left;
            if (x >= left - 1 && x <= left + PZ_PIZZA_CELLS) plate = 1;
        }
        if (!plate) set_cell(x, 24, (uint8_t)(ANIM_BASE + x % ANIM_COUNT));
    }
}

static void lab_boards(void) {
    print_at(10, 6, "ADDI A0,X0,4", FONT_BASE);
    print_at(10, 14, "6 + 2 VOICES", FONT2_BASE);
}

static void hud_reset(void);

static void screen_load(int n) {
    screen_t s = screen_info(n);
    v.screen = (uint8_t)n;
    /* Every colour of this screen becomes exact in its palette cell. On the
     * indexed framebuffer this is what makes the colours right; on QEMU's
     * direct-colour surface it is harmless. */
    for (int i = 0; i < s.palette_count; ++i) prg32_palette_set(cube_index(s.palette[i]), s.palette[i]);
    prg32_tile_clear(COL_INK);
    for (int i = 0; i < FONT_COUNT; ++i) {
        define_tile((uint8_t)(FONT_BASE + i), (unsigned)i, COL_CREAM, COL_INK);
        define_tile((uint8_t)(FONT2_BASE + i), (unsigned)i, COL_LEMON, COL_INK);
    }
    v.anim_fg = s.anim_fg;
    v.anim_bg = s.anim_bg;
    for (int i = 0; i < ANIM_COUNT; ++i) define_tile((uint8_t)(ANIM_BASE + i), anim_dict[i], s.anim_fg, s.anim_bg);
    for (int i = 0; i < s.tile_count; ++i) {
        const uint8_t *t = &s.tiles[i * 4];
        define_tile((uint8_t)(SCENE_BASE + i), (unsigned)t[0] | ((unsigned)t[1] << 8), s.palette[t[2]], s.palette[t[3]]);
    }
    for (int y = 0; y < PRG32_TILE_ROWS; ++y)
        for (int x = 0; x < PRG32_TILE_COLS; ++x) v.map[y][x] = 0;
    fill_cells(0, 0, PRG32_TILE_COLS - 1, s.first_row - 1, FONT_BASE);
    scene_rows(n, s.first_row, PRG32_TILE_ROWS - 1);
    for (int i = 0; i < OBJ_COUNT; ++i) v.prev[i].w = 0;
    for (int i = 0; i < 4; ++i) v.popups[i].ttl = 0;
    v.mask_y0 = -1;
    v.hud_dirty = 1;
    prg32_band_set_colors(PRG32_BAND_TOP, COL_CREAM, COL_INK);
    prg32_band_set_colors(PRG32_BAND_BOTTOM, COL_CREAM, COL_INK);
}

/* ---- HUD ----------------------------------------------------------------- */

static void hud_reset(void) {
    print_at(1, 0, "SCORE", FONT_BASE);
    print_at(14, 0, "BEST", FONT_BASE);
    print_at(26, 0, "LESSON", FONT_BASE);
    print_at(1, 1, "LIVES", FONT_BASE);
    print_at(10, 1, "FLOUR", FONT_BASE);
    print_at(26, 1, "BELL", FONT_BASE);
    v.hud_score = 0xFFFFFFFFu;
    v.hud_best = 0xFFFFFFFFu;
    v.hud_bell = 0xFFFFFFFFu;
    v.hud_lives = 255;
    v.hud_flour = 255;
    v.hud_lesson = 255;
}

static void hud_update(void) {
    uint32_t bell = 0;
    if (game.par_ticks > game.lesson_ticks) bell = (game.par_ticks - game.lesson_ticks) * 33u / 1000u;
    if (game.score != v.hud_score) {
        v.hud_score = game.score;
        print_number(7, 0, game.score, 6, FONT2_BASE);
    }
    uint32_t best = game.score > v.best ? game.score : v.best;
    if (best != v.hud_best) {
        v.hud_best = best;
        print_number(19, 0, best, 6, FONT2_BASE);
    }
    if (game.lesson != v.hud_lesson) {
        v.hud_lesson = game.lesson;
        print_number(33, 0, (uint32_t)game.lesson + 1u, 2, FONT2_BASE);
    }
    if (game.lives != v.hud_lives) {
        v.hud_lives = game.lives;
        print_number(7, 1, game.lives, 1, FONT2_BASE);
    }
    if (game.flour != v.hud_flour) {
        v.hud_flour = game.flour;
        print_number(16, 1, game.flour, 1, FONT2_BASE);
    }
    if (bell != v.hud_bell) {
        v.hud_bell = bell;
        print_number(31, 1, bell, 3, game.hurry ? FONT_BASE : FONT2_BASE);
    }
}

/* ---- the object list ------------------------------------------------------ */

static void set_obj(int id, int x, int y, int w, int h, unsigned look) {
    obj_t *o = &v.cur[id];
    o->x = (int16_t)x;
    o->y = (int16_t)y;
    o->w = (int16_t)w;
    o->h = (int16_t)h;
    o->look = (uint16_t)look;
}

static int prof_frame(void) {
    if (game.cheering) return 12;
    if (game.phase == PH_DYING) return 10 + ((game.phase_timer >> 3) & 1);
    if (game.p.ladder >= 0) return 8 + ((game.p.y >> 3) & 1);
    int base = game.facing < 0 ? 4 : 0;
    if (game.puff > PZ_PUFF_TICKS - 6) return base + 3;
    static const uint8_t walk[4] = {0, 1, 0, 2};
    return base + walk[(game.anim >> 2) & 3];
}

static int student_frame(const pz_student_t *s) {
    switch (s->state) {
    case ST_FLOURED: return 6 + ((s->anim >> 3) & 1);
    case ST_RIDING:
    case ST_FED: return 8 + ((s->anim >> 3) & 1);
    default: break;
    }
    if (s->b.ladder >= 0) return 4 + ((s->b.y >> 3) & 1);
    return (s->b.dx < 0 ? 2 : 0) + ((s->anim >> 3) & 1);
}

/* A hungry student near the Professor raises a question mark. */
static int student_asks(const pz_student_t *s) {
    if (s->state != ST_WALK) return 0;
    int dx = pz_abs(s->b.x - game.p.x), dy = pz_abs(s->b.y - game.p.y);
    return dx < 44 && dy < 24;
}

static void build_game_objects(void) {
    int flash = game.invulnerable && ((game.invulnerable >> 2) & 1) && game.phase == PH_PLAY;
    if (flash) set_obj(OBJ_PROF, 0, 0, 0, 0, 0);
    else set_obj(OBJ_PROF, game.p.x - 8, game.p.y - 16, 16, 16, (unsigned)prof_frame());
    for (int i = 0; i < PZ_MAX_STUDENTS; ++i) {
        const pz_student_t *s = &game.students[i];
        if (i >= game.student_count || s->state == ST_AWAY) {
            set_obj(OBJ_STUDENT + i, 0, 0, 0, 0, 0);
            continue;
        }
        int asks = student_asks(s);
        unsigned look = (unsigned)student_frame(s) | ((unsigned)asks << 8);
        if (asks) set_obj(OBJ_STUDENT + i, s->b.x - 8, s->b.y - 24, 16, 24, look);
        else set_obj(OBJ_STUDENT + i, s->b.x - 8, s->b.y - 16, 16, 16, look);
    }
    if (game.puff) {
        unsigned frame = game.puff > PZ_PUFF_TICKS / 2 ? (game.puff_dir ? 2u : 0u) : 1u;
        set_obj(OBJ_PUFF, game.puff_x - 8, game.puff_y - 16, 16, 16, frame);
    } else {
        set_obj(OBJ_PUFF, 0, 0, 0, 0, 0);
    }
    if (game.item != ITEM_NONE && (game.item_timer > 60 || ((game.item_timer >> 2) & 1)))
        set_obj(OBJ_ITEM, game.item_cell * PZ_CELL, pz_floor_y(game.item_floor) - 16, 16, 16, game.item);
    else
        set_obj(OBJ_ITEM, 0, 0, 0, 0, 0);
    for (int i = 0; i < PZ_MAX_LAYERS; ++i) {
        if (i >= game.layer_count) {
            set_obj(OBJ_LAYER + i, 0, 0, 0, 0, 0);
            continue;
        }
        const pz_layer_t *l = &game.layers[i];
        set_obj(OBJ_LAYER + i, l->x, l->y - (l->bounce ? 1 : 0), PZ_LAYER_W, PZ_LAYER_H + 3,
                l->pressed | ((unsigned)(l->bounce != 0) << 5));
    }
    for (int i = 0; i < PZ_MAX_PIZZAS; ++i) {
        const pz_pizza_t *p = i < game.pizza_count ? &game.pizzas[i] : NULL;
        if (p && p->bake)
            set_obj(OBJ_PIZZA + i, p->left * PZ_CELL + 8, PZ_PLATE_Y - 24 - PZ_LAYER_H * p->total - (PZ_BAKE_TICKS - p->bake) / 4, 24, 24, 1);
        else
            set_obj(OBJ_PIZZA + i, 0, 0, 0, 0, 0);
    }
    for (int i = 0; i < 4; ++i) {
        const popup_t *p = &v.popups[i];
        if (p->ttl) {
            int digits = p->value >= 1000 ? 4 : p->value >= 100 ? 3 : 2;
            set_obj(OBJ_POPUP + i, p->x - digits * 4, p->y - 8, digits * 8, 8, p->value);
        } else {
            set_obj(OBJ_POPUP + i, 0, 0, 0, 0, 0);
        }
    }
}

/* Nothing is drawn across a text panel: reading comes first. */
static void apply_panel_mask(void) {
    if (v.mask_y0 < 0) return;
    int y0 = v.mask_y0 * 8, y1 = (v.mask_y1 + 1) * 8;
    for (int i = 0; i < OBJ_COUNT; ++i) {
        obj_t *o = &v.cur[i];
        if (o->w > 0 && o->y < y1 && o->y + o->h > y0) o->w = 0;
    }
}

static void clear_game_objects(void) {
    for (int i = 0; i < OBJ_TITLE; ++i) set_obj(i, 0, 0, 0, 0, 0);
}

/* ---- drawing one object --------------------------------------------------- */

static void draw_number(int x, int y, uint32_t value, int digits) {
    for (int i = digits - 1; i >= 0; --i) {
        prg32_sprite_draw_indexed(x + i * 8, y, &v.glyph, (uint32_t)font_of((char)('0' + value % 10u)));
        value /= 10u;
    }
}

static void draw_object(int id) {
    const obj_t *o = &v.cur[id];
    if (id == OBJ_PROF) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.prof, o->look);
    } else if (id < OBJ_PUFF) {
        const pz_student_t *s = &game.students[id - OBJ_STUDENT];
        int asks = (o->look >> 8) & 1;
        if (asks) prg32_sprite_draw_indexed(o->x + 4, o->y, &v.glyph, (uint32_t)font_of('?'));
        prg32_sprite_draw_indexed(o->x, o->y + (asks ? 8 : 0), &v.student[s->kind], o->look & 255u);
    } else if (id == OBJ_PUFF) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.puff, o->look);
    } else if (id == OBJ_ITEM) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.item, o->look);
    } else if (id < OBJ_PIZZA) {
        const pz_layer_t *l = &game.layers[id - OBJ_LAYER];
        for (int s = 0; s < PZ_PIZZA_CELLS; ++s) {
            int piece = l->kind * 3 + (s == 0 ? 0 : s == PZ_PIZZA_CELLS - 1 ? 2 : 1);
            int sink = (l->pressed >> s) & 1 ? 2 : 0;
            prg32_sprite_draw_indexed(o->x + s * PZ_CELL, o->y + sink, &v.layer, (uint32_t)piece);
        }
    } else if (id < OBJ_POPUP) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.pizza, 0);
    } else if (id < OBJ_TITLE) {
        int digits = o->look >= 1000 ? 4 : o->look >= 100 ? 3 : 2;
        draw_number(o->x, o->y, o->look, digits);
    } else if (id == OBJ_TITLE) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.prof, o->look);
    } else if (id == OBJ_TITLE + 1) {
        prg32_sprite_draw_indexed(o->x, o->y, &v.pizza, 0);
    } else {
        prg32_sprite_draw_indexed(o->x, o->y, &v.student[id - OBJ_TITLE - 2], o->look);
    }
}

/* Painter's order: scenery, then people, then the Professor, then text. */
static void draw_objects_in(const region_t *r) {
    static const uint8_t order[6][2] = {
        {OBJ_LAYER, OBJ_PIZZA}, {OBJ_ITEM, OBJ_ITEM + 1}, {OBJ_PIZZA, OBJ_POPUP},
        {OBJ_STUDENT, OBJ_ITEM}, {OBJ_PROF, OBJ_PROF + 1}, {OBJ_POPUP, OBJ_COUNT},
    };
    for (int pass = 0; pass < 6; ++pass) {
        for (int id = order[pass][0]; id < order[pass][1]; ++id) {
            const obj_t *o = &v.cur[id];
            if (o->w <= 0) continue;
            if (o->x + o->w <= r->x0 || o->x >= r->x1 || o->y + o->h <= r->y0 || o->y >= r->y1) continue;
            draw_object(id);
        }
    }
}

/* ---- dirty regions -------------------------------------------------------- */

static void add_region(int x0, int y0, int x1, int y1) {
    /* Grow to whole cells, since a cell is what can be repainted. */
    x0 &= ~7;
    y0 &= ~7;
    x1 = (x1 + 7) & ~7;
    y1 = (y1 + 7) & ~7;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PRG32_GAME_W) x1 = PRG32_GAME_W;
    if (y1 > PRG32_GAME_H) y1 = PRG32_GAME_H;
    if (x0 >= x1 || y0 >= y1) return;
    if (v.region_count == MAX_REGIONS) {
        /* Out of slots: fold everything into the last one. */
        region_t *r = &v.regions[MAX_REGIONS - 1];
        if (x0 < r->x0) r->x0 = (int16_t)x0;
        if (y0 < r->y0) r->y0 = (int16_t)y0;
        if (x1 > r->x1) r->x1 = (int16_t)x1;
        if (y1 > r->y1) r->y1 = (int16_t)y1;
        return;
    }
    region_t *r = &v.regions[v.region_count++];
    r->x0 = (int16_t)x0;
    r->y0 = (int16_t)y0;
    r->x1 = (int16_t)x1;
    r->y1 = (int16_t)y1;
}

static void merge_into(region_t *a, const region_t *b) {
    if (b->x0 < a->x0) a->x0 = b->x0;
    if (b->y0 < a->y0) a->y0 = b->y0;
    if (b->x1 > a->x1) a->x1 = b->x1;
    if (b->y1 > a->y1) a->y1 = b->y1;
}

/* Close the regions under two rules, until nothing changes: touching regions
 * merge, and an object that a region touches must lie wholly inside it (its
 * pixels outside would otherwise be repainted on top of another region). */
static void close_regions(void) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int a = 0; a < v.region_count; ++a) {
            for (int b = a + 1; b < v.region_count; ++b) {
                region_t *ra = &v.regions[a], *rb = &v.regions[b];
                if (ra->x1 <= rb->x0 || rb->x1 <= ra->x0 || ra->y1 <= rb->y0 || rb->y1 <= ra->y0) continue;
                merge_into(ra, rb);
                v.regions[b] = v.regions[--v.region_count];
                changed = 1;
                --b;
            }
            for (int id = 0; id < OBJ_COUNT; ++id) {
                const obj_t *o = &v.cur[id];
                region_t *r = &v.regions[a];
                if (o->w <= 0) continue;
                if (o->x + o->w <= r->x0 || o->x >= r->x1 || o->y + o->h <= r->y0 || o->y >= r->y1) continue;
                int x0 = o->x & ~7, y0 = o->y & ~7, x1 = (o->x + o->w + 7) & ~7, y1 = (o->y + o->h + 7) & ~7;
                if (x1 > PRG32_GAME_W) x1 = PRG32_GAME_W;
                if (y1 > PRG32_GAME_H) y1 = PRG32_GAME_H;
                if (x0 < 0) x0 = 0;
                if (y0 < 0) y0 = 0;
                if (x0 < r->x0 || y0 < r->y0 || x1 > r->x1 || y1 > r->y1) {
                    if (x0 < r->x0) r->x0 = (int16_t)x0;
                    if (y0 < r->y0) r->y0 = (int16_t)y0;
                    if (x1 > r->x1) r->x1 = (int16_t)x1;
                    if (y1 > r->y1) r->y1 = (int16_t)y1;
                    changed = 1;
                }
            }
        }
    }
}

static void paint_cell(int cx, int cy) {
    uint8_t id = v.map[cy][cx];
    v.cell_palette[0] = v.tile_bg[id];
    v.cell_palette[1] = v.tile_fg[id];
    prg32_sprite_draw_indexed(cx * PRG32_TILE_W, cy * PRG32_TILE_H, &v.cell, v.tile_dict_id[id]);
}

static void touch_reset(void) {
    v.touch_x0 = PRG32_TILE_COLS;
    v.touch_y0 = PRG32_TILE_ROWS;
    v.touch_x1 = -1;
    v.touch_y1 = -1;
}

static void render_objects(void) {
    v.region_count = 0;
    /* Objects under cells the firmware repainted must be painted back. */
    if (v.touch_x1 >= 0) {
        for (int i = 0; i < OBJ_COUNT; ++i) {
            const obj_t *c = &v.cur[i];
            if (c->w <= 0) continue;
            if (c->x + c->w <= v.touch_x0 * 8 || c->x >= (v.touch_x1 + 1) * 8 ||
                c->y + c->h <= v.touch_y0 * 8 || c->y >= (v.touch_y1 + 1) * 8) continue;
            add_region(c->x, c->y, c->x + c->w, c->y + c->h);
        }
        touch_reset();
    }
    for (int i = 0; i < OBJ_COUNT; ++i) {
        const obj_t *c = &v.cur[i], *p = &v.prev[i];
        if (c->w == p->w && c->x == p->x && c->y == p->y && c->h == p->h && c->look == p->look) continue;
        if (c->w > 0 && p->w > 0 &&
            pz_abs(c->x - p->x) <= 16 && pz_abs(c->y - p->y) <= 16) {
            int x0 = c->x < p->x ? c->x : p->x, y0 = c->y < p->y ? c->y : p->y;
            int x1 = c->x + c->w > p->x + p->w ? c->x + c->w : p->x + p->w;
            int y1 = c->y + c->h > p->y + p->h ? c->y + c->h : p->y + p->h;
            add_region(x0, y0, x1, y1);
        } else {
            if (p->w > 0) add_region(p->x, p->y, p->x + p->w, p->y + p->h);
            if (c->w > 0) add_region(c->x, c->y, c->x + c->w, c->y + c->h);
        }
    }
    close_regions();
    for (int k = 0; k < v.region_count; ++k) {
        const region_t *r = &v.regions[k];
        for (int cy = r->y0 >> 3; cy < (r->y1 >> 3); ++cy)
            for (int cx = r->x0 >> 3; cx < (r->x1 >> 3); ++cx) paint_cell(cx, cy);
        draw_objects_in(r);
        /* Each group of objects is one small box on the SPI bus. */
        if (v.split_flush) prg32_gfx_present();
    }
    for (int i = 0; i < OBJ_COUNT; ++i) v.prev[i] = v.cur[i];
}

/* ---- screens and flow ------------------------------------------------------ */

static void read_best(void) {
    prg32_score_t s;
    if (prg32_score_count(SCORE_GAME) > 0 && prg32_score_get(SCORE_GAME, 0, &s) == 0) v.best = s.score;
}

static void music_for_lesson(void) {
    pz_music_play(&v.music, (game.lesson & 1) ? SONG_MERCATO : SONG_PIAZZA);
}

static void panel(int y0, int y1) {
    fill_cells(0, y0, PRG32_TILE_COLS - 1, y1, FONT_BASE);
    v.mask_y0 = (int8_t)y0;
    v.mask_y1 = (int8_t)y1;
}

static void lesson_screen(void) {
    screen_load(game.lesson % LEVEL_COUNT);
    anim_row();
    if (game.lesson % LEVEL_COUNT == 4) lab_boards();
    hud_reset();
    hud_update();
    prg32_band_set_game_info("YOU HAVE GOT PIZZA DELUXE");
}

static void show_ready(void) {
    int n = game.lesson % LEVEL_COUNT;
    char name[LEVEL_NAME_LEN];
    for (int i = 0; i < LEVEL_NAME_LEN; ++i) name[i] = level_names[n * LEVEL_NAME_LEN + i];
    panel(9, 11);
    print_center(9, name, FONT_BASE);
    print_center(10, game.lesson == 0 ? "WALK OVER THE INGREDIENTS!" : "THE STUDENTS ARE HUNGRY!", FONT2_BASE);
    print_center(11, "A: FLOUR   START: PAUSE", FONT_BASE);
    v.card_shown = 1;
}

static void hide_panel(int y0, int y1) {
    scene_rows(v.screen, y0, y1);
    if (game.lesson % LEVEL_COUNT == 4) lab_boards();
    v.mask_y0 = -1;
}

static void start_lesson(int lesson, int fresh, uint32_t seed) {
    if (fresh) pz_new_game(&game, level_data, level_offsets, LEVEL_COUNT, seed, lesson);
    else pz_load_lesson(&game, lesson);
    v.app = APP_PLAY;
    v.app_tick = 0;
    v.card_shown = 0;
    v.probe_frames = 0;
    v.probe_decided = 0;
    lesson_screen();
    show_ready();
    pz_music_play(&v.music, SONG_READY);
}

static void enter_title(void) {
    v.app = APP_TITLE;
    v.app_tick = 0;
    v.demo = 0;
    v.card_shown = 0;
    read_best();
    screen_load(TITLE_SCREEN);
    panel(11, 12);
    print_center(11, "DELUXE EDITION", FONT2_BASE);
    print_center(12, "C'E PIZZA PER TE", FONT_BASE);
    print_at(1, 24, "UNIPARTHENOPE 2026", FONT_BASE);
    print_at(28, 24, prg32_audio_get_mode() == PRG32_AUDIO_MODE_STEREO ? "STEREO 8V" : "MONO 8V", FONT_BASE);
    print_center(17, "HIGH SCORE", FONT_BASE);
    print_number(17, 18, v.best, 6, FONT2_BASE);
    clear_game_objects();
    v.title_x = -40;
    pz_music_play(&v.music, SONG_TITLE);
    prg32_band_set_game_info("YOU HAVE GOT PIZZA DELUXE");
}

static void title_objects(void) {
    /* The eternal chase, across the bay. */
    int x = v.title_x;
    int t = (int)v.app_tick;
    set_obj(OBJ_TITLE, x, 168, 16, 16, (unsigned)((t >> 2) & 1));
    set_obj(OBJ_TITLE + 1, x + 22, 158, 24, 24, 1);
    set_obj(OBJ_TITLE + 2, x - 34, 168, 16, 16, (unsigned)((t >> 3) & 1));
    set_obj(OBJ_TITLE + 3, x - 58, 168, 16, 16, (unsigned)(((t >> 3) + 1) & 1));
    if (t & 1) ++v.title_x;
    if (v.title_x > PRG32_GAME_W + 60) v.title_x = -40;
}

static void clear_title_objects(void) {
    for (int i = OBJ_TITLE; i < OBJ_COUNT; ++i) set_obj(i, 0, 0, 0, 0, 0);
}

static void show_card(void) {
    panel(7, 16);
    char head[] = "LESSON 00 PASSED!";
    head[7] = (char)('0' + (game.lesson + 1) / 10);
    head[8] = (char)('0' + (game.lesson + 1) % 10);
    print_center(8, head, FONT_BASE);
    if (game.lode) {
        print_center(10, "MARK: 30 E LODE!", FONT2_BASE);
    } else {
        char mark[] = "MARK: 00/30";
        mark[6] = (char)('0' + game.grade / 10);
        mark[7] = (char)('0' + game.grade % 10);
        print_center(10, mark, FONT2_BASE);
    }
    print_at(13, 12, "FED:", FONT_BASE);
    print_number(19, 12, game.fed_count, 2, FONT2_BASE);
    print_at(13, 13, "SCORE:", FONT_BASE);
    print_number(19, 13, game.score, 6, FONT2_BASE);
    print_center(15, "PRESS A", FONT_BASE);
    v.app = APP_CARD;
    v.app_tick = 0;
    pz_music_play(&v.music, game.lode ? SONG_LODE : SONG_CLEAR);
}

static void show_ending(void) {
    panel(5, 18);
    print_center(6, "CONGRATULATIONS, PROFESSOR!", FONT2_BASE);
    print_center(8, "THE WHOLE CLASS IS FED", FONT_BASE);
    print_center(9, "AND FOOLISH ENOUGH", FONT_BASE);
    print_center(10, "TO TRY HARD THINGS.", FONT_BASE);
    print_center(12, "THEY ALL GRADUATED!", FONT2_BASE);
    print_center(14, "NEXT YEAR'S CLASS", FONT_BASE);
    print_center(15, "IS EVEN HUNGRIER...", FONT_BASE);
    print_center(17, "PRESS A", FONT_BASE);
    v.app = APP_ENDING;
    v.app_tick = 0;
    pz_music_play(&v.music, SONG_LODE);
}

static void show_gameover(void) {
    panel(7, 15);
    print_center(8, "THE BELL HAS RUNG", FONT_BASE);
    print_center(10, "GAME OVER", FONT2_BASE);
    print_at(13, 12, "SCORE:", FONT_BASE);
    print_number(19, 12, game.score, 6, FONT2_BASE);
    print_center(14, "PRESS A", FONT_BASE);
    v.app = APP_GAMEOVER;
    v.app_tick = 0;
    pz_music_play(&v.music, SONG_GAMEOVER);
    if (!v.demo && game.score) {
        if (game.score > v.best) v.best = game.score;
        prg32_score_submit_current_player(SCORE_GAME, game.score);
    }
}

/* ---- events: what the rules said happened ----------------------------------- */

static int pan_of(int x) {
    int pan = (x - PRG32_GAME_W / 2) * 48 / (PRG32_GAME_W / 2);
    return pan == 0 ? 1 : pan;
}

static void popup(int x, int y, uint16_t value) {
    for (int i = 0; i < 4; ++i) {
        if (!v.popups[i].ttl) {
            v.popups[i].x = (int16_t)x;
            v.popups[i].y = (int16_t)y;
            v.popups[i].value = value;
            v.popups[i].ttl = 28;
            return;
        }
    }
}

static void handle_events(void) {
    for (int i = 0; i < game.event_count; ++i) {
        const pz_event_t *e = &game.events[i];
        int pan = pan_of(e->x);
        switch (e->type) {
        case EV_PRESS: {
            /* Each piece rings one note higher: walking across a layer plays a scale. */
            static const uint8_t scale[6] = {0, 0, 2, 4, 5, 7};
            pz_sfx_play(&v.music, SFX_PRESS, pan, scale[e->a > 5 ? 5 : e->a]);
            break;
        }
        case EV_DROP:
            pz_sfx_play(&v.music, SFX_DROP, pan, e->a * 3);
            popup(e->x, e->y - 4, e->value);
            break;
        case EV_LAND: pz_sfx_play(&v.music, SFX_LAND, pan, 0); break;
        case EV_STACK: pz_sfx_play(&v.music, SFX_STACK, pan, e->a); break;
        case EV_PIZZA:
            pz_sfx_play(&v.music, SFX_PIZZA, pan, 0);
            popup(e->x, e->y, e->value);
            break;
        case EV_FLOUR: pz_sfx_play(&v.music, SFX_FLOUR, pan, 0); break;
        case EV_NO_FLOUR: pz_sfx_play(&v.music, SFX_EMPTY, pan, 0); break;
        case EV_STUN:
            pz_sfx_play(&v.music, SFX_STUN, pan, 0);
            popup(e->x, e->y, e->value);
            break;
        case EV_FEED:
            pz_sfx_play(&v.music, SFX_FEED, pan, 0);
            popup(e->x, e->y, e->value);
            break;
        case EV_ITEM_SPAWN: pz_sfx_play(&v.music, SFX_ITEM, pan, 0); break;
        case EV_ITEM_TAKE:
            pz_sfx_play(&v.music, SFX_TAKE, pan, 0);
            popup(e->x, e->y, e->value);
            break;
        case EV_RESPAWN: pz_sfx_play(&v.music, SFX_ENTER, pan, 0); break;
        case EV_EXTRA_LIFE: pz_sfx_play(&v.music, SFX_LIFE, 1, 0); break;
        case EV_DIE: pz_music_play(&v.music, SONG_DIE); break;
        case EV_HURRY: v.music.tempo_cut = 1; break;
        case EV_READY:
            if (e->a) {
                pz_music_play(&v.music, SONG_READY);
                show_ready();
            }
            break;
        default: break;
        }
    }
}

/* The arpeggio voice joins only when somebody is close: music that follows
 * the danger. It is one line of game logic for a lot of tension. */
static void music_follow_game(void) {
    int close = 0;
    for (int i = 0; i < game.student_count; ++i) {
        const pz_student_t *s = &game.students[i];
        if (s->state == ST_WALK && pz_abs(s->b.x - game.p.x) < 72 && pz_abs(s->b.y - game.p.y) < 40) close = 1;
    }
    v.music.mute_mask = (close || game.hurry) ? 0 : (uint8_t)(1u << 4);
}

/* ---- the three cartridge entry points --------------------------------------- */

static void make_sprite(prg32_indexed_sprite_t *s, const uint8_t *pixels, const uint16_t *palette, int colors,
                        int w, int h, int frames, int bpp, int transparent) {
    s->pixels = pixels;
    s->palette = palette;
    s->palette_count = (uint16_t)colors;
    s->width = (uint16_t)w;
    s->height = (uint16_t)h;
    s->frame_count = (uint16_t)frames;
    s->bits_per_pixel = (uint8_t)bpp;
    s->transparent_index = (int16_t)transparent;
}

void pizza_deluxe_init(void) {
    uint8_t *raw = (uint8_t *)&v;
    for (unsigned i = 0; i < sizeof(v); ++i) raw[i] = 0;
    make_sprite(&v.cell, tile_dict, v.cell_palette, 2, 8, 8, TILE_DICT_COUNT, PRG32_SPRITE_BPP_1, -1);
    v.glyph_palette[0] = COL_INK;
    v.glyph_palette[1] = COL_LEMON;
    make_sprite(&v.glyph, tile_dict, v.glyph_palette, 2, 8, 8, FONT_COUNT, PRG32_SPRITE_BPP_1, 0);
    make_sprite(&v.prof, prof_pixels, prof_palette, PROF_COLORS, 16, 16, PROF_FRAMES, PRG32_SPRITE_BPP_4, 0);
    for (int i = 0; i < 3; ++i)
        make_sprite(&v.student[i], student_pixels, student_palettes[i], STUDENT_COLORS, 16, 16, STUDENT_FRAMES, PRG32_SPRITE_BPP_4, 0);
    make_sprite(&v.item, item_pixels, item_palette, ITEM_COLORS, 16, 16, ITEM_FRAMES, PRG32_SPRITE_BPP_4, 0);
    make_sprite(&v.puff, puff_pixels, puff_palette, PUFF_COLORS, 16, 16, PUFF_FRAMES, PRG32_SPRITE_BPP_4, 0);
    make_sprite(&v.layer, layer_pixels, layer_palette, LAYER_COLORS, 8, 6, LAYER_PIECES, PRG32_SPRITE_BPP_4, 0);
    make_sprite(&v.pizza, pizza_pixels, layer_palette, LAYER_COLORS, 24, 24, 1, PRG32_SPRITE_BPP_4, 0);
    v.split_flush = 1;
    touch_reset();
    v.last_ms = prg32_ticks_ms();
    v.last_input = prg32_input_read();
    enter_title();
}

static uint32_t to_core_input(uint32_t buttons) {
    uint32_t in = 0;
    if (buttons & PRG32_BTN_LEFT) in |= PZ_IN_LEFT;
    if (buttons & PRG32_BTN_RIGHT) in |= PZ_IN_RIGHT;
    if (buttons & PRG32_BTN_UP) in |= PZ_IN_UP;
    if (buttons & PRG32_BTN_DOWN) in |= PZ_IN_DOWN;
    if (buttons & (PRG32_BTN_A | PRG32_BTN_B)) in |= PZ_IN_FLOUR;
    return in;
}

static void next_lesson(void) {
    int next = game.lesson + 1;
    if (next == LEVEL_COUNT && v.app != APP_ENDING) {
        show_ending();
        return;
    }
    start_lesson(next, 0, 0);
}

/* One 33 ms step of the application. */
static void update_step(uint32_t buttons, uint32_t pressed) {
    v.app_tick++;
    pz_music_tick(&v.music);

    switch (v.app) {
    case APP_TITLE:
        title_objects();
        if ((v.app_tick & 31u) == 0) {
            if (v.app_tick & 32u) print_center(15, "PRESS A TO TEACH", FONT_BASE);
            else scene_rows(TITLE_SCREEN, 15, 15);
        }
        if (pressed & (PRG32_BTN_A | PRG32_BTN_B)) {
            clear_title_objects();
            v.demo = 0;
            start_lesson(0, 1, prg32_ticks_ms() * 2654435761u + 1u);
        } else if (pressed & PRG32_BTN_START) {
            pz_music_stop(&v.music);
            prg32_audio_stop_all();
            prg32_scoreboard_show(SCORE_GAME, "THE BEST PROFESSORS");
            v.last_input = prg32_input_read();
            enter_title();
        } else if (v.app_tick > IDLE_TO_DEMO_TICKS) {
            clear_title_objects();
            v.demo = 1;
            start_lesson((int)(prg32_ticks_ms() >> 8) % LEVEL_COUNT, 1, prg32_ticks_ms() + 7u);
        }
        break;
    case APP_PLAY: {
        if (v.demo && (pressed || v.app_tick > DEMO_TICKS)) {
            enter_title();
            break;
        }
        if (!v.demo && (pressed & PRG32_BTN_START)) {
            v.app = APP_PAUSE;
            panel(10, 10);
            print_center(10, "PAUSED", FONT2_BASE);
            prg32_audio_stop_all();
            break;
        }
        int was_ready = game.phase == PH_READY;
        uint32_t input = v.demo ? pz_bot_input(&game) : to_core_input(buttons);
        pz_tick(&game, input);
        handle_events();
        if (was_ready && game.phase == PH_PLAY) {
            hide_panel(9, 11);
            v.card_shown = 0;
            music_for_lesson();
        }
        if (game.phase == PH_PLAY && !v.music.playing) music_for_lesson();
        if (game.phase == PH_PLAY) music_follow_game();
        for (int i = 0; i < 4; ++i) {
            if (v.popups[i].ttl) {
                v.popups[i].ttl--;
                if ((v.popups[i].ttl & 1) == 0) v.popups[i].y--;
            }
        }
        if (game.phase == PH_CLEAR && game.phase_timer == 0) {
            if (v.demo) enter_title();
            else show_card();
        } else if (game.phase == PH_GAMEOVER) {
            if (v.demo) enter_title();
            else show_gameover();
        }
        if (v.demo && (v.app_tick & 63u) == 0) {
            print_at(19, 1, (v.app_tick & 64u) ? "DEMO" : "    ", FONT_BASE);
        }
        break;
    }
    case APP_PAUSE:
        if (pressed & PRG32_BTN_START) {
            v.app = APP_PLAY;
            hide_panel(10, 10);
        }
        break;
    case APP_CARD:
        if ((pressed & (PRG32_BTN_A | PRG32_BTN_B | PRG32_BTN_START)) || v.app_tick > 450) next_lesson();
        break;
    case APP_ENDING:
        if ((pressed & (PRG32_BTN_A | PRG32_BTN_B | PRG32_BTN_START)) || v.app_tick > 900) {
            int next = game.lesson + 1;
            start_lesson(next, 0, 0);
        }
        break;
    default:
        if ((pressed & (PRG32_BTN_A | PRG32_BTN_B | PRG32_BTN_START)) || v.app_tick > 450) enter_title();
        break;
    }
}

/* Fixed timestep: game and music always advance in whole 33 ms ticks of the wall
 * clock. A host that draws slowly shows fewer frames, but the game keeps its speed
 * and the music its tempo. At most a few ticks are replayed, so a pause in the
 * host (the modal scoreboard, say) never fast-forwards the game. */
void pizza_deluxe_update(void) {
    uint32_t buttons = prg32_input_read();
    uint32_t pressed = buttons & ~v.last_input;
    v.last_input = buttons;
    uint32_t now = prg32_ticks_ms();
    v.tick_accum += now - v.last_ms;
    v.last_ms = now;
    if (v.tick_accum > TICK_MS * MAX_CATCH_UP) v.tick_accum = TICK_MS * MAX_CATCH_UP;
    while (v.tick_accum >= TICK_MS) {
        v.tick_accum -= TICK_MS;
        update_step(buttons, pressed);
        pressed = 0;
    }
}

/* Which is cheaper on this host: many small flushes, or one big box? On the real
 * SPI display small boxes win by far; on QEMU every present recopies the whole
 * panel, so one big box wins. Instead of guessing, each lesson times both for a
 * moment of real play and keeps the faster: a small experiment, run at run time. */
static void flush_probe(void) {
    if (v.probe_decided || v.app != APP_PLAY || game.phase != PH_PLAY) return;
    uint32_t now = prg32_ticks_ms();
    if (v.probe_frames == 0) {
        v.split_flush = 1;
        v.probe_mark = now;
    } else if (v.probe_frames == PROBE_FRAMES) {
        v.probe_split_ms = now - v.probe_mark;
        v.probe_mark = now;
        v.split_flush = 0;
    } else if (v.probe_frames == 2 * PROBE_FRAMES) {
        v.split_flush = v.probe_split_ms <= now - v.probe_mark;
        v.probe_decided = 1;
    }
    v.probe_frames++;
}

void pizza_deluxe_draw(void) {
    if (v.screen != TITLE_SCREEN && (v.app == APP_PLAY || v.app == APP_PAUSE)) hud_update();
    /* Character animation: one definition every few frames rewrites a whole row.
     * Never together with a HUD change, or the two would share one screen-tall box. */
    if (v.screen != TITLE_SCREEN && (v.app_tick & 7u) == 0 && !v.hud_dirty) {
        uint8_t phase = v.anim_phase = (uint8_t)((v.anim_phase + 1) % ANIM_COUNT);
        for (int i = 0; i < ANIM_COUNT; ++i)
            define_tile((uint8_t)(ANIM_BASE + i), anim_dict[(i + phase) % ANIM_COUNT], v.anim_fg, v.anim_bg);
        v.hud_dirty = 1;
    }
    if (v.hud_dirty) {
        /* Dirty cells only, with the firmware's own bookkeeping. */
        prg32_tile_present();
        v.hud_dirty = 0;
    }
    if (v.screen == TITLE_SCREEN) {
        for (int i = 0; i < OBJ_TITLE; ++i) set_obj(i, 0, 0, 0, 0, 0);
    } else {
        build_game_objects();
    }
    apply_panel_mask();
    render_objects();
    flush_probe();
}
