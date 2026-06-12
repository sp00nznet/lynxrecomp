/* test_audio.c - Mikey audio: a configured channel produces sound. No game data. */
#include <stdio.h>
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/audio.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

#define R(c,f) lynx_mikey.reg[0x20 + (c)*8 + (f)]

int main(void) {
    lynx_mikey_init();

    /* Channel 0: volume 64, feedback tap 0, counting+reload, 1us clock,
     * reload 2 -> LFSR clocks ~ every 3us. Seed the shift register. */
    R(0,0) = 64;        /* volume       */
    R(0,1) = 0x01;      /* feedback taps */
    R(0,3) = 0xFF;      /* shift lo (seed) */
    R(0,4) = 2;         /* reload       */
    R(0,5) = 0x08 | 0x10 | 0x00;  /* ENABLE_COUNT | ENABLE_RELOAD | clk 0 */
    R(0,6) = 2;         /* count        */
    lynx_audio_reseed(0);

    /* generate 256 samples at ~22.7us/sample */
    int16_t pcm[256];
    int nonzero = 0, distinct_seen = 0; int16_t first = 0;
    for (int i = 0; i < 256; i++) {
        lynx_audio_step(23);
        pcm[i] = lynx_audio_sample();
        if (i == 0) first = pcm[i];
        if (pcm[i] != 0) nonzero++;
        if (pcm[i] != first) distinct_seen = 1;
    }
    CHECK(nonzero > 0, "channel produced a non-zero signal (%d/256 nonzero)", nonzero);
    CHECK(distinct_seen, "channel output varies over time (a waveform, not a constant)");

    /* a disabled channel is silent */
    lynx_mikey_init();
    R(0,0) = 64; R(0,5) = 0x00;     /* ENABLE_COUNT clear */
    R(0,3) = 0xFF; lynx_audio_reseed(0);
    int silent = 1;
    for (int i = 0; i < 64; i++) { lynx_audio_step(23); if (lynx_audio_sample() != 0) silent = 0; }
    CHECK(silent, "disabled channel is silent");

    /* WAV writer round-trips a header */
    for (int i = 0; i < 64; i++) pcm[i] = (int16_t)(i * 100);
    CHECK(lynx_audio_write_wav("audio_test.wav", pcm, 64, 16000) == 0, "wav written");

    printf(fails ? "FAIL: %d\n" : "PASS: audio (channel generates a varying waveform)\n", fails);
    return fails ? 1 : 0;
}
