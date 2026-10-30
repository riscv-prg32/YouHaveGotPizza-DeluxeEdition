/*
 * pz_music.h - the cartridge's sequencer and effects player.
 *
 * Why a sequencer here, when the firmware has a tracker? Three reasons, each a
 * lesson in docs/audio.md: the tracker ties each channel to one instrument and
 * knows no per-note pan, while the game wants ping-pong arpeggios and pulse-width
 * cycling; music here can react to the game in one frame; and the firmware's tracker
 * currently logs every event to the serial console while holding the audio lock.
 *
 * It is the classic pattern/order design of the 8-bit trackers: a pattern is 12
 * bytes (one per eighth note), and for each voice a song lists one (pattern,
 * transposition) pair per bar. No PRG32 call is made directly: the includer defines
 * PZ_NOTE_ON and PZ_NOTE_OFF, so the host tests can log what would be played.
 *
 * The tempo is a whole number of 33 ms frames per step, so no step is ever
 * longer than another.
 */
#ifndef PZ_MUSIC_H
#define PZ_MUSIC_H

#include <stdint.h>

#define PZ_SFX_FIRST 6
#define PZ_SFX_VOICES 2
#define PZ_MUSIC_VOICES 8
#define PZ_VOICE_HEADER 7

typedef struct {
    const uint8_t *song;
    uint8_t playing, bars, voices, loop;
    uint8_t fps, tempo_cut;
    uint8_t bar, step, frames;
    uint8_t mute_mask;
    uint8_t note_count[PZ_MUSIC_VOICES];
    int8_t swing[PZ_MUSIC_VOICES];
    const uint8_t *sfx[PZ_SFX_VOICES];
    uint8_t sfx_frames[PZ_SFX_VOICES];
    int8_t sfx_pan[PZ_SFX_VOICES];
    int8_t sfx_transpose[PZ_SFX_VOICES];
    uint8_t sfx_next;
    uint8_t finished;
} pz_music_t;

static void pz_music_silence(pz_music_t *m) {
    for (int v = 0; v < PZ_MUSIC_VOICES; ++v) {
        if (m->playing && v < m->voices) PZ_NOTE_OFF(v);
    }
}

/* Start a song; steps of a voice in song_data[]: 7 header bytes, then bars * 2. */
static void pz_music_play(pz_music_t *m, int song) {
    pz_music_silence(m);
    const uint8_t *d = song_data + song_offsets[song];
    m->song = d;
    m->fps = d[0];
    m->bars = d[1];
    m->voices = d[2];
    m->loop = d[3];
    m->bar = 0;
    m->step = 0;
    m->frames = 1;
    m->playing = 1;
    m->finished = 0;
    m->tempo_cut = 0;
    m->mute_mask = 0;
    for (int v = 0; v < PZ_MUSIC_VOICES; ++v) {
        m->note_count[v] = 0;
        m->swing[v] = 1;
    }
}

static void pz_music_stop(pz_music_t *m) {
    pz_music_silence(m);
    m->playing = 0;
}

static const uint8_t *pz_voice(const pz_music_t *m, int v) {
    return m->song + 4 + v * (PZ_VOICE_HEADER + m->bars * 2);
}

static void pz_music_step(pz_music_t *m) {
    for (int v = 0; v < m->voices; ++v) {
        const uint8_t *h = pz_voice(m, v);
        const uint8_t *order = h + PZ_VOICE_HEADER + m->bar * 2;
        uint8_t value = music_patterns[order[0] * MUSIC_STEPS + m->step];
        if ((m->mute_mask >> v) & 1u) {
            if (value >= 12 || value == MUSIC_OFF) PZ_NOTE_OFF(v);
            continue;
        }
        /* Effects own their channels while they sound. */
        if (v >= PZ_SFX_FIRST && m->sfx[v - PZ_SFX_FIRST]) continue;
        if (value == MUSIC_OFF) {
            PZ_NOTE_OFF(v);
        } else if (value != MUSIC_HOLD) {
            int note = value;
            int instrument = h[0];
            if (h[2] && value >= h[2]) instrument = h[1];          /* drum split */
            if (h[2] == 0) note += (int8_t)order[1];                /* transposition */
            if (h[3] > 1) instrument += m->note_count[v] % h[3];    /* pulse-width sweep */
            m->note_count[v]++;
            int pan = (int8_t)h[5];
            if (h[6]) {                                             /* ping-pong */
                m->swing[v] = (int8_t)-m->swing[v];
                pan += m->swing[v] * (int)h[6];
            }
            PZ_NOTE_ON(v, instrument, note, h[4], pan);
        }
    }
    if (++m->step >= MUSIC_STEPS) {
        m->step = 0;
        if (++m->bar >= m->bars) {
            m->bar = 0;
            if (!m->loop) {
                pz_music_stop(m);
                m->finished = 1;
            }
        }
    }
}

/* Start an effect on the free effect channel, or steal the older one. */
static void pz_sfx_play(pz_music_t *m, int sfx, int pan, int transpose) {
    int v = m->sfx_next;
    if (!m->sfx[v] || m->sfx[1 - v]) m->sfx_next = (uint8_t)(1 - v);
    else v = 1 - v;
    m->sfx[v] = sfx_data + sfx_offsets[sfx];
    m->sfx_frames[v] = 1;
    m->sfx_pan[v] = (int8_t)pan;
    m->sfx_transpose[v] = (int8_t)transpose;
}

/* Call once per frame. */
static void pz_music_tick(pz_music_t *m) {
    if (m->playing && --m->frames == 0) {
        int fps = m->fps - m->tempo_cut;
        m->frames = (uint8_t)(fps < 2 ? 2 : fps);
        pz_music_step(m);
    }
    for (int v = 0; v < PZ_SFX_VOICES; ++v) {
        const uint8_t *s = m->sfx[v];
        if (!s || --m->sfx_frames[v]) continue;
        if (s[0] == 255) {
            m->sfx[v] = 0;
            continue;
        }
        PZ_NOTE_ON(PZ_SFX_FIRST + v, s[0], s[1] + m->sfx_transpose[v], 220, m->sfx_pan[v]);
        m->sfx_frames[v] = s[2];
        m->sfx[v] = s + 3;
    }
}

#endif
