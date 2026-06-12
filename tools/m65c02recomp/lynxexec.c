/* lynxexec.c - boot a Lynx cart far enough to snapshot the loaded game.
 *
 * Runs the shared 65SC02 interpreter (interp.c) over a bus that maps the real
 * boot ROM at $FE00 and models Mikey/Suzy's cart-read interface, then dumps the
 * RAM image + game entry the moment the loader hands off (its JMP ($004E)).
 * See docs/IMAGE.md.
 *
 *   lynxexec <cart.lnx> <lynxboot.img> <ram.bin> [stopHex] [maxInsns] [traceN]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"
#include "interp.h"

static uint8_t  ram[0x10000];
static uint8_t  bootrom[512];
static const uint8_t *cart;
static size_t   cart_size;
static unsigned pagesize = 2048;

static uint8_t  mapctl = 0;
static uint8_t  hwreg[0x200];
static unsigned cart_block = 0, cart_pos = 0;
static uint8_t  iodat = 0;
static int      strobe_prev = 0;

static uint8_t cart_read(void) {
    size_t addr = (size_t)cart_block * pagesize + cart_pos;
    cart_pos++;
    return (addr < cart_size) ? cart[addr] : 0xFF;
}

uint8_t bus_read(uint16_t addr) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        if (addr == 0xFCB2) return cart_read();
        return hwreg[addr - 0xFC00];
    }
    if (addr == 0xFFF9) return mapctl;
    if (addr >= 0xFE00) {
        int vectors = (addr >= 0xFFFA);
        int as_ram = vectors ? (mapctl & 0x08) : (mapctl & 0x04);
        if (!as_ram && addr != 0xFFF8) return bootrom[addr - 0xFE00];
    }
    return ram[addr];
}

void bus_write(uint16_t addr, uint8_t v) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        hwreg[addr - 0xFC00] = v;
        if (addr == 0xFD8B) iodat = v;
        else if (addr == 0xFD87) {
            int strobe = v & 1;
            if (strobe && !strobe_prev) {
                cart_block = ((cart_block << 1) | ((iodat >> 1) & 1)) & 0xFF;
                cart_pos = 0;
            }
            strobe_prev = strobe;
        }
        return;
    }
    if (addr == 0xFFF9) { mapctl = v; return; }
    ram[addr] = v;
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
        fprintf(stderr, "usage: %s <cart.lnx> <lynxboot.img> <ram.bin> [stopHex] [maxInsns] [traceN]\n", argv[0]);
        return 2;
    }
    size_t csz=0, bsz=0;
    uint8_t *cdata = read_file(argv[1], &csz);
    uint8_t *bdata = read_file(argv[2], &bsz);
    if (!cdata || !bdata || bsz < 512) { fprintf(stderr, "load error\n"); return 1; }

    lnx_info_t info;
    lnx_parse(cdata, csz, &info);
    cart = info.rom; cart_size = info.rom_size;
    if (info.valid && info.page_size_bank0) pagesize = info.page_size_bank0;
    memcpy(bootrom, bdata, 512);

    uint16_t stop_pc = (argc > 4) ? (uint16_t)strtoul(argv[4], NULL, 0) : 0xFFFF;
    long maxi = (argc > 5) ? strtol(argv[5], NULL, 0) : 20000000L;
    interp_trace = (argc > 6) ? strtol(argv[6], NULL, 0) : 0;

    memset(ram, 0, sizeof(ram));
    memset(hwreg, 0, sizeof(hwreg));
    hwreg[0x88] = 0x01;                 /* SUZYHREV: Suzy present */
    mapctl = 0; cart_block = cart_pos = 0; strobe_prev = 0;
    interp_reset_pc(0);
    icpu.pc = (uint16_t)(bus_read(0xFFFC) | (bus_read(0xFFFD) << 8));
    printf("reset vector -> $%04X, cart pagesize %u\n", icpu.pc, pagesize);

    uint16_t entry = 0;
    long i = 0; int reason = 0;
    for (; i < maxi; i++) {
        if (icpu.pc == stop_pc) { reason = 2; break; }
        if (interp_step() < 0) { reason = -1; break; }
        if (interp_event == EV_INDJMP) { entry = interp_event_addr; reason = 1; break; }
        if (interp_event == EV_HALT)   { reason = 4; break; }
    }
    if (reason == 0) reason = 3;

    printf("stopped after %ld insns (reason %d), pc=$%04X\n", i, reason, icpu.pc);
    if (reason == 1) printf("game entry (JMP indirect) -> $%04X\n", entry);
    printf("zp $4E/$4F = $%04X\n", (unsigned)(ram[0x4E] | (ram[0x4F] << 8)));

    FILE *o = fopen(argv[3], "wb");
    if (o) { fwrite(ram, 1, sizeof(ram), o); fclose(o); printf("wrote 64KB RAM image -> %s\n", argv[3]); }

    free(cdata); free(bdata);
    return reason == -1 ? 1 : 0;
}
