/* lynxexec.c - a small 65SC02 executor that boots a Lynx cart far enough to
 * snapshot the loaded game.
 *
 * The recompiler needs the post-boot RAM image and the game's entry point, but
 * a retail cart only yields that after the boot ROM decrypts the loader and the
 * loader streams the (multi-frame) game off the cart. Rather than model all of
 * that by hand, we run it: map the real 512-byte boot ROM at $FE00, model
 * Mikey/Suzy's cart-read interface, start at the reset vector, and let the real
 * boot ROM + loader execute. When the loader hands control to the game (its
 * `JMP ($004E)`), we snapshot RAM and report the entry.
 *
 * This is a build-time tool, deliberately self-contained (reuses only the
 * shared decoder); it is not the game runtime.
 *
 *   lynxexec <cart.lnx> <lynxboot.img> <ram.bin> [stopHex] [maxInsns]
 *
 * Cart-read model (from the boot ROM disassembly + Lynx hardware notes):
 *   - $FE00 shifts an 8-bit block number into a register MSB-first using
 *     IODAT($FD8B).bit1 as data and SYSCTL1($FD87).bit0 as the strobe; the
 *     strobe's rising edge shifts and resets the position counter.
 *   - reading RCART0 ($FCB2) returns cart[block*pagesize + position] and
 *     post-increments position.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"
#include "decode.h"

/* ---- machine state ---- */
static uint8_t  ram[0x10000];
static uint8_t  bootrom[512];
static const uint8_t *cart;
static size_t   cart_size;
static unsigned pagesize = 2048;

static uint8_t  mapctl = 0;          /* $FFF9: bit2=ROM->RAM, bit3=vectors->RAM */
static uint8_t  hwreg[0x200];        /* $FC00-$FDFF shadow                      */

/* cart address generator */
static unsigned cart_block = 0;
static unsigned cart_pos   = 0;
static uint8_t  iodat      = 0;      /* last $FD8B write */
static int      strobe_prev = 0;

/* CPU */
static uint8_t a, x, y, s;
static uint16_t pc;
static int fn_, fv, fd, fi, fz, fc;  /* flags */
static long trace = 0;               /* trace this many instructions */

static uint8_t cart_read(void) {
    size_t addr = (size_t)cart_block * pagesize + cart_pos;
    cart_pos++;
    return (addr < cart_size) ? cart[addr] : 0xFF;
}

static uint8_t rd(uint16_t addr) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        if (addr == 0xFCB2) return cart_read();          /* RCART0 */
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

