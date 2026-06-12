/* mem.h - Atari Lynx address space + memory access for recompiled code.
 *
 * The 65SC02 sees a flat 64 KiB space. The Lynx overlays hardware at the top,
 * gated by the MAPCTL register ($FFF9): when a region is "vectored to hardware"
 * the CPU reads/writes Suzy/Mikey registers there; otherwise it sees the 64 KiB
 * of DRAM underneath (including the boot-ROM shadow). All recompiled loads and
 * stores funnel through lynx_mem_read/write so this dispatch lives in one place.
 *
 *   $0000-$00FF  zero page (fast addressing)
 *   $0100-$01FF  CPU stack
 *   $0200-$FBFF  general DRAM (game code+data after boot copies it here)
 *   $FC00-$FCFF  Suzy   hardware (sprite engine, math unit) - when mapped
 *   $FD00-$FDFF  Mikey  hardware (timers, audio, video DMA, UART) - when mapped
 *   $FE00-$FFF7  boot ROM (512 B) - when mapped
 *   $FFF8        reserved
 *   $FFF9        MAPCTL - selects hardware vs DRAM for the top regions
 *   $FFFA-$FFFF  CPU vectors (NMI/RESET/IRQ) - when boot ROM mapped
 */
#ifndef LYNXRECOMP_MEM_H
#define LYNXRECOMP_MEM_H

#include <stdint.h>

#define LYNX_RAM_SIZE      0x10000u

#define LYNX_SUZY_BASE     0xFC00u
#define LYNX_MIKEY_BASE    0xFD00u
#define LYNX_BOOTROM_BASE  0xFE00u
#define LYNX_MAPCTL_ADDR   0xFFF9u

#define LYNX_VEC_NMI       0xFFFAu
#define LYNX_VEC_RESET     0xFFFCu
#define LYNX_VEC_IRQ       0xFFFEu

extern uint8_t lynx_ram[LYNX_RAM_SIZE];

uint8_t lynx_mem_read(uint16_t addr);
void    lynx_mem_write(uint16_t addr, uint8_t val);

/* 16-bit little-endian helpers used by indirect addressing. */
uint16_t lynx_mem_read16(uint16_t addr);

void lynx_mem_init(void);

#endif /* LYNXRECOMP_MEM_H */
