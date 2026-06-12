/* mikey.c - Mikey peripheral (timers/video/audio). Phase-1 register file only.
 * Timer stepping (timer.c), the video DMA readout, and audio land later. See
 * mikey.h / ROADMAP. */
#include "lynxrecomp/mikey.h"

lynx_mikey_t lynx_mikey;

void lynx_mikey_init(void) {
    for (int i = 0; i < 0x100; i++) lynx_mikey.reg[i] = 0;
}

uint8_t lynx_mikey_read(uint8_t off) {
    return lynx_mikey.reg[off];
}

void lynx_mikey_write(uint8_t off, uint8_t val) {
    lynx_mikey.reg[off] = val;
}
