/* test_units.c - pure unit tests: decoder + runtime ALU/flag helpers. */
#include <stdio.h>
#include <string.h>
#include "decode.h"
#include "lynxrecomp/recomp_rt.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_decode(void) {
    insn_t in;

    uint8_t a[] = {0xA9, 0x01};                 /* LDA #$01 */
    m65c02_decode(a, 0x0200, &in);
    CHECK(!strcmp(in.mnemonic,"LDA") && in.mode==AM_IMM && in.len==2 && in.operand==0x01, "LDA #imm");

    uint8_t b[] = {0x20, 0x50, 0x03};           /* JSR $0350 */
    m65c02_decode(b, 0x0200, &in);
    CHECK(!strcmp(in.mnemonic,"JSR") && in.mode==AM_ABS && in.cflow==CF_CALL && in.target==0x0350, "JSR abs");

    uint8_t c[] = {0xF0, 0x03};                 /* BEQ +3 from $020C -> $0211 */
    m65c02_decode(c, 0x020C, &in);
    CHECK(!strcmp(in.mnemonic,"BEQ") && in.mode==AM_REL && in.cflow==CF_BRANCH && in.target==0x0211, "BEQ rel");

    uint8_t d[] = {0x0F, 0x30, 0x05};           /* BBR0 $30,+5 from $0216 -> $021E */
    m65c02_decode(d, 0x0216, &in);
    CHECK(!strcmp(in.mnemonic,"BBR0") && in.mode==AM_ZPREL && in.len==3 && in.bit==0 && in.operand==0x30 && in.target==0x021E, "BBR0 zp,rel");

    uint8_t e[] = {0x7C, 0x00, 0x04};           /* JMP ($0400,X) */
    m65c02_decode(e, 0x0200, &in);
    CHECK(!strcmp(in.mnemonic,"JMP") && in.mode==AM_IAX && in.cflow==CF_JMP, "JMP (abs,X)");

    uint8_t f[] = {0x64, 0x21};                 /* STZ $21 (CMOS) */
    m65c02_decode(f, 0x0200, &in);
    CHECK(!strcmp(in.mnemonic,"STZ") && in.mode==AM_ZP, "STZ zp");
}

static void test_alu(void) {
    memset(&lynx_cpu, 0, sizeof(lynx_cpu));

    /* ADC: 0x50 + 0x50 = 0xA0, signed overflow, negative, no carry */
    lynx_cpu.a = 0x50; lynx_cpu.c = 0; lynx_cpu.d = 0;
    lynx_adc(0x50);
    CHECK(lynx_cpu.a==0xA0 && lynx_cpu.v==1 && lynx_cpu.n==1 && lynx_cpu.c==0 && lynx_cpu.z==0, "ADC 50+50");

    /* ADC with carry out: 0xFF + 0x01 = 0x00, carry, zero */
    lynx_cpu.a = 0xFF; lynx_cpu.c = 0;
    lynx_adc(0x01);
    CHECK(lynx_cpu.a==0x00 && lynx_cpu.c==1 && lynx_cpu.z==1, "ADC FF+01");

    /* SBC: 0x50 - 0x10 (carry set = no borrow) = 0x40, carry stays */
    lynx_cpu.a = 0x50; lynx_cpu.c = 1;
    lynx_sbc(0x10);
    CHECK(lynx_cpu.a==0x40 && lynx_cpu.c==1, "SBC 50-10");

    /* SBC borrow: 0x10 - 0x20 = 0xF0, carry clear (borrow), negative */
    lynx_cpu.a = 0x10; lynx_cpu.c = 1;
    lynx_sbc(0x20);
    CHECK(lynx_cpu.a==0xF0 && lynx_cpu.c==0 && lynx_cpu.n==1, "SBC 10-20");

    /* CMP equal: Z and C set */
    lynx_cpu.a = 0x42;
    lynx_cmp(0x42);
    CHECK(lynx_cpu.z==1 && lynx_cpu.c==1, "CMP equal");

    /* shifts */
    CHECK(lynx_alu_asl(0x80)==0x00 && lynx_cpu.c==1 && lynx_cpu.z==1, "ASL 80");
    CHECK(lynx_alu_lsr(0x01)==0x00 && lynx_cpu.c==1 && lynx_cpu.z==1, "LSR 01");
    lynx_cpu.c = 1;
    CHECK(lynx_alu_rol(0x80)==0x01 && lynx_cpu.c==1, "ROL 80 (c in)");
    lynx_cpu.c = 1;
    CHECK(lynx_alu_ror(0x01)==0x80 && lynx_cpu.c==1, "ROR 01 (c in)");

    /* BCD ADC: 0x09 + 0x01 in decimal = 0x10 */
    memset(&lynx_cpu, 0, sizeof(lynx_cpu));
    lynx_cpu.a = 0x09; lynx_cpu.d = 1; lynx_cpu.c = 0;
    lynx_adc(0x01);
    CHECK(lynx_cpu.a==0x10 && lynx_cpu.c==0, "ADC BCD 09+01");
}

int main(void) {
    test_decode();
    test_alu();
    if (fails == 0) printf("PASS: all unit checks\n");
    else            printf("FAIL: %d check(s)\n", fails);
    return fails ? 1 : 0;
}
