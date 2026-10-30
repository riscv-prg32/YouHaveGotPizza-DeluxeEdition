/*
 * Render the soundtrack with the firmware's own mixer, natively.
 *
 * The default PRG32 firmware profile is mono, and QEMU's audio stream is mono, so
 * neither can show the stereo image. This tool links the real
 * components/prg32_audio/audio_mixer.c and audio_synth.c from the PRG32 checkout
 * and drives them with the cartridge's own sequencer. The result is the signal a
 * firmware built for PRG32 Audio Plus (two amplifiers) sends to the I2S bus.
 *
 *   render_music <audio.block> <song> <seconds> <out.wav>  [tempo_cut]
 *
 * Prints one CSV row: peak, RMS, stereo correlation and side-to-mid energy.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_internal.h"

prg32_audio_state_t g_prg32_audio;
void prg32_audio_lock(void) {}
void prg32_audio_unlock(void) {}

#define PZ_NOTE_ON(ch, inst, note, vol, pan) \
    prg32_audio_note_on_pan((uint8_t)(ch), (uint8_t)(inst), (uint8_t)(note), (uint8_t)(vol), (int8_t)(pan))
#define PZ_NOTE_OFF(ch) prg32_audio_note_off((uint8_t)(ch))
#include "gen/music.inc"
#include "pz_music.h"

static pz_music_t music;
#define RATE 22050

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
    if (argc < 5) return 2;
    FILE *b = fopen(argv[1], "rb");
    if (!b) return 2;
    static uint8_t block[65536];
    size_t size = fread(block, 1, sizeof block, b);
    fclose(b);
    const prg32_audio_block_header_t *h = (const void *)block;
    if (size < sizeof *h || memcmp(h->magic, PRG32_AUDIO_BLOCK_MAGIC, 4)) return 3;

    g_prg32_audio.initialized = true;
    g_prg32_audio.config.sample_rate = RATE;
    g_prg32_audio.max_voices = 8;
    g_prg32_audio.master_volume = 96;   /* the firmware's default */
    for (int i = 0; i < 8; ++i) g_prg32_audio.channel_volume[i] = 255;
    for (int i = 0; i < h->instrument_count; ++i) {
        memcpy(&g_prg32_audio.instruments[i].desc, block + h->instruments_offset + (size_t)i * 8, 8);
        g_prg32_audio.instruments[i].present = true;
    }

    int song = atoi(argv[2]);
    int seconds = atoi(argv[3]);
    pz_music_play(&music, song);
    if (argc > 5) music.tempo_cut = (uint8_t)atoi(argv[5]);
    long total = (long)seconds * RATE;
    int16_t *pcm = calloc((size_t)total * 2, sizeof(int16_t));
    long done = 0;
    double carry = 0;
    while (done < total) {
        pz_music_tick(&music);
        carry += RATE * 0.033;
        int n = (int)carry;
        carry -= n;
        if (done + n > total) n = (int)(total - done);
        prg32_audio_mix_stereo(pcm + done * 2, (size_t)n);
        done += n;
    }
    double ll = 0, rr = 0, lr = 0, mid = 0, side = 0;
    int peak = 0;
    for (long i = 0; i < total; ++i) {
        double l = pcm[i * 2], r = pcm[i * 2 + 1];
        ll += l * l;
        rr += r * r;
        lr += l * r;
        mid += (l + r) * (l + r) / 4;
        side += (l - r) * (l - r) / 4;
        if (abs(pcm[i * 2]) > peak) peak = abs(pcm[i * 2]);
        if (abs(pcm[i * 2 + 1]) > peak) peak = abs(pcm[i * 2 + 1]);
    }
    FILE *w = fopen(argv[4], "wb");
    if (!w) return 2;
    uint32_t bytes = (uint32_t)total * 4;
    fwrite("RIFF", 1, 4, w); put32(w, 36 + bytes); fwrite("WAVEfmt ", 1, 8, w); put32(w, 16);
    put16(w, 1); put16(w, 2); put32(w, RATE); put32(w, RATE * 4); put16(w, 4); put16(w, 16);
    fwrite("data", 1, 4, w); put32(w, bytes);
    fwrite(pcm, 4, (size_t)total, w);
    fclose(w);
    double rms = sqrt((ll + rr) / (2.0 * (double)total));
    printf("%d,%d,%.1f,%.3f,%.3f,%.3f\n", song, peak, rms, lr / sqrt(ll * rr + 1), side / (mid + 1), sqrt(ll / (rr + 1)));
    free(pcm);
    return 0;
}
