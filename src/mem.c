/* mem.c - Lynx address-space dispatch. See mem.h.
 *
 * MAPCTL ($FFF9) selects, per top-region, whether the CPU sees hardware or the
 * DRAM underneath. Phase 1 implements the common case (hardware mapped in for
 * Suzy/Mikey, DRAM elsewhere) which is what games run with; the per-bit MAPCTL
 * shadowing nuance is a documented later refinement. */
#include "lynxrecomp/mem.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"

uint8_t lynx_ram[LYNX_RAM_SIZE];

void lynx_mem_init(void) {
    for (unsigned i = 0; i < LYNX_RAM_SIZE; i++) lynx_ram[i] = 0;
    lynx_ram[LYNX_MAPCTL_ADDR] = 0x00;   /* default: hardware mapped in */
}

uint8_t lynx_mem_read(uint16_t addr) {
    if (addr >= LYNX_SUZY_BASE && addr < LYNX_MIKEY_BASE)
        return lynx_suzy_read((uint8_t)(addr - LYNX_SUZY_BASE));
    if (addr >= LYNX_MIKEY_BASE && addr < LYNX_BOOTROM_BASE)
        return lynx_mikey_read((uint8_t)(addr - LYNX_MIKEY_BASE));
    return lynx_ram[addr];
}

void lynx_mem_write(uint16_t addr, uint8_t val) {
    if (addr >= LYNX_SUZY_BASE && addr < LYNX_MIKEY_BASE) {
        lynx_suzy_write((uint8_t)(addr - LYNX_SUZY_BASE), val);
        return;
    }
    if (addr >= LYNX_MIKEY_BASE && addr < LYNX_BOOTROM_BASE) {
        lynx_mikey_write((uint8_t)(addr - LYNX_MIKEY_BASE), val);
        return;
    }
    lynx_ram[addr] = val;
}

uint16_t lynx_mem_read16(uint16_t addr) {
    return (uint16_t)(lynx_mem_read(addr) |
                      (lynx_mem_read((uint16_t)(addr + 1)) << 8));
}
