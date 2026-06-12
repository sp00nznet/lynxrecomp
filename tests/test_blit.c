/* test_blit.c - Suzy sprite blitter (no game data; synthetic SCB + sprite). */
#include <stdio.h>
#include "lynxrecomp/mem.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void poke(uint16_t a, const uint8_t *b, int n) {
    for (int i = 0; i < n; i++) lynx_mem_write((uint16_t)(a + i), b[i]);
}
/* read a screen pixel's 4-bit pen from the video buffer */
static uint8_t pix(uint16_t vidbas, int x, int y) {
    uint8_t byte = lynx_mem_read((uint16_t)(vidbas + y * LYNX_SCREEN_PITCH + (x >> 1)));
    return (x & 1) ? (byte & 0x0F) : (byte >> 4);
}

int main(void) {
    lynx_mem_init();
    lynx_suzy_init();

    const uint16_t VID = 0xC000;
    /* Suzy: video buffer base, screen origin, first SCB */
    lynx_suzy.reg[SUZY_VIDBASL] = VID & 0xFF; lynx_suzy.reg[SUZY_VIDBASH] = VID >> 8;
    lynx_suzy.reg[SUZY_HOFFL] = 0; lynx_suzy.reg[SUZY_HOFFH] = 0;
    lynx_suzy.reg[SUZY_VOFFL] = 0; lynx_suzy.reg[SUZY_VOFFH] = 0;

    /* ---- literal sprite: 4x4 block of pen-index 1 -> colour 5, at (10,8) ---- */
    uint8_t scb1[] = {
        0xC0,             /* SPRCTL0: 4bpp (bits7-6=11), type/flip 0      */
        0x80,             /* SPRCTL1: literal, reload none, load palette  */
        0x00,             /* SPRCOLL                                      */
        0x00, 0x00,       /* next = 0 (end of chain)                      */
        0x20, 0x04,       /* data = $0420                                 */
        10, 0,            /* hpos = 10                                    */
        8, 0,             /* vpos = 8                                     */
        0x05, 0,0,0,0,0,0,0 /* pen palette: pen[1]=5, rest 0 (8 bytes)    */
    };
    uint8_t spr1[] = {
        0x03, 0x11, 0x11, /* line: offset 3, four 4-bit pens = 1,1,1,1    */
        0x03, 0x11, 0x11,
        0x03, 0x11, 0x11,
        0x03, 0x11, 0x11,
        0x00              /* end of sprite                                */
    };
    poke(0x0400, scb1, sizeof(scb1));
    poke(0x0420, spr1, sizeof(spr1));
    lynx_suzy.reg[SUZY_SCBNEXTL] = 0x00; lynx_suzy.reg[SUZY_SCBNEXTH] = 0x04;

    lynx_suzy_write(SUZY_SPRGO, 0x01);    /* go */

    for (int y = 8; y < 12; y++)
        for (int x = 10; x < 14; x++)
            CHECK(pix(VID, x, y) == 5, "literal pixel (%d,%d)=%d exp 5", x, y, pix(VID, x, y));
    CHECK(pix(VID, 9, 8) == 0 && pix(VID, 14, 8) == 0, "literal edges clear");

    /* ---- packed sprite: 8-px run of pen-index 1 -> colour 7, at (20,20) ----
     * line bits: [0][0111][0001]=9 (run of 8), then [0][0000]=5 (line end). */
    lynx_mem_init();
    lynx_suzy.reg[SUZY_VIDBASL] = VID & 0xFF; lynx_suzy.reg[SUZY_VIDBASH] = VID >> 8;
    uint8_t scb2[] = {
        0xC0, 0x00 /* not literal -> packed */, 0x00,
        0x00, 0x00,
        0x20, 0x04,
        20, 0,
        20, 0,
        0x07, 0,0,0,0,0,0,0   /* pen[1]=7 */
    };
    uint8_t spr2[] = { 0x03, 0x38, 0x80, 0x00 };  /* one line + end */
    poke(0x0400, scb2, sizeof(scb2));
    poke(0x0420, spr2, sizeof(spr2));
    lynx_suzy.reg[SUZY_SCBNEXTL] = 0x00; lynx_suzy.reg[SUZY_SCBNEXTH] = 0x04;
    lynx_suzy_write(SUZY_SPRGO, 0x01);

    for (int x = 20; x < 28; x++)
        CHECK(pix(VID, x, 20) == 7, "packed pixel (%d,20)=%d exp 7", x, pix(VID, x, 20));
    CHECK(pix(VID, 28, 20) == 0, "packed run ends at 8 px");

    /* visual artifact: render both onto one frame as a PPM */
    lynx_mem_init();
    lynx_suzy.reg[SUZY_VIDBASL] = VID & 0xFF; lynx_suzy.reg[SUZY_VIDBASH] = VID >> 8;
    lynx_mikey.reg[MIKEY_DISPADRL] = VID & 0xFF; lynx_mikey.reg[MIKEY_DISPADRH] = VID >> 8;
    lynx_mikey.reg[MIKEY_PALGREEN + 5] = 0x00; lynx_mikey.reg[MIKEY_PALBLURD + 5] = 0x0F; /* 5=red */
    lynx_mikey.reg[MIKEY_PALGREEN + 7] = 0x0F; lynx_mikey.reg[MIKEY_PALBLURD + 7] = 0x00; /* 7=green */
    poke(0x0400, scb1, sizeof(scb1)); poke(0x0420, spr1, sizeof(spr1));
    lynx_suzy.reg[SUZY_SCBNEXTL] = 0x00; lynx_suzy.reg[SUZY_SCBNEXTH] = 0x04;
    lynx_suzy_write(SUZY_SPRGO, 0x01);
    poke(0x0400, scb2, sizeof(scb2)); poke(0x0420, spr2, sizeof(spr2));
    lynx_suzy.reg[SUZY_SCBNEXTL] = 0x00; lynx_suzy.reg[SUZY_SCBNEXTH] = 0x04;
    lynx_suzy_write(SUZY_SPRGO, 0x01);
    if (lynx_video_write_ppm("blit_test.ppm") == 0) printf("  wrote blit_test.ppm\n");

    if (fails == 0) printf("PASS: blitter (literal + packed) checks\n");
    else            printf("FAIL: %d check(s)\n", fails);
    return fails ? 1 : 0;
}
