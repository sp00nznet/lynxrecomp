/* lynxrun - execution driver: run a Lynx game against the real runtime.
 *
 * This is the bring-up driver. It boots the cart with the shared interpreter
 * (interp.c) over the real boot ROM + cart-read model, but routes Suzy/Mikey to
 * the tested runtime peripherals (so the blitter, timers and video actually
 * run). It steps time, delivers the Mikey timer interrupt into the game's RAM
 * handler, runs a budget of instructions, and writes the framebuffer to a PPM.
 *
 *   lynxrun <cart.lnx> <lynxboot.img> <out.ppm> [maxInsns] [traceN]
 *
 * The eventual goal is to run the *recompiled* C against these same peripherals;
 * this interpreter-driver gets a frame on screen now and is the reference oracle
 * for that work.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"
#include "interp.h"
#include "lynxrecomp/mem.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/input.h"

static uint8_t  bootrom[512];
static const uint8_t *cart;
static size_t   cart_size;
static unsigned pagesize = 2048;
static uint8_t  mapctl = 0;
static unsigned cart_block = 0, cart_pos = 0;
static uint8_t  iodat = 0;
static int      strobe_prev = 0;

static uint8_t cart_read(void) {
    size_t a = (size_t)cart_block * pagesize + cart_pos;
    cart_pos++;
    return (a < cart_size) ? cart[a] : 0xFF;
}

uint8_t bus_read(uint16_t addr) {
    if (addr == 0xFCB2) return cart_read();
    if (addr == 0xFFF9) return mapctl;
    if (addr >= 0xFE00) {
        int vectors = (addr >= 0xFFFA);
        int as_ram = vectors ? (mapctl & 0x08) : (mapctl & 0x04);
        if (!as_ram && addr != 0xFFF8) return bootrom[addr - 0xFE00];
    }
    return lynx_mem_read(addr);          /* runtime: $FC/$FD -> suzy/mikey, else RAM */
}

void bus_write(uint16_t addr, uint8_t v) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        if (addr == 0xFD8B) iodat = v;
        else if (addr == 0xFD87) {
            int strobe = v & 1;
            if (strobe && !strobe_prev) {
                cart_block = ((cart_block << 1) | ((iodat >> 1) & 1)) & 0xFF;
                cart_pos = 0;
            }
            strobe_prev = strobe;
        }
        lynx_mem_write(addr, v);         /* runtime peripheral (blitter, timers, ...) */
        return;
    }
    if (addr == 0xFFF9) { mapctl = v; lynx_mem_write(addr, v); return; }
    lynx_mem_write(addr, v);
}

static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f=fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long s0=ftell(f); fseek(f,0,SEEK_SET);
    if(s0<=0){fclose(f);return NULL;}
    uint8_t *b=malloc((size_t)s0);
    if(fread(b,1,(size_t)s0,f)!=(size_t)s0){free(b);fclose(f);return NULL;}
    fclose(f); *n=(size_t)s0; return b;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <cart.lnx> <lynxboot.img> <out.ppm> [maxInsns] [traceN]\n", argv[0]);
        return 2;
    }
    size_t csz=0, bsz=0;
    uint8_t *cdata = read_file(argv[1], &csz);
    uint8_t *bdata = read_file(argv[2], &bsz);
    if (!cdata || !bdata || bsz < 512) { fprintf(stderr, "load error\n"); return 1; }

    lnx_info_t info; lnx_parse(cdata, csz, &info);
    cart = info.rom; cart_size = info.rom_size;
    if (info.valid && info.page_size_bank0) pagesize = info.page_size_bank0;
    memcpy(bootrom, bdata, 512);

    long maxi = (argc > 4) ? strtol(argv[4], NULL, 0) : 40000000L;
    long tracen = (argc > 5) ? strtol(argv[5], NULL, 0) : 0;
    long trace_start = (argc > 6) ? strtol(argv[6], NULL, 0) : 0;

    lynx_mem_init();
    lynx_suzy_init();
    lynx_mikey_init();
    lynx_timer_init();
    lynx_input_set(0x00, 0x00);
    mapctl = 0; cart_block = cart_pos = 0; strobe_prev = 0;

    interp_reset_pc((uint16_t)(bus_read(0xFFFC) | (bus_read(0xFFFD) << 8)));
    printf("reset -> $%04X, pagesize %u\n", icpu.pc, pagesize);

    long irqs = 0, blits = 0, entry_at = -1;
    uint16_t game_entry = 0;
    long i = 0;
    for (; i < maxi; i++) {
        if (interp_step() < 0) break;
        if (interp_event == EV_INDJMP && entry_at < 0) {
            entry_at = i; game_entry = interp_event_addr;   /* loader -> game */
        }
        /* ~1us of emulated time per instruction (approx); drive the timers. */
        lynx_timer_step(1);
        if (lynx_irq_pending() && !icpu.i) {
            interp_irq(); irqs++;
            if (irqs == trace_start && tracen > 0) interp_trace = tracen;  /* trace this IRQ */
        }
        if (lynx_suzy.busy) { /* a blit just ran (busy is cleared inside) */ }
    }
    /* blits are completed synchronously; count via a cheap proxy: re-derive
     * nothing here - just report what we can observe. */
    (void)blits;

    printf("ran %ld insns, game entry $%04X at insn %ld, %ld IRQs delivered, pc=$%04X\n",
           i, game_entry, entry_at, irqs, icpu.pc);
    printf("DISPADR=$%04X  MAPCTL=$%02X  irq_latch=$%02X  I=%d\n",
           (unsigned)(lynx_mikey.reg[MIKEY_DISPADRL] | (lynx_mikey.reg[MIKEY_DISPADRH] << 8)),
           mapctl, lynx_irq_latch, icpu.i);
    for (int t = 0; t < 8; t++)
        printf("  timer%d: backup=%02X ctlA=%02X count=%02X ctlB=%02X\n", t,
               lynx_mikey.reg[t*4+0], lynx_mikey.reg[t*4+1],
               lynx_mikey.reg[t*4+2], lynx_mikey.reg[t*4+3]);

    if (lynx_video_write_ppm(argv[3]) == 0) printf("wrote frame -> %s\n", argv[3]);
    free(cdata); free(bdata);
    return 0;
}
