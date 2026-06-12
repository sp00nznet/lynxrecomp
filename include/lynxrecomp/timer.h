/* timer.h - Mikey timer / interrupt stepping (frame cadence).
 *
 * The 8 Mikey timers count down from a backup value at a selected clock; on
 * underflow they can reload, chain into the next timer, and raise an IRQ. The
 * vertical timer's underflow is the ~60 Hz frame interrupt the game's main
 * loop waits on. The recompiled CPU and this stepping run in lockstep; phase 1
 * fixes the API, the implementation lands with the runtime loop. */
#ifndef LYNXRECOMP_TIMER_H
#define LYNXRECOMP_TIMER_H

#include <stdint.h>

/* Advance all timers by `cpu_cycles`; returns a mask of newly-raised IRQs. */
uint8_t lynx_timer_step(uint32_t cpu_cycles);

void lynx_timer_init(void);

#endif /* LYNXRECOMP_TIMER_H */
