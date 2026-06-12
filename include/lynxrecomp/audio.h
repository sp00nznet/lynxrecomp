/* audio.h - Mikey 4-channel audio.
 *
 * Each channel ($FD20+8*c) is a POKEY-style generator: a reload timer (same
 * clock dividers as the system timers) clocks a 12-bit LFSR whose feedback is
 * the inverted XOR of selected taps; a signed volume modulates the LFSR output
 * bit into the channel's DAC (optionally integrated). The four DACs sum to the
 * output. Channel registers live in lynx_mikey.reg; this evolves the LFSR/DAC
 * state and renders PCM.
 *
 * Per-channel bytes (offset from $FD20 + 8*c):
 *   +0 volume (signed)   +1 feedback taps   +2 dac (output)   +3 shift lo
 *   +4 reload (backup)   +5 control          +6 count          +7 other
 *      control: clock select (0-2), ENABLE_COUNT 0x08, ENABLE_RELOAD 0x10,
 *               ENABLE_INTEGRATE 0x20, FEEDBACK_7 0x80
 *      other:   shift-register bits 8-11 in the high nibble
 */
#ifndef LYNXRECOMP_AUDIO_H
#define LYNXRECOMP_AUDIO_H

#include <stdint.h>

void lynx_audio_init(void);

/* Advance all 4 channels by `us` microseconds (clocking their LFSRs). */
void lynx_audio_step(uint32_t us);

/* The current mixed output as signed 16-bit PCM (mono). */
int16_t lynx_audio_sample(void);

/* Re-seed a channel's LFSR from its shift-register registers (call when the
 * game writes shiftlo/other). c = 0..3. */
void lynx_audio_reseed(int c);

/* Write a mono 16-bit PCM buffer as a RIFF/WAV file. Returns 0 on success. */
int lynx_audio_write_wav(const char *path, const int16_t *pcm, int nsamples, int rate);

#endif /* LYNXRECOMP_AUDIO_H */
