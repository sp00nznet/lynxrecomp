/* cpu.h - 65SC02 CPU state for recompiled Lynx code.
 *
 * Recompiled routines manipulate this state directly (registers, flags, the
 * stack page) and call mem_read/mem_write for memory access. The model is the
 * standard 6502 programmer's model; the Lynx variant is a CMOS 65SC02 with no
 * decimal-mode ALU quirks of the NMOS part. */
#ifndef LYNXRECOMP_CPU_H
#define LYNXRECOMP_CPU_H

#include <stdint.h>

typedef struct {
    uint8_t  a, x, y;   /* accumulator, index X, index Y     */
    uint8_t  s;         /* stack pointer (page 0x01)         */
    uint16_t pc;        /* program counter                   */
    /* Processor status flags, stored unpacked for cheap access. */
    uint8_t  n, v, d, i, z, c;  /* sign, overflow, decimal, irq-disable, zero, carry */
} lynx_cpu_t;

extern lynx_cpu_t lynx_cpu;

/* Pack/unpack P (used by PHP/PLP/BRK/RTI and recompiled flag ops). */
uint8_t lynx_p_pack(void);
void    lynx_p_unpack(uint8_t p);

void lynx_cpu_reset(void);

#endif /* LYNXRECOMP_CPU_H */
