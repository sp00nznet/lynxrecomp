/* suzy.c - Suzy peripheral: input passthrough + the sprite blitter.
 *
 * Writing SPRGO ($FC91) starts the blitter, which walks the Sprite Control
 * Block (SCB) chain beginning at the address in SCBNEXT ($FC10/11) and draws
 * each sprite into the video buffer (VIDBAS, $FC08/09) in RAM. Sprite data is a
 * per-line bitstream (Atari's RLE), decoded here exactly as the hardware does
 * (format from the Handy reference): an 8-bit line offset (0 = end of sprite),
 * then packets - a 1-bit literal flag, a 4-bit count, and pen indices of
 * SPRCTL0 bits-per-pixel each, mapped through the SCB pen palette.
 *
 * Implemented: SCB chain walk, all four reload depths, the pen palette
 * (with reuse), packed + literal lines, 1-4 bpp, H/V flip, 1:1 placement with
 * the HOFF/VOFF screen origin. Not yet: hardware scaling/stretch/tilt,
 * collision, and the per-type pen-0 transparency rules (all pens are drawn).
 */
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mem.h"
#include "lynxrecomp/mikey.h"   /* LYNX_SCREEN_W/H/PITCH */

uint8_t lynx_input_joystick(void);
uint8_t lynx_input_switches(void);

lynx_suzy_t lynx_suzy;

void lynx_suzy_init(void) {
    for (int i = 0; i < 0x100; i++) lynx_suzy.reg[i] = 0;
    lynx_suzy.busy = 0;
}

uint8_t lynx_suzy_read(uint8_t off) {
    switch (off) {
        case SUZY_JOYSTICK: return lynx_input_joystick();
        case SUZY_SWITCHES: return lynx_input_switches();
        case SUZY_SPRSYS:   return lynx_suzy.busy ? 0x01 : 0x00;
        case 0x88:          return 0x01;          /* SUZYHREV: present */
        default:            return lynx_suzy.reg[off];
    }
}

/* ---- blitter ---- */

/* Suzy is a DMA engine: it reads SCBs / sprite data and writes the video
 * buffer in the physical DRAM directly, *bypassing* the MAPCTL hardware
 * overlay. So a video buffer placed at e.g. $E000 (extending past $FC00) writes
 * to RAM, not to the Suzy/Mikey registers - hence these go straight to
 * lynx_ram[] rather than through lynx_mem_read/write. */
static uint8_t  dram_rd(uint16_t a)            { return lynx_ram[a]; }
static void     dram_wr(uint16_t a, uint8_t v) { lynx_ram[a] = v; }

static uint16_t sreg16(uint8_t off) {
    return (uint16_t)(lynx_suzy.reg[off] | (lynx_suzy.reg[off + 1] << 8));
}

/* MSB-first bit reader over RAM. */
typedef struct { uint16_t addr; int bit; } bitr_t;
static unsigned bits_get(bitr_t *b, int n) {
    unsigned v = 0;
    while (n-- > 0) {
        uint8_t byte = dram_rd(b->addr);
        v = (v << 1) | ((byte >> (7 - b->bit)) & 1);
        if (++b->bit == 8) { b->bit = 0; b->addr++; }
    }
    return v;
}

/* Draw one pen (already palette-mapped to a 4-bit colour) at screen (x,y). */
static void put_pixel(uint16_t vidbas, int x, int y, uint8_t pen) {
    if (x < 0 || x >= LYNX_SCREEN_W || y < 0 || y >= LYNX_SCREEN_H) return;
    uint16_t a = (uint16_t)(vidbas + y * LYNX_SCREEN_PITCH + (x >> 1));
    uint8_t cur = dram_rd(a);
    if (x & 1) cur = (uint8_t)((cur & 0xF0) | (pen & 0x0F));
    else       cur = (uint8_t)((cur & 0x0F) | (pen << 4));
    dram_wr(a, cur);
}

