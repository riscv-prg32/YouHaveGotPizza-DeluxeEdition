/*
 * Host implementation of the PRG32 calls used by the cartridge.
 *
 * A test includes this header and then src/pizza.c, so the unmodified cartridge
 * source runs on the workstation. Declarations come from the real PRG32 public
 * headers, so a signature that differs from the firmware is a compile error.
 * The graphics model follows components/prg32/prg32_display_ili9341.c and
 * prg32_tile.c step for step:
 *
 *   - a 320 x 200 framebuffer of palette indices (host_fb);
 *   - RGB565 drawing quantised to the 6x6x6 cube, as the firmware does;
 *   - one dirty bounding box, sent to a model of the panel (host_panel) only
 *     by prg32_gfx_present(), which also counts the pixels on the "SPI bus".
 *
 * So what the tests inspect is what a person would see on the display.
 */
#ifndef PZ_HOST_PRG32_H
#define PZ_HOST_PRG32_H

#include <stdio.h>
#include <string.h>

#define PZ_HOST 1
#include "prg32.h"

#define HOST_MAX_NOTES 200000

typedef struct {
    uint32_t ms;
    uint8_t channel, instrument, note, volume, kind; /* kind 1 on, 0 off */
    int8_t pan;
} host_note_t;

static uint8_t host_fb[PRG32_GAME_H][PRG32_GAME_W];
static uint16_t host_panel[PRG32_GAME_H][PRG32_GAME_W];
static uint16_t host_palette[256];
static int host_dx0 = 9999, host_dy0 = 9999, host_dx1 = -1, host_dy1 = -1;
static uint64_t host_spi_pixels, host_presents, host_abi_calls;
static uint32_t host_frame_pixels;

static uint32_t host_ms;
static uint32_t host_input;
static host_note_t host_notes[HOST_MAX_NOTES];
static int host_note_count;
static int host_audio_mode = PRG32_AUDIO_MODE_MONO;
static uint32_t host_score_last;
static int host_score_submits;
static int host_scoreboards;
static int host_bad_tile_calls;

/* tile engine state: mirrors prg32_tile.c */
typedef struct { uint8_t bits[8]; uint16_t fg, bg; } host_tile_t;
static host_tile_t host_tiles[256];
static uint8_t host_map[PRG32_TILE_ROWS][PRG32_TILE_COLS];
static uint8_t host_dirty[PRG32_TILE_ROWS][PRG32_TILE_COLS];

static uint8_t host_quantise(uint16_t color) {
    static const uint16_t named[8] = {0x0000, 0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0x07ff, 0xf81f};
    for (uint8_t i = 0; i < 8; ++i)
        if (color == named[i]) return i;
    unsigned r = ((color >> 11) & 31u) * 5u / 31u;
    unsigned g = ((color >> 5) & 63u) * 5u / 63u;
    unsigned b = (color & 31u) * 5u / 31u;
    return (uint8_t)(16u + r * 36u + g * 6u + b);
}

static void host_dirty_add(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > PRG32_GAME_W) w = PRG32_GAME_W - x;
    if (y + h > PRG32_GAME_H) h = PRG32_GAME_H - y;
    if (w <= 0 || h <= 0) return;
    if (x < host_dx0) host_dx0 = x;
    if (y < host_dy0) host_dy0 = y;
    if (x + w - 1 > host_dx1) host_dx1 = x + w - 1;
    if (y + h - 1 > host_dy1) host_dy1 = y + h - 1;
}

uint32_t prg32_ticks_ms(void) { return host_ms; }
uint32_t prg32_input_read(void) { return host_input; }

void prg32_palette_set(uint8_t index, uint16_t rgb565) {
    host_palette[index] = rgb565;
    host_dirty_add(0, 0, PRG32_GAME_W, PRG32_GAME_H);
}

void prg32_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    uint8_t index = host_quantise(color);
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= PRG32_GAME_H) continue;
        for (int px = x; px < x + w; ++px) {
            if (px >= 0 && px < PRG32_GAME_W) host_fb[py][px] = index;
        }
    }
    host_dirty_add(x, y, w, h);
}

void prg32_gfx_present(void) {
    ++host_presents;
    if (host_dx1 < host_dx0) return;
    for (int y = host_dy0; y <= host_dy1; ++y)
        for (int x = host_dx0; x <= host_dx1; ++x) host_panel[y][x] = host_palette[host_fb[y][x]];
    uint32_t n = (uint32_t)(host_dx1 - host_dx0 + 1) * (uint32_t)(host_dy1 - host_dy0 + 1);
    host_spi_pixels += n;
    host_frame_pixels += n;
    host_dx0 = 9999;
    host_dy0 = 9999;
    host_dx1 = -1;
    host_dy1 = -1;
}

