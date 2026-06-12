/* timer.h - Mikey timer / interrupt model (frame cadence).
 *
 * The 8 Mikey timers each count down from a backup value at a selected clock;
 * on underflow they can reload, cascade into a linked timer, and (if armed)
 * raise an interrupt. The vertical timer's underflow is the ~60 Hz frame
 * interrupt the game's main loop waits on. The recompiled CPU and this stepping
 * run together; the runtime advances time with lynx_timer_step().
 *
 * Each timer occupies 4 bytes in Mikey space at $FD00 + 4*i:
 *   +0 backup    reload value
 *   +1 control A  clock select (bits 0-2) + ENABLE_COUNT/ENABLE_RELOAD/
 *                 RESET_DONE/ENABLE_INT
 *   +2 count      current value (live; reads see it count down)
 *   +3 control B  TIMER_DONE + borrow flags
 */
#ifndef LYNXRECOMP_TIMER_H
#define LYNXRECOMP_TIMER_H

#include <stdint.h>
#include <stddef.h>

/* control A bits */
#define TCTLA_INT       0x80   /* underflow raises an interrupt   */
#define TCTLA_RESETDONE 0x40   /* write: clear TIMER_DONE          */
#define TCTLA_RELOAD    0x10   /* reload from backup on underflow  */
#define TCTLA_COUNT     0x08   /* counting enabled                 */
#define TCTLA_CLOCK     0x07   /* 0=1us,1=2us,..6=64us, 7=linked   */

/* control B bits */
#define TCTLB_DONE      0x08   /* timer has underflowed            */
#define TCTLB_BORROWOUT 0x01

/* Advance all timers by `us` microseconds; processes underflows, reloads,
 * cascades, and sets interrupt-latch bits. Returns the interrupt latch. */
uint8_t lynx_timer_step(uint32_t us);

/* Interrupt latch (bit i = timer i's pending interrupt). Backed by INTSET/
 * INTRST in Mikey space. */
extern uint8_t lynx_irq_latch;
uint8_t lynx_irq_pending(void);          /* nonzero if any interrupt pending */
void    lynx_irq_ack(uint8_t mask);      /* clear latch bits (INTRST write)  */
void    lynx_irq_raise(uint8_t mask);    /* set latch bits   (INTSET write)  */

void lynx_timer_init(void);

/* save-state: serialize the timer internals (phase accumulators + irq latch)
 * that aren't already in lynx_mikey.reg. Returns bytes written/read. */
size_t lynx_timer_state_size(void);
size_t lynx_timer_state_save(uint8_t *buf);
size_t lynx_timer_state_load(const uint8_t *buf);

#endif /* LYNXRECOMP_TIMER_H */