void lynx_suzy_blit(void) {
    uint16_t vidbas = sreg16(SUZY_VIDBASL);
    int16_t  hoff   = (int16_t)sreg16(SUZY_HOFFL);
    int16_t  voff   = (int16_t)sreg16(SUZY_VOFFL);
    uint16_t scb    = sreg16(SUZY_SCBNEXTL);

    uint8_t pen[16];
    for (int i = 0; i < 16; i++) pen[i] = (uint8_t)i;   /* identity until loaded */

    int guard_sprites = 0;
    while (scb != 0 && guard_sprites++ < 256) {
        uint16_t p = scb;
        uint8_t ctl0 = dram_rd(p + 0);
        uint8_t ctl1 = dram_rd(p + 1);
        /* uint8_t coll = dram_rd(p + 2); */
        uint16_t next = (uint16_t)(dram_rd(p + 3) | (dram_rd(p + 4) << 8));
        uint16_t data = (uint16_t)(dram_rd(p + 5) | (dram_rd(p + 6) << 8));
        int16_t  hpos = (int16_t)(dram_rd(p + 7) | (dram_rd(p + 8) << 8));
        int16_t  vpos = (int16_t)(dram_rd(p + 9) | (dram_rd(p + 10) << 8));
        uint16_t f = (uint16_t)(p + 11);

        int bpp     = ((ctl0 >> 6) & 3) + 1;
        int hflip   = (ctl0 & 0x20) != 0;
        int vflip   = (ctl0 & 0x10) != 0;
        int literal = (ctl1 & 0x80) != 0;
        int reload  = (ctl1 >> 4) & 3;             /* 0 none,1 HV,2 HVS,3 HVST */
        int skip    = (ctl1 & 0x04) != 0;
        int nopal   = (ctl1 & 0x08) != 0;          /* reuse previous palette   */

        f += reload * 2 * 2;                       /* skip HV/S/T size words    */
        if (!nopal) {                              /* load pen palette          */
            int npens = 1 << bpp;
            for (int i = 0; i < npens; i += 2) {
                uint8_t byte = dram_rd(f++);
                pen[i]     = (uint8_t)(byte >> 4);
                pen[i + 1] = (uint8_t)(byte & 0x0F);
            }
        }

        if (!skip) {
            int sx = hpos - hoff;
            int sy = vpos - voff;
            int line = 0, guard_lines = 0;
            uint16_t dp = data;
            while (guard_lines++ < LYNX_SCREEN_H * 2) {
                uint8_t offset = dram_rd(dp);
                if (offset == 0) break;            /* end of sprite */
                int line_bits = (offset - 1) * 8;
                bitr_t br = { (uint16_t)(dp + 1), 0 };
                int y = vflip ? (sy - line) : (sy + line);
                int col = 0;

                if (offset != 1) {                 /* offset 1 = blank line */
                    if (literal) {                 /* totally-literal: raw pixels */
                        while (line_bits >= bpp) {
                            uint8_t px = pen[bits_get(&br, bpp) & 15]; line_bits -= bpp;
                            int x = hflip ? (sx - col) : (sx + col); col++;
                            put_pixel(vidbas, x, y, px);
                        }
                    } else while (line_bits > 0) { /* packed: flagged packets */
                        int is_lit = (int)bits_get(&br, 1); line_bits -= 1;
                        if (is_lit) {              /* literal run: count+1 pixels */
                            int cnt = (int)bits_get(&br, 4) + 1; line_bits -= 4;
                            for (int k = 0; k < cnt && line_bits >= bpp; k++) {
                                uint8_t px = pen[bits_get(&br, bpp) & 15]; line_bits -= bpp;
                                int x = hflip ? (sx - col) : (sx + col); col++;
                                put_pixel(vidbas, x, y, px);
                            }
                        } else {                   /* packed run: count+1 copies */
                            int cnt = (int)bits_get(&br, 4); line_bits -= 4;
                            if (cnt == 0) break;   /* end of line */
                            if (line_bits < bpp) break;
                            uint8_t px = pen[bits_get(&br, bpp) & 15]; line_bits -= bpp;
                            for (int k = 0; k <= cnt; k++) {
                                int x = hflip ? (sx - col) : (sx + col); col++;
                                put_pixel(vidbas, x, y, px);
                            }
                        }
                    }
                }
                dp = (uint16_t)(dp + offset);
                line++;
            }
        }
        scb = next;
    }
    lynx_suzy.busy = 0;
}

/* Suzy math: 16x16->32 multiply (trigger: write MATHA) and 32/16 divide
 * (trigger: write MATHE). Unsigned; signed mode (SPRSYS bit) is a refinement.
 * Byte order: ABCD.Long = A<<24|B<<16|C<<8|D ($55..$52), so AB=(A<<8)|B,
 * CD=(C<<8)|D; EFGH.Long = E<<24|F<<16|G<<8|H ($63..$60); likewise JKLM. */
static void put32(uint8_t lo_off, uint32_t v) {
    lynx_suzy.reg[lo_off + 0] = (uint8_t)v;
    lynx_suzy.reg[lo_off + 1] = (uint8_t)(v >> 8);
    lynx_suzy.reg[lo_off + 2] = (uint8_t)(v >> 16);
    lynx_suzy.reg[lo_off + 3] = (uint8_t)(v >> 24);
}
static uint32_t get32(uint8_t lo_off) {
    return (uint32_t)lynx_suzy.reg[lo_off]
         | ((uint32_t)lynx_suzy.reg[lo_off + 1] << 8)
         | ((uint32_t)lynx_suzy.reg[lo_off + 2] << 16)
         | ((uint32_t)lynx_suzy.reg[lo_off + 3] << 24);
}

void lynx_suzy_write(uint8_t off, uint8_t val) {
    lynx_suzy.reg[off] = val;
    switch (off) {
        case SUZY_SPRGO:
            if (val & 1) { lynx_suzy.busy = 1; lynx_suzy_blit(); }
            return;
        case SUZY_MATHA: {                  /* multiply: EFGH = AB * CD */
            uint16_t ab = (uint16_t)((lynx_suzy.reg[SUZY_MATHA] << 8) | lynx_suzy.reg[SUZY_MATHB]);
            uint16_t cd = (uint16_t)((lynx_suzy.reg[SUZY_MATHC] << 8) | lynx_suzy.reg[SUZY_MATHD]);
            put32(SUZY_MATHH, (uint32_t)ab * cd);
            return;
        }
        case SUZY_MATHE: {                  /* divide: ABCD = EFGH/NP, JKLM = EFGH%NP */
            uint32_t efgh = get32(SUZY_MATHH);
            uint16_t np = (uint16_t)((lynx_suzy.reg[SUZY_MATHN] << 8) | lynx_suzy.reg[SUZY_MATHP]);
            if (np) { put32(SUZY_MATHD, efgh / np); put32(SUZY_MATHM, efgh % np); }
            else    { put32(SUZY_MATHD, 0xFFFFFFFFu); put32(SUZY_MATHM, 0); }
            return;
        }
        default:
            return;
    }
}
