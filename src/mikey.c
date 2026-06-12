/* mikey.c - Mikey register access. Timers live in timer.c, video in video.c;
 * here we route the interrupt registers to the latch and store the rest. */
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/recomp_rt.h"   /* lynx_frame_hook */

lynx_mikey_t lynx_mikey;

void lynx_mikey_init(void) {
    for (int i = 0; i < 0x100; i++) lynx_mikey.reg[i] = 0;
    lynx_timer_init();
}

uint8_t lynx_mikey_read(uint8_t off) {
    switch (off) {
        case MIKEY_INTRST:
        case MIKEY_INTSET:
            return lynx_irq_latch;     /* both read the pending interrupt latch */
        default:
            return lynx_mikey.reg[off];
    }
}

void lynx_mikey_write(uint8_t off, uint8_t val) {
    switch (off) {
        case MIKEY_INTRST:                 /* write 1s to clear (acknowledge) */
            lynx_irq_ack(val);
            return;
        case MIKEY_INTSET:                 /* write 1s to set */
            lynx_irq_raise(val);
            return;
        case MIKEY_DISPADRH:               /* high byte written last = frame flip */
            lynx_mikey.reg[off] = val;
            if (lynx_frame_hook) lynx_frame_hook();
            return;
        default:
            lynx_mikey.reg[off] = val;
            return;
    }
}
