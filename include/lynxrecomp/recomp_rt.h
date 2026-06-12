/* recomp_rt.h - 65SC02 semantic helpers for recompiled code.
 *
 * Generated functions stay readable by calling these named helpers instead of
 * inlining flag math everywhere (the vbrecomp approach). Each helper updates
 * lynx_cpu (registers + N/V/Z/C, and D/I where relevant) exactly as the CPU
 * would. Memory is reached through lynx_mem_read/write; the emitter computes
 * effective addresses inline and passes values/targets here.
 *
 * Naming: lynx_<mnemonic> for the common ops. Read-modify-write and shift ops
 * that apply to either A or memory take/return a value (lynx_alu_*).
 */
#ifndef LYNXRECOMP_RECOMP_RT_H
#define LYNXRECOMP_RECOMP_RT_H

#include <stdint.h>
#include "cpu.h"
#include "mem.h"

/* --- flag helper --- */
static inline void lynx_set_nz(uint8_t v) {
    lynx_cpu.z = (v == 0);
    lynx_cpu.n = (v >> 7) & 1;
}

/* Zero-page indirect pointer: read a 16-bit LE pointer from zero page, with
 * the 6502 zero-page wrap on the high byte (used by (zp), (zp,X), (zp),Y). */
static inline uint16_t lynx_zp_ptr(uint8_t zp) {
    return (uint16_t)(lynx_mem_read(zp) | (lynx_mem_read((uint8_t)(zp + 1)) << 8));
}

/* --- control-transfer hooks the emitter targets when control leaves the
 * recompiled image (boot ROM calls, computed/indirect jumps, unimplemented
 * opcodes). Defaults live in recomp_rt.c; a host may override behavior by
 * setting these function pointers. --- */
extern void (*lynx_hook_boot_call)(uint16_t addr);   /* JSR into boot ROM      */
extern void (*lynx_hook_ext_jmp)(uint16_t addr);     /* JMP out of image       */
extern void (*lynx_hook_jmp_indirect)(uint16_t addr);/* JMP (ptr) computed dest */
extern void (*lynx_hook_unimpl)(uint8_t opcode);     /* opcode not yet emitted  */

void lynx_boot_call(uint16_t addr);
void lynx_ext_jmp(uint16_t addr);
void lynx_jmp_indirect(uint16_t addr);
void lynx_unimpl(uint8_t opcode);

/* --- loads --- */
static inline void lynx_lda(uint8_t v) { lynx_cpu.a = v; lynx_set_nz(v); }
static inline void lynx_ldx(uint8_t v) { lynx_cpu.x = v; lynx_set_nz(v); }
static inline void lynx_ldy(uint8_t v) { lynx_cpu.y = v; lynx_set_nz(v); }

/* --- logical (on A) --- */
static inline void lynx_ora(uint8_t v) { lynx_cpu.a |= v; lynx_set_nz(lynx_cpu.a); }
static inline void lynx_and(uint8_t v) { lynx_cpu.a &= v; lynx_set_nz(lynx_cpu.a); }
static inline void lynx_eor(uint8_t v) { lynx_cpu.a ^= v; lynx_set_nz(lynx_cpu.a); }

/* --- BIT --- */
static inline void lynx_bit(uint8_t v) {
    lynx_cpu.z = ((lynx_cpu.a & v) == 0);
    lynx_cpu.n = (v >> 7) & 1;
    lynx_cpu.v = (v >> 6) & 1;
}
/* BIT #imm (65C02): only Z is affected. */
static inline void lynx_bit_imm(uint8_t v) {
    lynx_cpu.z = ((lynx_cpu.a & v) == 0);
}

/* --- ADC/SBC (binary + 65C02 decimal) --- */
static inline void lynx_adc(uint8_t v) {
    unsigned c = lynx_cpu.c;
    if (!lynx_cpu.d) {
        unsigned sum = lynx_cpu.a + v + c;
        lynx_cpu.v = ((~(lynx_cpu.a ^ v) & (lynx_cpu.a ^ sum)) >> 7) & 1;
        lynx_cpu.c = (sum > 0xFF);
        lynx_cpu.a = (uint8_t)sum;
        lynx_set_nz(lynx_cpu.a);
    } else { /* 65C02 decimal: N/Z/V reflect the corrected result */
        unsigned lo = (lynx_cpu.a & 0x0F) + (v & 0x0F) + c;
        unsigned hi = (lynx_cpu.a >> 4) + (v >> 4);
        if (lo > 9) { lo += 6; hi += 1; }
        unsigned bin = lynx_cpu.a + v + c;
        lynx_cpu.v = ((~(lynx_cpu.a ^ v) & (lynx_cpu.a ^ bin)) >> 7) & 1;
        if (hi > 9) hi += 6;
        lynx_cpu.c = (hi > 0x0F);
        lynx_cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
        lynx_set_nz(lynx_cpu.a);
    }
}
static inline void lynx_sbc(uint8_t v) {
    unsigned c = lynx_cpu.c;
    if (!lynx_cpu.d) {
        unsigned diff = lynx_cpu.a - v - (1 - c);
        lynx_cpu.v = (((lynx_cpu.a ^ v) & (lynx_cpu.a ^ diff)) >> 7) & 1;
        lynx_cpu.c = (diff < 0x100);
        lynx_cpu.a = (uint8_t)diff;
        lynx_set_nz(lynx_cpu.a);
    } else {
        int lo = (lynx_cpu.a & 0x0F) - (v & 0x0F) - (1 - c);
        int hi = (lynx_cpu.a >> 4) - (v >> 4);
        if (lo < 0) { lo -= 6; hi -= 1; }
        if (hi < 0) hi -= 6;
        unsigned diff = lynx_cpu.a - v - (1 - c);
        lynx_cpu.v = (((lynx_cpu.a ^ v) & (lynx_cpu.a ^ diff)) >> 7) & 1;
        lynx_cpu.c = (diff < 0x100);
        lynx_cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
        lynx_set_nz(lynx_cpu.a);
    }
}