void prg32_sprite_draw_indexed(int x, int y, const prg32_indexed_sprite_t *s, uint32_t frame) {
    ++host_abi_calls;
    if (frame >= s->frame_count) { ++host_bad_tile_calls; return; }
    size_t frame_bytes = ((size_t)s->width * s->height * s->bits_per_pixel + 7u) / 8u;
    const uint8_t *data = s->pixels + frame * frame_bytes;
    int wrote = 0;
    for (int row = 0; row < s->height; ++row) {
        int py = y + row;
        if (py < 0 || py >= PRG32_GAME_H) continue;
        for (int col = 0; col < s->width; ++col) {
            int px = x + col;
            if (px < 0 || px >= PRG32_GAME_W) continue;
            size_t bit = ((size_t)row * s->width + (size_t)col) * s->bits_per_pixel;
            unsigned shift = 8u - s->bits_per_pixel - (unsigned)(bit % 8u);
            unsigned index = (data[bit / 8u] >> shift) & ((1u << s->bits_per_pixel) - 1u);
            if (index >= s->palette_count || (s->transparent_index >= 0 && (int)index == s->transparent_index)) continue;
            host_fb[py][px] = host_quantise(s->palette[index]);
            wrote = 1;
        }
    }
    if (wrote) host_dirty_add(x, y, s->width, s->height);
}

void prg32_tile_clear(uint16_t color) {
    memset(host_map, 0, sizeof host_map);
    memset(host_dirty, 1, sizeof host_dirty);
    memset(host_tiles[0].bits, 0, 8);
    host_tiles[0].fg = color;
    host_tiles[0].bg = color;
}

void prg32_tile_define(uint8_t id, const uint8_t *bitmap, uint16_t fg, uint16_t bg) {
    ++host_abi_calls;
    memcpy(host_tiles[id].bits, bitmap, 8);
    host_tiles[id].fg = fg;
    host_tiles[id].bg = bg;
    for (int y = 0; y < PRG32_TILE_ROWS; ++y)
        for (int x = 0; x < PRG32_TILE_COLS; ++x)
            if (host_map[y][x] == id) host_dirty[y][x] = 1;
}

void prg32_tile_put(uint8_t tx, uint8_t ty, uint8_t id) {
    ++host_abi_calls;
    if (tx >= PRG32_TILE_COLS || ty >= PRG32_TILE_ROWS) { ++host_bad_tile_calls; return; }
    if (host_map[ty][tx] == id) return;
    host_map[ty][tx] = id;
    host_dirty[ty][tx] = 1;
}

void prg32_tile_present(void) {
    ++host_abi_calls;
    for (int y = 0; y < PRG32_TILE_ROWS; ++y) {
        for (int x = 0; x < PRG32_TILE_COLS; ++x) {
            if (!host_dirty[y][x]) continue;
            host_dirty[y][x] = 0;
            const host_tile_t *t = &host_tiles[host_map[y][x]];
            prg32_gfx_rect(x * 8, y * 8, 8, 8, t->bg);
            if (t->fg == t->bg) continue;
            for (int row = 0; row < 8; ++row)
                for (int col = 0; col < 8; ++col)
                    if (t->bits[row] & (1u << (7 - col))) prg32_gfx_rect(x * 8 + col, y * 8 + row, 1, 1, t->fg);
        }
    }
    prg32_gfx_present();
}

static void host_log_note(uint8_t channel, uint8_t instrument, uint8_t note, uint8_t volume, int8_t pan, uint8_t kind) {
    if (host_note_count < HOST_MAX_NOTES) {
        host_note_t *n = &host_notes[host_note_count++];
        n->ms = host_ms;
        n->channel = channel;
        n->instrument = instrument;
        n->note = note;
        n->volume = volume;
        n->pan = pan;
        n->kind = kind;
    }
}

void prg32_audio_note_on_pan(uint8_t channel, uint8_t instrument, uint8_t note, uint8_t volume, int8_t pan) {
    host_log_note(channel, instrument, note, volume, pan, 1);
}
void prg32_audio_note_off(uint8_t channel) { host_log_note(channel, 0, 0, 0, 0, 0); }
void prg32_audio_stop_all(void) {
    for (uint8_t c = 0; c < 8; ++c) host_log_note(c, 0, 0, 0, 0, 0);
}
prg32_audio_mode_t prg32_audio_get_mode(void) { return (prg32_audio_mode_t)host_audio_mode; }

void prg32_band_set_colors(uint8_t band, uint16_t fg, uint16_t bg) { (void)band; (void)fg; (void)bg; }
void prg32_band_set_game_info(const char *text) { (void)text; }

int prg32_score_count(const char *game) { (void)game; return 0; }
int prg32_score_get(const char *game, int index, prg32_score_t *out) { (void)game; (void)index; (void)out; return -1; }
int prg32_score_submit_current_player(const char *game, uint32_t score) {
    (void)game;
    host_score_last = score;
    ++host_score_submits;
    return 0;
}
int prg32_scoreboard_show(const char *game, const char *title) { (void)game; (void)title; ++host_scoreboards; return 0; }

#endif