static void wr(uint16_t addr, uint8_t v) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        hwreg[addr - 0xFC00] = v;
        if (addr == 0xFD8B) iodat = v;                   /* IODAT */
        else if (addr == 0xFD87) {                       /* SYSCTL1 */
            int strobe = v & 1;
            if (strobe && !strobe_prev) {                /* rising edge */
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

static uint16_t rd16(uint16_t a0) { return (uint16_t)(rd(a0) | (rd((uint16_t)(a0 + 1)) << 8)); }

/* ---- flags / alu ---- */
static void setnz(uint8_t v) { fz = (v == 0); fn_ = (v >> 7) & 1; }
static void op_adc(uint8_t v) {
    if (!fd) {
        unsigned s0 = a + v + fc;
        fv = ((~(a ^ v) & (a ^ s0)) >> 7) & 1; fc = (s0 > 0xFF); a = (uint8_t)s0; setnz(a);
    } else {
        unsigned lo = (a & 15) + (v & 15) + fc, hi = (a >> 4) + (v >> 4);
        if (lo > 9) { lo += 6; hi++; }
        unsigned bin = a + v + fc; fv = ((~(a ^ v) & (a ^ bin)) >> 7) & 1;
        if (hi > 9) hi += 6; fc = (hi > 15); a = (uint8_t)((hi << 4) | (lo & 15)); setnz(a);
    }
}
static void op_sbc(uint8_t v) {
    if (!fd) {
        unsigned d0 = a - v - (1 - fc);
        fv = (((a ^ v) & (a ^ d0)) >> 7) & 1; fc = (d0 < 0x100); a = (uint8_t)d0; setnz(a);
    } else {
        int lo = (a & 15) - (v & 15) - (1 - fc), hi = (a >> 4) - (v >> 4);
        if (lo < 0) { lo -= 6; hi--; } if (hi < 0) hi -= 6;
        unsigned d0 = a - v - (1 - fc); fv = (((a ^ v) & (a ^ d0)) >> 7) & 1;
        fc = (d0 < 0x100); a = (uint8_t)((hi << 4) | (lo & 15)); setnz(a);
    }
}
static void cmp_(uint8_t r, uint8_t v) { uint8_t t = (uint8_t)(r - v); fc = (r >= v); setnz(t); }
static uint8_t op_asl(uint8_t v) { fc = (v >> 7) & 1; v <<= 1; setnz(v); return v; }
static uint8_t op_lsr(uint8_t v) { fc = v & 1; v >>= 1; setnz(v); return v; }
static uint8_t op_rol(uint8_t v) { int c = (v >> 7) & 1; v = (uint8_t)((v << 1) | fc); fc = c; setnz(v); return v; }
static uint8_t op_ror(uint8_t v) { int c = v & 1; v = (uint8_t)((v >> 1) | (fc << 7)); fc = c; setnz(v); return v; }

static uint8_t pack_p(void) {
    return (uint8_t)((fn_<<7)|(fv<<6)|0x30|(fd<<3)|(fi<<2)|(fz<<1)|fc);
}
static void unpack_p(uint8_t p) {
    fn_=(p>>7)&1; fv=(p>>6)&1; fd=(p>>3)&1; fi=(p>>2)&1; fz=(p>>1)&1; fc=p&1;
}
static void push(uint8_t v) { wr(0x100 + s, v); s--; }
static uint8_t pull(void) { s++; return rd(0x100 + s); }

/* effective address for memory addressing modes */
static uint16_t ea(const insn_t *in) {
    switch (in->mode) {
        case AM_ZP:  return in->operand;
        case AM_ZPX: return (uint8_t)(in->operand + x);
        case AM_ZPY: return (uint8_t)(in->operand + y);
        case AM_ABS: return in->operand;
        case AM_ABX: return (uint16_t)(in->operand + x);
        case AM_ABY: return (uint16_t)(in->operand + y);
        case AM_IZX: { uint8_t p = (uint8_t)(in->operand + x); return (uint16_t)(rd(p) | (rd((uint8_t)(p+1)) << 8)); }
        case AM_IZY: { uint8_t p = (uint8_t)in->operand; return (uint16_t)((rd(p) | (rd((uint8_t)(p+1)) << 8)) + y); }
        case AM_IZP: { uint8_t p = (uint8_t)in->operand; return (uint16_t)(rd(p) | (rd((uint8_t)(p+1)) << 8)); }
        default: return 0;
    }
}
static uint8_t rdval(const insn_t *in) {
    if (in->mode == AM_IMM) return (uint8_t)in->operand;
    return rd(ea(in));
}

/* returns 0 to continue, 1 to stop (game entry reached), -1 on unknown opcode */
static int step(uint16_t *game_entry) {
    uint8_t buf[3];
    buf[0] = rd(pc); buf[1] = rd((uint16_t)(pc+1)); buf[2] = rd((uint16_t)(pc+2));
    insn_t in; m65c02_decode(buf, pc, &in);
    const char *m = in.mnemonic;
    uint16_t next = (uint16_t)(pc + in.len);

    if (trace > 0) {
        char dis[48]; m65c02_format(&in, dis, sizeof(dis));
        fprintf(stderr, "%04X A=%02X X=%02X Y=%02X S=%02X blk=%u pos=%u  %s\n",
                pc, a, x, y, s, cart_block, cart_pos, dis);
        trace--;
    }

    #define BR(cond) do { if (cond) next = in.target; } while (0)
    if      (!strcmp(m,"LDA")) { a = rdval(&in); setnz(a); }
    else if (!strcmp(m,"LDX")) { x = rdval(&in); setnz(x); }
    else if (!strcmp(m,"LDY")) { y = rdval(&in); setnz(y); }
    else if (!strcmp(m,"STA")) wr(ea(&in), a);
    else if (!strcmp(m,"STX")) wr(ea(&in), x);
    else if (!strcmp(m,"STY")) wr(ea(&in), y);
    else if (!strcmp(m,"STZ")) wr(ea(&in), 0);
    else if (!strcmp(m,"ORA")) { a |= rdval(&in); setnz(a); }
    else if (!strcmp(m,"AND")) { a &= rdval(&in); setnz(a); }
    else if (!strcmp(m,"EOR")) { a ^= rdval(&in); setnz(a); }
    else if (!strcmp(m,"ADC")) op_adc(rdval(&in));
    else if (!strcmp(m,"SBC")) op_sbc(rdval(&in));
    else if (!strcmp(m,"CMP")) cmp_(a, rdval(&in));
    else if (!strcmp(m,"CPX")) cmp_(x, rdval(&in));
    else if (!strcmp(m,"CPY")) cmp_(y, rdval(&in));
    else if (!strcmp(m,"BIT")) { uint8_t v = rdval(&in); fz = ((a & v) == 0); if (in.mode != AM_IMM) { fn_ = (v>>7)&1; fv = (v>>6)&1; } }
    else if (!strcmp(m,"INC")) { if (in.mode==AM_ACC) { a++; setnz(a); } else { uint16_t e=ea(&in); uint8_t v=rd(e)+1; wr(e,v); setnz(v); } }
    else if (!strcmp(m,"DEC")) { if (in.mode==AM_ACC) { a--; setnz(a); } else { uint16_t e=ea(&in); uint8_t v=rd(e)-1; wr(e,v); setnz(v); } }
    else if (!strcmp(m,"ASL")) { if (in.mode==AM_ACC) a=op_asl(a); else { uint16_t e=ea(&in); wr(e,op_asl(rd(e))); } }
    else if (!strcmp(m,"LSR")) { if (in.mode==AM_ACC) a=op_lsr(a); else { uint16_t e=ea(&in); wr(e,op_lsr(rd(e))); } }
    else if (!strcmp(m,"ROL")) { if (in.mode==AM_ACC) a=op_rol(a); else { uint16_t e=ea(&in); wr(e,op_rol(rd(e))); } }
    else if (!strcmp(m,"ROR")) { if (in.mode==AM_ACC) a=op_ror(a); else { uint16_t e=ea(&in); wr(e,op_ror(rd(e))); } }
    else if (!strcmp(m,"TSB")) { uint16_t e=ea(&in); uint8_t v=rd(e); fz=((a&v)==0); wr(e,(uint8_t)(v|a)); }
    else if (!strcmp(m,"TRB")) { uint16_t e=ea(&in); uint8_t v=rd(e); fz=((a&v)==0); wr(e,(uint8_t)(v&~a)); }
    else if (!strncmp(m,"RMB",3)) { uint8_t z=(uint8_t)in.operand; wr(z, rd(z) & (uint8_t)~(1u<<in.bit)); }
    else if (!strncmp(m,"SMB",3)) { uint8_t z=(uint8_t)in.operand; wr(z, rd(z) | (uint8_t)(1u<<in.bit)); }
    else if (!strncmp(m,"BBR",3)) BR((rd((uint8_t)in.operand) & (1u<<in.bit)) == 0);
    else if (!strncmp(m,"BBS",3)) BR((rd((uint8_t)in.operand) & (1u<<in.bit)) != 0);
    else if (!strcmp(m,"INX")) { x++; setnz(x); }
    else if (!strcmp(m,"INY")) { y++; setnz(y); }
    else if (!strcmp(m,"DEX")) { x--; setnz(x); }
    else if (!strcmp(m,"DEY")) { y--; setnz(y); }
    else if (!strcmp(m,"TAX")) { x=a; setnz(x); }
    else if (!strcmp(m,"TAY")) { y=a; setnz(y); }
    else if (!strcmp(m,"TXA")) { a=x; setnz(a); }
    else if (!strcmp(m,"TYA")) { a=y; setnz(a); }
    else if (!strcmp(m,"TSX")) { x=s; setnz(x); }
    else if (!strcmp(m,"TXS")) { s=x; }
    else if (!strcmp(m,"CLC")) fc=0; else if (!strcmp(m,"SEC")) fc=1;
    else if (!strcmp(m,"CLI")) fi=0; else if (!strcmp(m,"SEI")) fi=1;
    else if (!strcmp(m,"CLD")) fd=0; else if (!strcmp(m,"SED")) fd=1;
    else if (!strcmp(m,"CLV")) fv=0;
    else if (!strcmp(m,"PHA")) push(a); else if (!strcmp(m,"PHX")) push(x);
    else if (!strcmp(m,"PHY")) push(y); else if (!strcmp(m,"PHP")) push((uint8_t)(pack_p()|0x10));
    else if (!strcmp(m,"PLA")) { a=pull(); setnz(a); } else if (!strcmp(m,"PLX")) { x=pull(); setnz(x); }
    else if (!strcmp(m,"PLY")) { y=pull(); setnz(y); } else if (!strcmp(m,"PLP")) unpack_p(pull());
    else if (!strcmp(m,"BPL")) BR(!fn_); else if (!strcmp(m,"BMI")) BR(fn_);
    else if (!strcmp(m,"BVC")) BR(!fv); else if (!strcmp(m,"BVS")) BR(fv);
    else if (!strcmp(m,"BCC")) BR(!fc); else if (!strcmp(m,"BCS")) BR(fc);
    else if (!strcmp(m,"BNE")) BR(!fz); else if (!strcmp(m,"BEQ")) BR(fz);
    else if (!strcmp(m,"BRA")) next = in.target;
    else if (!strcmp(m,"NOP")) { /* nop */ }
    else if (!strcmp(m,"JMP")) {
        if (in.mode == AM_IND) { *game_entry = rd16(in.operand); pc = *game_entry; return 1; } /* loader -> game */
        if (in.mode == AM_IAX) { next = rd16((uint16_t)(in.operand + x)); }
        else next = in.operand;
    }
    else if (!strcmp(m,"JSR")) { uint16_t r=(uint16_t)(pc+2); push((uint8_t)(r>>8)); push((uint8_t)r); next = in.operand; }
    else if (!strcmp(m,"RTS")) { uint8_t lo=pull(), hi=pull(); next=(uint16_t)(((hi<<8)|lo)+1); }
    else if (!strcmp(m,"RTI")) { unpack_p(pull()); uint8_t lo=pull(), hi=pull(); next=(uint16_t)((hi<<8)|lo); }
    else if (!strcmp(m,"BRK")) { uint16_t r=(uint16_t)(pc+2); push((uint8_t)(r>>8)); push((uint8_t)r); push((uint8_t)(pack_p()|0x10)); fi=1; next=rd16(0xFFFE); }
    else if (!strcmp(m,"WAI") || !strcmp(m,"STP")) { return 1; }
    else { fprintf(stderr, "unimpl opcode %02X (%s) at $%04X\n", in.opcode, m, pc); return -1; }

    pc = next;
    return 0;
    #undef BR
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
        fprintf(stderr, "usage: %s <cart.lnx> <lynxboot.img> <ram.bin> [stopHex] [maxInsns]\n", argv[0]);
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
    trace = (argc > 6) ? strtol(argv[6], NULL, 0) : 0;

    /* reset */
    memset(ram, 0, sizeof(ram));
    memset(hwreg, 0, sizeof(hwreg));
    hwreg[0x88] = 0x01;   /* SUZYHREV ($FC88): boot ROM checks Suzy is present */
    a=x=y=0; s=0xFF; fi=1; fn_=fv=fd=fz=fc=0;
    mapctl = 0; cart_block = 0; cart_pos = 0; strobe_prev = 0;
    pc = rd16(0xFFFC);
    printf("reset vector -> $%04X, cart pagesize %u\n", pc, pagesize);

    uint16_t entry = 0;
    long i = 0;
    int reason = 0;   /* 1=game entry, 2=stop_pc, 3=max, -1=unimpl */
    for (; i < maxi; i++) {
        if (pc == stop_pc) { reason = 2; break; }
        int r = step(&entry);
        if (r == 1) { reason = 1; break; }
        if (r < 0) { reason = -1; break; }
    }
    if (reason == 0 || i >= maxi) reason = 3;

    printf("stopped after %ld insns (reason %d), pc=$%04X\n", i, reason, pc);
    if (reason == 1) printf("game entry (JMP indirect) -> $%04X\n", entry);
    printf("zp $4E/$4F = $%04X\n", (unsigned)(ram[0x4E] | (ram[0x4F] << 8)));

    FILE *o = fopen(argv[3], "wb");
    if (o) { fwrite(ram, 1, sizeof(ram), o); fclose(o); printf("wrote 64KB RAM image -> %s\n", argv[3]); }

    free(cdata); free(bdata);
    return reason == -1 ? 1 : 0;
}
