/* cpu.c - 65SC02 state + flag pack/unpack. See cpu.h. */
#include "lynxrecomp/cpu.h"
#include "lynxrecomp/mem.h"

lynx_cpu_t lynx_cpu;

/* P bit layout: N V 1 B D I Z C. */
uint8_t lynx_p_pack(void) {
    return (uint8_t)((lynx_cpu.n ? 0x80 : 0) |
                     (lynx_cpu.v ? 0x40 : 0) |
                     0x20 |                       /* unused, always 1 */
                     0x10 |                       /* B - set when pushed by PHP/BRK */
                     (lynx_cpu.d ? 0x08 : 0) |
                     (lynx_cpu.i ? 0x04 : 0) |
                     (lynx_cpu.z ? 0x02 : 0) |
                     (lynx_cpu.c ? 0x01 : 0));
}

void lynx_p_unpack(uint8_t p) {
    lynx_cpu.n = (p & 0x80) != 0;
    lynx_cpu.v = (p & 0x40) != 0;
    lynx_cpu.d = (p & 0x08) != 0;
    lynx_cpu.i = (p & 0x04) != 0;
    lynx_cpu.z = (p & 0x02) != 0;
    lynx_cpu.c = (p & 0x01) != 0;
}

void lynx_cpu_reset(void) {
    lynx_cpu.a = lynx_cpu.x = lynx_cpu.y = 0;
    lynx_cpu.s = 0xFF;
    lynx_cpu.n = lynx_cpu.v = lynx_cpu.d = lynx_cpu.z = lynx_cpu.c = 0;
    lynx_cpu.i = 1;                 /* IRQs masked at reset */
    lynx_cpu.pc = lynx_mem_read16(LYNX_VEC_RESET);
}
