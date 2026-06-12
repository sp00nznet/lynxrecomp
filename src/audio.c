/* audio.c - Mikey 4-channel audio. See audio.h. */
#include "lynxrecomp/audio.h"
#include "lynxrecomp/mikey.h"
#include <stdio.h>
#include <string.h>

#define AUD_BASE     0x20          /* channel 0 register base within Mikey */
#define A_VOLUME     0
#define A_FEEDBACK   1
#define A_DAC        2
#define A_SHIFTLO    3
#define A_RELOAD     4
#define A_CONTROL    5
#define A_COUNT      6
#define A_OTHER      7

#define ACTL_CLOCK     0x07
#define ACTL_COUNT     0x08
#define ACTL_RELOAD    0x10
#define ACTL_INTEGRATE 0x20
#define ACTL_FEEDBACK7 0x80

typedef struct {
    uint16_t lfsr;       /* 12-bit shift register */
    int      dac;        /* signed accumulator/output */
    uint32_t phase;      /* sub-period clock accumulator */
} aud_ch_t;

static aud_ch_t ch[4];

static uint8_t areg(int c, int field) { return lynx_mikey.reg[AUD_BASE + c * 8 + field]; }
static void    asetreg(int c, int field, uint8_t v) { lynx_mikey.reg[AUD_BASE + c * 8 + field] = v; }

void lynx_audio_init(void) {
    for (int c = 0; c < 4; c++) { ch[c].lfsr = 0; ch[c].dac = 0; ch[c].phase = 0; }
}

/* save-state: the per-channel LFSR/DAC/phase (channel config lives in
 * lynx_mikey.reg). */
size_t lynx_audio_state_size(void) { return sizeof(ch); }
size_t lynx_audio_state_save(uint8_t *b) { memcpy(b, ch, sizeof(ch)); return sizeof(ch); }
size_t lynx_audio_state_load(const uint8_t *b) { memcpy(ch, b, sizeof(ch)); return sizeof(ch); }

void lynx_audio_reseed(int c) {
    ch[c].lfsr = (uint16_t)(areg(c, A_SHIFTLO) | ((areg(c, A_OTHER) >> 4) << 8));
}

/* Clock one channel's LFSR + update its DAC. */
static void clock_lfsr(int c) {
    uint8_t fb   = areg(c, A_FEEDBACK);
    uint8_t ctl  = areg(c, A_CONTROL);
    /* 9 feedback switches map to taps in this order (Handy/Lynx hardware). */
    static const int tap[9] = { 7, 0, 1, 2, 3, 4, 5, 10, 11 };
    unsigned sw = (unsigned)((ctl >> 7) & 1)          /* sw0 -> tap 7  */
                | (unsigned)((fb & 0x3F) << 1)        /* sw1-6 -> taps 0-5 */
                | (unsigned)(((fb >> 6) & 3) << 7);   /* sw7-8 -> taps 10,11 */
    unsigned r = 0;
    for (int i = 0; i < 9; i++)
        if ((sw >> i) & 1) r ^= (ch[c].lfsr >> tap[i]) & 1;
    r = r ? 0 : 1;                                     /* inverted feedback */
    ch[c].lfsr = (uint16_t)(((ch[c].lfsr << 1) | r) & 0x0FFF);

    int vol = (int8_t)areg(c, A_VOLUME);
    int bit = ch[c].lfsr & 1;
    if (ctl & ACTL_INTEGRATE) {
        ch[c].dac += bit ? vol : -vol;
        if (ch[c].dac >  127) ch[c].dac =  127;
        if (ch[c].dac < -128) ch[c].dac = -128;
    } else {
        ch[c].dac = bit ? vol : -vol;
    }
    asetreg(c, A_DAC, (uint8_t)ch[c].dac);            /* mirror to the DAC reg */
}

void lynx_audio_step(uint32_t us) {
    for (int c = 0; c < 4; c++) {
        uint8_t ctl = areg(c, A_CONTROL);
        if (!(ctl & ACTL_COUNT)) continue;            /* channel disabled */
        unsigned clk = ctl & ACTL_CLOCK;
        if (clk == 7) continue;                       /* linked: not modelled yet */
        uint32_t period = 1u << clk;                  /* microseconds per count */
        ch[c].phase += us;
        while (ch[c].phase >= period) {
            ch[c].phase -= period;
            uint8_t cnt = areg(c, A_COUNT);
            if (cnt == 0) {                           /* underflow */
                if (ctl & ACTL_RELOAD) asetreg(c, A_COUNT, areg(c, A_RELOAD));
                clock_lfsr(c);
            } else {
                asetreg(c, A_COUNT, (uint8_t)(cnt - 1));
            }
        }
    }
}

int16_t lynx_audio_sample(void) {
    int mix = ch[0].dac + ch[1].dac + ch[2].dac + ch[3].dac;   /* -512..511 */
    int s = mix * 48;
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    return (int16_t)s;
}

int lynx_audio_write_wav(const char *path, const int16_t *pcm, int nsamples, int rate) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int datasz = nsamples * 2;
    int byterate = rate * 2;
    fwrite("RIFF", 1, 4, f);
    uint32_t riff = 36 + datasz; fwrite(&riff, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtsz = 16; fwrite(&fmtsz, 4, 1, f);
    uint16_t pcm16 = 1, chn = 1, bits = 16, align = 2;
    fwrite(&pcm16, 2, 1, f); fwrite(&chn, 2, 1, f);
    uint32_t r = (uint32_t)rate; fwrite(&r, 4, 1, f);
    uint32_t br = (uint32_t)byterate; fwrite(&br, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    uint32_t ds = (uint32_t)datasz; fwrite(&ds, 4, 1, f);
    fwrite(pcm, 2, (size_t)nsamples, f);
    fclose(f);
    return 0;
}
