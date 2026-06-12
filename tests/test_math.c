/* test_math.c - Suzy math unit (multiply + divide). No game data. */
#include <stdio.h>
#include "lynxrecomp/suzy.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint32_t rd32(uint8_t lo) {
    return (uint32_t)lynx_suzy.reg[lo] | ((uint32_t)lynx_suzy.reg[lo+1] << 8)
         | ((uint32_t)lynx_suzy.reg[lo+2] << 16) | ((uint32_t)lynx_suzy.reg[lo+3] << 24);
}

int main(void) {
    lynx_suzy_init();

    /* multiply: AB=0x1234, CD=0x0010 -> 0x00012340 in EFGH */
    lynx_suzy_write(SUZY_MATHD, 0x10);  /* CD low  */
    lynx_suzy_write(SUZY_MATHC, 0x00);  /* CD high */
    lynx_suzy_write(SUZY_MATHB, 0x34);  /* AB low  */
    lynx_suzy_write(SUZY_MATHA, 0x12);  /* AB high -> trigger */
    CHECK(rd32(SUZY_MATHH) == 0x00012340u, "mul 0x1234*0x10 = %08X exp 00012340", rd32(SUZY_MATHH));

    /* divide: 1000 / 7 = 142 r 6 */
    lynx_suzy_write(SUZY_MATHP, 7);     /* divisor low  */
    lynx_suzy_write(SUZY_MATHN, 0);     /* divisor high */
    lynx_suzy_write(SUZY_MATHH + 0, 0xE8); /* dividend EFGH = 0x000003E8 = 1000 */
    lynx_suzy_write(SUZY_MATHH + 1, 0x03);
    lynx_suzy_write(SUZY_MATHH + 2, 0x00);
    lynx_suzy_write(SUZY_MATHE, 0x00);  /* high byte + trigger */
    CHECK(rd32(SUZY_MATHD) == 142, "div quotient = %u exp 142", rd32(SUZY_MATHD));
    CHECK(rd32(SUZY_MATHM) == 6,   "div remainder = %u exp 6", rd32(SUZY_MATHM));

    /* divide by zero -> quotient all ones */
    lynx_suzy_write(SUZY_MATHP, 0); lynx_suzy_write(SUZY_MATHN, 0);
    lynx_suzy_write(SUZY_MATHE, 0x00);
    CHECK(rd32(SUZY_MATHD) == 0xFFFFFFFFu, "div by zero quotient = %08X", rd32(SUZY_MATHD));

    if (fails == 0) printf("PASS: Suzy math (multiply + divide) checks\n");
    else            printf("FAIL: %d check(s)\n", fails);
    return fails ? 1 : 0;
}
