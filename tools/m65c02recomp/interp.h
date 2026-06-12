/* interp.h - a reusable 65SC02 interpreter core.
 *
 * The CPU steps over a bus the embedding tool provides (bus_read/bus_write), so
 * the same interpreter drives lynxexec (boot-to-snapshot, with its own cart +
 * boot ROM memory) and lynxrun (run the game against the real runtime
 * peripherals). It also delivers IRQs through the bus vectors. */
#ifndef INTERP_H
#define INTERP_H

#include <stdint.h>

/* Provided by the embedding tool. */
uint8_t bus_read(uint16_t addr);
void    bus_write(uint16_t addr, uint8_t val);

typedef struct {
    uint8_t  a, x, y, s;
    uint16_t pc;
    int      n, v, d, i, z, c;
} interp_cpu_t;

extern interp_cpu_t icpu;

/* Events reported by the last interp_step(). */
enum { EV_NONE = 0, EV_INDJMP, EV_HALT };
extern int      interp_event;        /* one of EV_*                       */
extern uint16_t interp_event_addr;   /* target of an EV_INDJMP            */
extern long     interp_trace;        /* >0: trace this many insns to stderr */

void interp_reset_pc(uint16_t pc);   /* set sane reset regs, PC = pc       */
int  interp_step(void);              /* 0 ok, -1 unimplemented opcode       */
void interp_irq(void);               /* deliver a maskable IRQ via $FFFE    */
void interp_nmi(void);               /* deliver an NMI via $FFFA            */

#endif /* INTERP_H */