/* --- compares --- */
static inline void lynx_cmp_reg(uint8_t reg, uint8_t v) {
    uint8_t r = (uint8_t)(reg - v);
    lynx_cpu.c = (reg >= v);
    lynx_set_nz(r);
}
static inline void lynx_cmp(uint8_t v) { lynx_cmp_reg(lynx_cpu.a, v); }
static inline void lynx_cpx(uint8_t v) { lynx_cmp_reg(lynx_cpu.x, v); }
static inline void lynx_cpy(uint8_t v) { lynx_cmp_reg(lynx_cpu.y, v); }

/* --- inc/dec (registers) --- */
static inline void lynx_inx(void) { lynx_cpu.x++; lynx_set_nz(lynx_cpu.x); }
static inline void lynx_iny(void) { lynx_cpu.y++; lynx_set_nz(lynx_cpu.y); }
static inline void lynx_dex(void) { lynx_cpu.x--; lynx_set_nz(lynx_cpu.x); }
static inline void lynx_dey(void) { lynx_cpu.y--; lynx_set_nz(lynx_cpu.y); }
static inline void lynx_inc_a(void) { lynx_cpu.a++; lynx_set_nz(lynx_cpu.a); }
static inline void lynx_dec_a(void) { lynx_cpu.a--; lynx_set_nz(lynx_cpu.a); }

/* --- inc/dec & shifts on a value (A or memory) --- */
static inline uint8_t lynx_alu_inc(uint8_t v) { v++; lynx_set_nz(v); return v; }
static inline uint8_t lynx_alu_dec(uint8_t v) { v--; lynx_set_nz(v); return v; }
static inline uint8_t lynx_alu_asl(uint8_t v) { lynx_cpu.c = (v >> 7) & 1; v = (uint8_t)(v << 1); lynx_set_nz(v); return v; }
static inline uint8_t lynx_alu_lsr(uint8_t v) { lynx_cpu.c = v & 1; v = v >> 1; lynx_set_nz(v); return v; }
static inline uint8_t lynx_alu_rol(uint8_t v) { unsigned nc = (v >> 7) & 1; v = (uint8_t)((v << 1) | lynx_cpu.c); lynx_cpu.c = nc; lynx_set_nz(v); return v; }
static inline uint8_t lynx_alu_ror(uint8_t v) { unsigned nc = v & 1; v = (uint8_t)((v >> 1) | (lynx_cpu.c << 7)); lynx_cpu.c = nc; lynx_set_nz(v); return v; }

/* --- TRB/TSB --- */
static inline uint8_t lynx_alu_tsb(uint8_t v) { lynx_cpu.z = ((lynx_cpu.a & v) == 0); return (uint8_t)(v | lynx_cpu.a); }
static inline uint8_t lynx_alu_trb(uint8_t v) { lynx_cpu.z = ((lynx_cpu.a & v) == 0); return (uint8_t)(v & ~lynx_cpu.a); }

/* --- transfers --- */
static inline void lynx_tax(void) { lynx_cpu.x = lynx_cpu.a; lynx_set_nz(lynx_cpu.x); }
static inline void lynx_tay(void) { lynx_cpu.y = lynx_cpu.a; lynx_set_nz(lynx_cpu.y); }
static inline void lynx_txa(void) { lynx_cpu.a = lynx_cpu.x; lynx_set_nz(lynx_cpu.a); }
static inline void lynx_tya(void) { lynx_cpu.a = lynx_cpu.y; lynx_set_nz(lynx_cpu.a); }
static inline void lynx_tsx(void) { lynx_cpu.x = lynx_cpu.s; lynx_set_nz(lynx_cpu.x); }
static inline void lynx_txs(void) { lynx_cpu.s = lynx_cpu.x; } /* no flags */

/* --- stack --- */
static inline void    lynx_push(uint8_t v) { lynx_mem_write(0x0100 + lynx_cpu.s, v); lynx_cpu.s--; }
static inline uint8_t lynx_pull(void)      { lynx_cpu.s++; return lynx_mem_read(0x0100 + lynx_cpu.s); }
static inline void lynx_pha(void) { lynx_push(lynx_cpu.a); }
static inline void lynx_phx(void) { lynx_push(lynx_cpu.x); }
static inline void lynx_phy(void) { lynx_push(lynx_cpu.y); }
static inline void lynx_php(void) { lynx_push(lynx_p_pack()); }
static inline void lynx_pla(void) { lynx_cpu.a = lynx_pull(); lynx_set_nz(lynx_cpu.a); }
static inline void lynx_plx(void) { lynx_cpu.x = lynx_pull(); lynx_set_nz(lynx_cpu.x); }
static inline void lynx_ply(void) { lynx_cpu.y = lynx_pull(); lynx_set_nz(lynx_cpu.y); }
static inline void lynx_plp(void) { lynx_p_unpack(lynx_pull()); }

#endif /* LYNXRECOMP_RECOMP_RT_H */
