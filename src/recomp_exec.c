/* recomp_exec.c - the recompiled-game execution model. See recomp_rt.h.
 *
 * The recompiled code is straight-line C: JSR is a C call, RTS/JMP are returns/
 * gotos. The 6502 stack is only used for PHA/PLA-style saves and for interrupt
 * entry/return. Because the main loop never returns to the host, the runtime
 * drives time and interrupts from inside the recompiled code via lynx_tick(),
 * which the emitter plants at loop back-edges. */
#include "lynxrecomp/recomp_rt.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/audio.h"

void (*lynx_frame_hook)(void) = 0;

/* Deliver a maskable IRQ to the recompiled handler. Mirror the CPU's IRQ entry:
 * push PCH, PCL, P (B clear) so the handler's "BRK vs IRQ" check sees a clean
 * status, mask further IRQs, then call the function at the $FFFE vector. */
void lynx_irq_deliver(void) {
    uint16_t vec = lynx_mem_read16(LYNX_VEC_IRQ);   /* game's RAM IRQ handler */
    /* PC is not maintained in recompiled code; the value is discarded by RTI. */
    lynx_push(0x00);                                /* PCH */
    lynx_push(0x00);                                /* PCL */
    lynx_push((uint8_t)(lynx_p_pack() & ~0x10));    /* P, B clear (hardware IRQ) */
    lynx_cpu.i = 1;
    lynx_cpu.d = 0;
    lynx_call_addr(vec);
}

/* RTI in recompiled code: pop the P/PCL/PCH the delivery pushed and restore the
 * flags. (Control returns via the C `return` the emitter places after this.) */
void lynx_rti(void) {
    lynx_p_unpack(lynx_pull());                     /* P */
    (void)lynx_pull();                              /* PCL (discarded) */
    (void)lynx_pull();                              /* PCH (discarded) */
}

/* Loop back-edge: advance emulated time, drive audio, and service interrupts. */
void lynx_tick(void) {
    lynx_timer_step(LYNX_TICK_US);
    lynx_audio_step(LYNX_TICK_US);
    if (lynx_irq_pending() && !lynx_cpu.i)
        lynx_irq_deliver();
}
