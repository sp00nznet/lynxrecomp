/* mikey.h - Mikey: timers, video DMA, audio, UART ($FD00-$FDFF).
 *
 * Mikey is the system glue chip (and it physically contains the 65SC02 core).
 * For recompilation it is another fixed-function peripheral:
 *   - 8 general timers (some chained) - timer 0 = horizontal, timer 2 =
 *     vertical refresh; these drive the frame cadence and IRQs;
 *   - a video DMA that streams the 160x102 x 4bpp framebuffer out of RAM to
 *     the LCD using the palette;
 *   - 4 audio channels;
 *   - a 16-entry palette (GREEN at $FDA0, BLUERED at $FDB0).
 *
 * Timer/IRQ stepping lives in timer.c; video readout is below. Register
 * offsets are from $FD00.
 */
#ifndef LYNXRECOMP_MIKEY_H
#define LYNXRECOMP_MIKEY_H

#include <stdint.h>

enum {
    MIKEY_TIM0BKUP = 0x00, MIKEY_TIM0CTLA = 0x01, MIKEY_TIM0CNT = 0x02, MIKEY_TIM0CTLB = 0x03,
    MIKEY_TIM2BKUP = 0x08, /* horizontal/vertical timers live in 0x00..0x1F */
    MIKEY_INTRST   = 0x80, /* interrupt reset / poll                        */
    MIKEY_INTSET   = 0x81,
    MIKEY_SYSCTL1  = 0x87, /* power + cart address strobe                   */
    MIKEY_IODIR    = 0x8A,
    MIKEY_IODAT    = 0x8B,
    MIKEY_SERCTL   = 0x8C,
    MIKEY_DISPCTL  = 0x92, /* video DMA enable + flip                       */
    MIKEY_PBKUP    = 0x93,
    MIKEY_DISPADRL = 0x94, MIKEY_DISPADRH = 0x95,  /* framebuffer base in RAM */
    MIKEY_PALGREEN = 0xA0, /* 16 bytes: green nibble per palette index       */
    MIKEY_PALBLURD = 0xB0  /* 16 bytes: blue/red nibbles per palette index   */
};

/* DISPCTL ($FD92) bits */
#define DISPCTL_DMA_ENABLE 0x01
#define DISPCTL_FLIP       0x02
#define DISPCTL_FOURBIT    0x04
#define DISPCTL_COLOR      0x08

/* Display geometry: 160x102, 4 bits/pixel, 2 pixels/byte (left = high nibble). */
#define LYNX_SCREEN_W  160
#define LYNX_SCREEN_H  102
#define LYNX_SCREEN_PITCH (LYNX_SCREEN_W / 2)   /* 80 bytes per scanline */

typedef struct {
    uint8_t reg[0x100];
} lynx_mikey_t;

extern lynx_mikey_t lynx_mikey;

uint8_t lynx_mikey_read(uint8_t off);
void    lynx_mikey_write(uint8_t off, uint8_t val);
void    lynx_mikey_init(void);

/* --- video readout (video.c) ---
 * Decode the framebuffer at DISPADR through the 16-entry palette into
 * 160*102 RGBA8888 pixels (0xAARRGGBB host order). `out` holds >= 160*102. */
void lynx_video_render(uint32_t *out);

/* Convert a 4-bit Lynx palette index to a 0xAARRGGBB colour. */
uint32_t lynx_palette_color(int index);

/* Write the current frame as a binary PPM (P6) - dependency-free; handy for
 * tests and headless dumps. Returns 0 on success. */
int lynx_video_write_ppm(const char *path);

#endif /* LYNXRECOMP_MIKEY_H */
