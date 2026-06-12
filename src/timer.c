/* timer.c - Mikey timer/IRQ stepping. Phase-1 stub. See timer.h / ROADMAP. */
#include "lynxrecomp/timer.h"

void lynx_timer_init(void) {
}

uint8_t lynx_timer_step(uint32_t cpu_cycles) {
    (void)cpu_cycles;
    /* TODO(phase 2): decrement each enabled timer, handle reload/chain, and
     * return a mask of timers that underflowed and raised an IRQ this step. */
    return 0;
}
