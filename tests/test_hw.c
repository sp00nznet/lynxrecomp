/* test_hw.c - Mikey timers/IRQ + video readout (no game data). */
#include <stdio.h>
#include <string.h>
#include "lynxrecomp/mem.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_timer(void) {
    lynx_mikey_init();

    /* Timer 0: backup 3, clock 0 (1us/count), counting + reload + interrupt. */
    lynx_mikey.reg[0] = 3;                                  /* backup */
    lynx_mikey.reg[1] = TCTLA_COUNT | TCTLA_RELOAD | TCTLA_INT; /* ctlA, clk 0 */
    lynx_mikey.reg[2] = 3;                                  /* count */

    /* 3 us: count 3->2->1->0, no underflow yet */
    lynx_timer_step(3);
    CHECK(lynx_mikey.reg[2] == 0, "timer count after 3us = %u (exp 0)", lynx_mikey.reg[2]);
    CHECK(lynx_irq_latch == 0, "no IRQ yet");

    /* 1 more us: underflow -> reload to 3, TIMER_DONE, IRQ bit 0 */
    lynx_timer_step(1);
    CHECK(lynx_mikey.reg[2] == 3, "reloaded to backup, got %u", lynx_mikey.reg[2]);
    CHECK(lynx_mikey.reg[3] & TCTLB_DONE, "TIMER_DONE set");
    CHECK(lynx_irq_latch == 0x01, "IRQ latch bit0 set, got %02X", lynx_irq_latch);
    CHECK(lynx_irq_pending(), "irq pending");

    /* Acknowledge via INTRST write */
    lynx_mikey_write(MIKEY_INTRST, 0x01);
    CHECK(!lynx_irq_pending(), "irq cleared after INTRST");

    /* Clock divider: clock 2 = 4us/count. backup 1, counting only. */
    lynx_mikey_init();
    lynx_mikey.reg[0] = 1; lynx_mikey.reg[1] = TCTLA_COUNT | 2; lynx_mikey.reg[2] = 1;
    lynx_timer_step(4);
    CHECK(lynx_mikey.reg[2] == 0, "clk2: 4us -> 1 tick, count %u (exp 0)", lynx_mikey.reg[2]);

    /* Linked cascade: timer2 (VBL) is linked (clock 7) to timer0 (HBL) per the
     * Lynx chain 0->2; timer2 advances when timer0 underflows. */
    lynx_mikey_init();
    lynx_mikey.reg[0] = 0; lynx_mikey.reg[1] = TCTLA_COUNT | TCTLA_RELOAD; lynx_mikey.reg[2] = 0;   /* t0 underflows every us */
    lynx_mikey.reg[8] = 0; lynx_mikey.reg[9] = TCTLA_COUNT | TCTLA_RELOAD | 7; lynx_mikey.reg[10] = 2; /* t2 linked, count 2 */
    lynx_timer_step(2);   /* t0 underflows twice -> t2 ticks twice: 2->1->0 */
    CHECK(lynx_mikey.reg[10] == 0, "linked timer2 counted to %u (exp 0)", lynx_mikey.reg[10]);
}

static void test_video(void) {
    lynx_mem_init();
    lynx_mikey_init();

    /* palette index 1 = pure red (R=0xF), index 2 = pure green, 3 = pure blue */
    lynx_mikey.reg[MIKEY_PALGREEN + 1] = 0x00; lynx_mikey.reg[MIKEY_PALBLURD + 1] = 0x0F; /* R */
    lynx_mikey.reg[MIKEY_PALGREEN + 2] = 0x0F; lynx_mikey.reg[MIKEY_PALBLURD + 2] = 0x00; /* G */
    lynx_mikey.reg[MIKEY_PALGREEN + 3] = 0x00; lynx_mikey.reg[MIKEY_PALBLURD + 3] = 0xF0; /* B */
    CHECK(lynx_palette_color(1) == 0xFFFF0000u, "palette red = %08X", lynx_palette_color(1));
    CHECK(lynx_palette_color(2) == 0xFF00FF00u, "palette green = %08X", lynx_palette_color(2));
    CHECK(lynx_palette_color(3) == 0xFF0000FFu, "palette blue = %08X", lynx_palette_color(3));

    /* framebuffer at $A000; first byte = pens 1 (left) and 2 (right) */
    lynx_mikey.reg[MIKEY_DISPADRL] = 0x00;
    lynx_mikey.reg[MIKEY_DISPADRH] = 0xA0;
    lynx_mem_write(0xA000, 0x12);     /* high nibble=1 (red), low nibble=2 (green) */

    static uint32_t fb[LYNX_SCREEN_W * LYNX_SCREEN_H];
    lynx_video_render(fb);
    CHECK(fb[0] == 0xFFFF0000u, "pixel(0,0) red, got %08X", fb[0]);
    CHECK(fb[1] == 0xFF00FF00u, "pixel(1,0) green, got %08X", fb[1]);
}

int main(void) {
    test_timer();
    test_video();
    if (fails == 0) printf("PASS: timer + video hardware checks\n");
    else            printf("FAIL: %d check(s)\n", fails);
    return fails ? 1 : 0;
}
