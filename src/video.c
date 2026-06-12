/* video.c - Mikey video readout: framebuffer + palette -> RGBA.
 *
 * The Lynx display is 160x102 at 4 bits/pixel, 2 pixels per byte with the LEFT
 * pixel in the high nibble. The framebuffer lives in RAM at DISPADR
 * ($FD94/95). Colour comes from the 16-entry palette: the green nibble is in
 * PALGREEN ($FDA0+i, low nibble) and blue/red are in PALBLUERED ($FDB0+i, blue
 * = high nibble, red = low nibble). Each 4-bit component scales to 8 bits via
 * v*17 (0x0->0x00 .. 0xF->0xFF). */
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/mem.h"
#include <stdio.h>

/* The video DMA reads the framebuffer straight from DRAM (like Suzy), bypassing
 * the MAPCTL hardware overlay - so a buffer at e.g. $E000 reads RAM, not the
 * $FC00+ registers. Hence lynx_ram[] rather than lynx_mem_read(). */

static uint8_t exp4(uint8_t v) { return (uint8_t)((v & 0x0F) * 17); }

uint32_t lynx_palette_color(int index) {
    index &= 0x0F;
    uint8_t green  = lynx_mikey.reg[MIKEY_PALGREEN + index];
    uint8_t bluered = lynx_mikey.reg[MIKEY_PALBLURD + index];
    uint8_t r = exp4((uint8_t)(bluered & 0x0F));
    uint8_t g = exp4((uint8_t)(green   & 0x0F));
    uint8_t b = exp4((uint8_t)(bluered >> 4));
    return (uint32_t)0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void lynx_video_render(uint32_t *out) {
    uint16_t base = (uint16_t)(lynx_mikey.reg[MIKEY_DISPADRL] |
                               (lynx_mikey.reg[MIKEY_DISPADRH] << 8));
    for (int row = 0; row < LYNX_SCREEN_H; row++) {
        uint16_t line = (uint16_t)(base + row * LYNX_SCREEN_PITCH);
        for (int col = 0; col < LYNX_SCREEN_W; col++) {
            uint8_t byte = lynx_ram[(uint16_t)(line + (col >> 1))];
            uint8_t pen  = (col & 1) ? (uint8_t)(byte & 0x0F) : (uint8_t)(byte >> 4);
            out[row * LYNX_SCREEN_W + col] = lynx_palette_color(pen);
        }
    }
}

int lynx_video_write_ppm(const char *path) {
    static uint32_t fb[LYNX_SCREEN_W * LYNX_SCREEN_H];
    lynx_video_render(fb);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", LYNX_SCREEN_W, LYNX_SCREEN_H);
    for (int i = 0; i < LYNX_SCREEN_W * LYNX_SCREEN_H; i++) {
        uint32_t c = fb[i];
        unsigned char rgb[3] = { (unsigned char)(c >> 16),
                                 (unsigned char)(c >> 8),
                                 (unsigned char)c };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}
