/* interp.c - reusable 65SC02 interpreter core. See interp.h. */
#include "interp.h"
#include "decode.h"
#include <string.h>
#include <stdio.h>

interp_cpu_t icpu;
int      interp_event = EV_NONE;
uint16_t interp_event_addr = 0;
long     interp_trace = 0;

#define A  icpu.a
#define X  icpu.x
#define Y  icpu.y
#define S  icpu.s
#define PC icpu.pc

static uint16_t rd16(uint16_t a0) {
    return (uint16_t)(bus_read(a0) | (bus_read((uint16_t)(a0 + 1)) << 8));
}
static void setnz(uint8_t v) { icpu.z = (v == 0); icpu.n = (v >> 7) & 1; }

static void op_adc(uint8_t v) {
    unsigned c = icpu.c;
    if (!icpu.d) {
        unsigned s0 = A + v + c;
        icpu.v = ((~(A ^ v) & (A ^ s0)) >> 7) & 1; icpu.c = (s0 > 0xFF); A = (uint8_t)s0; setnz(A);
    } else {
        unsigned lo = (A & 15) + (v & 15) + c, hi = (A >> 4) + (v >> 4);
        if (lo > 9) { lo += 6; hi++; }
        unsigned bin = A + v + c; icpu.v = ((~(A ^ v) & (A ^ bin)) >> 7) & 1;
        if (hi > 9) hi += 6; icpu.c = (hi > 15); A = (uint8_t)((hi << 4) | (lo & 15)); setnz(A);
    }
}
static void op_sbc(uint8_t v) {
    unsigned c = icpu.c;
    if (!icpu.d) {
        unsigned d0 = A - v - (1 - c);
        icpu.v = (((A ^ v) & (A ^ d0)) >> 7) & 1; icpu.c = (d0 < 0x100); A = (uint8_t)d0; setnz(A);
    } else {
        int lo = (A & 15) - (v & 15) - (1 - c), hi = (A >> 4) - (v >> 4);
        if (lo < 0) { lo -= 6; hi--; } if (hi < 0) hi -= 6;
        unsigned d0 = A - v - (1 - c); icpu.v = (((A ^ v) & (A ^ d0)) >> 7) & 1;
        icpu.c = (d0 < 0x100); A = (uint8_t)((hi << 4) | (lo & 15)); setnz(A);
    }
}
static void cmp_(uint8_t r, uint8_t v) { uint8_t t = (uint8_t)(r - v); icpu.c = (r >= v); setnz(t); }
static uint8_t op_asl(uint8_t v) { icpu.c = (v >> 7) & 1; v <<= 1; setnz(v); return v; }
static uint8_t op_lsr(uint8_t v) { icpu.c = v & 1; v >>= 1; setnz(v); return v; }
static uint8_t op_rol(uint8_t v) { int c = (v >> 7) & 1; v = (uint8_t)((v << 1) | icpu.c); icpu.c = c; setnz(v); return v; }
static uint8_t op_ror(uint8_t v) { int c = v & 1; v = (uint8_t)((v >> 1) | (icpu.c << 7)); icpu.c = c; setnz(v); return v; }

static uint8_t pack_p(void) { return (uint8_t)((icpu.n<<7)|(icpu.v<<6)|0x30|(icpu.d<<3)|(icpu.i<<2)|(icpu.z<<1)|icpu.c); }
static void unpack_p(uint8_t p) { icpu.n=(p>>7)&1; icpu.v=(p>>6)&1; icpu.d=(p>>3)&1; icpu.i=(p>>2)&1; icpu.z=(p>>1)&1; icpu.c=p&1; }
static void push(uint8_t v) { bus_write(0x100 + S, v); S--; }
static uint8_t pull(void) { S++; return bus_read(0x100 + S); }

static uint16_t ea(const insn_t *in) {
    switch (in->mode) {
        case AM_ZP:  return in->operand;
        case AM_ZPX: return (uint8_t)(in->operand + X);
        case AM_ZPY: return (uint8_t)(in->operand + Y);
        case AM_ABS: return in->operand;
        case AM_ABX: return (uint16_t)(in->operand + X);
        case AM_ABY: return (uint16_t)(in->operand + Y);
        case AM_IZX: { uint8_t p = (uint8_t)(in->operand + X); return (uint16_t)(bus_read(p) | (bus_read((uint8_t)(p+1)) << 8)); }
        case AM_IZY: { uint8_t p = (uint8_t)in->operand; return (uint16_t)((bus_read(p) | (bus_read((uint8_t)(p+1)) << 8)) + Y); }
        case AM_IZP: { uint8_t p = (uint8_t)in->operand; return (uint16_t)(bus_read(p) | (bus_read((uint8_t)(p+1)) << 8)); }
        default: return 0;
    }
}
static uint8_t rdval(const insn_t *in) {
    if (in->mode == AM_IMM) return (uint8_t)in->operand;
    return bus_read(ea(in));
}

void interp_reset_pc(uint16_t pc) {
    A = X = Y = 0; S = 0xFF;
    icpu.n = icpu.v = icpu.d = icpu.z = icpu.c = 0; icpu.i = 1;
    PC = pc;
}

void interp_irq(void) {
    push((uint8_t)(PC >> 8)); push((uint8_t)PC);
    push((uint8_t)(pack_p() & ~0x10));   /* B clear on hardware IRQ */
    icpu.i = 1; icpu.d = 0;
    PC = rd16(0xFFFE);
}
void interp_nmi(void) {
    push((uint8_t)(PC >> 8)); push((uint8_t)PC);
    push((uint8_t)(pack_p() & ~0x10));
    icpu.i = 1; icpu.d = 0;
    PC = rd16(0xFFFA);
}

int interp_step(void) {
    interp_event = EV_NONE;
    uint8_t buf[3] = { bus_read(PC), bus_read((uint16_t)(PC+1)), bus_read((uint16_t)(PC+2)) };
    insn_t in; m65c02_decode(buf, PC, &in);
    const char *m = in.mnemonic;
    uint16_t next = (uint16_t)(PC + in.len);

    if (interp_trace > 0) {
        char dis[48]; m65c02_format(&in, dis, sizeof(dis));
        fprintf(stderr, "%04X A=%02X X=%02X Y=%02X S=%02X P=%c%c%c%c%c%c  %s\n",
                PC, A, X, Y, S, icpu.n?'N':'.', icpu.v?'V':'.', icpu.d?'D':'.',
                icpu.i?'I':'.', icpu.z?'Z':'.', icpu.c?'C':'.', dis);
        interp_trace--;
    }

    #define BR(c) do { if (c) next = in.target; } while (0)
    if      (!strcmp(m,"LDA")) { A = rdval(&in); setnz(A); }
    else if (!strcmp(m,"LDX")) { X = rdval(&in); setnz(X); }
    else if (!strcmp(m,"LDY")) { Y = rdval(&in); setnz(Y); }
    else if (!strcmp(m,"STA")) bus_write(ea(&in), A);
    else if (!strcmp(m,"STX")) bus_write(ea(&in), X);
    else if (!strcmp(m,"STY")) bus_write(ea(&in), Y);
    else if (!strcmp(m,"STZ")) bus_write(ea(&in), 0);
    else if (!strcmp(m,"ORA")) { A |= rdval(&in); setnz(A); }
    else if (!strcmp(m,"AND")) { A &= rdval(&in); setnz(A); }
    else if (!strcmp(m,"EOR")) { A ^= rdval(&in); setnz(A); }
    else if (!strcmp(m,"ADC")) op_adc(rdval(&in));
    else if (!strcmp(m,"SBC")) op_sbc(rdval(&in));
    else if (!strcmp(m,"CMP")) cmp_(A, rdval(&in));
    else if (!strcmp(m,"CPX")) cmp_(X, rdval(&in));
    else if (!strcmp(m,"CPY")) cmp_(Y, rdval(&in));
    else if (!strcmp(m,"BIT")) { uint8_t v = rdval(&in); icpu.z = ((A & v) == 0); if (in.mode != AM_IMM) { icpu.n = (v>>7)&1; icpu.v = (v>>6)&1; } }
    else if (!strcmp(m,"INC")) { if (in.mode==AM_ACC) { A++; setnz(A); } else { uint16_t e=ea(&in); uint8_t v=bus_read(e)+1; bus_write(e,v); setnz(v); } }
    else if (!strcmp(m,"DEC")) { if (in.mode==AM_ACC) { A--; setnz(A); } else { uint16_t e=ea(&in); uint8_t v=bus_read(e)-1; bus_write(e,v); setnz(v); } }
    else if (!strcmp(m,"ASL")) { if (in.mode==AM_ACC) A=op_asl(A); else { uint16_t e=ea(&in); bus_write(e,op_asl(bus_read(e))); } }
    else if (!strcmp(m,"LSR")) { if (in.mode==AM_ACC) A=op_lsr(A); else { uint16_t e=ea(&in); bus_write(e,op_lsr(bus_read(e))); } }
    else if (!strcmp(m,"ROL")) { if (in.mode==AM_ACC) A=op_rol(A); else { uint16_t e=ea(&in); bus_write(e,op_rol(bus_read(e))); } }
    else if (!strcmp(m,"ROR")) { if (in.mode==AM_ACC) A=op_ror(A); else { uint16_t e=ea(&in); bus_write(e,op_ror(bus_read(e))); } }
    else if (!strcmp(m,"TSB")) { uint16_t e=ea(&in); uint8_t v=bus_read(e); icpu.z=((A&v)==0); bus_write(e,(uint8_t)(v|A)); }
    else if (!strcmp(m,"TRB")) { uint16_t e=ea(&in); uint8_t v=bus_read(e); icpu.z=((A&v)==0); bus_write(e,(uint8_t)(v&~A)); }
    else if (!strncmp(m,"RMB",3)) { uint8_t z=(uint8_t)in.operand; bus_write(z, bus_read(z) & (uint8_t)~(1u<<in.bit)); }
    else if (!strncmp(m,"SMB",3)) { uint8_t z=(uint8_t)in.operand; bus_write(z, bus_read(z) | (uint8_t)(1u<<in.bit)); }
    else if (!strncmp(m,"BBR",3)) BR((bus_read((uint8_t)in.operand) & (1u<<in.bit)) == 0);
    else if (!strncmp(m,"BBS",3)) BR((bus_read((uint8_t)in.operand) & (1u<<in.bit)) != 0);
    else if (!strcmp(m,"INX")) { X++; setnz(X); }
    else if (!strcmp(m,"INY")) { Y++; setnz(Y); }
    else if (!strcmp(m,"DEX")) { X--; setnz(X); }
    else if (!strcmp(m,"DEY")) { Y--; setnz(Y); }
    else if (!strcmp(m,"TAX")) { X=A; setnz(X); }
    else if (!strcmp(m,"TAY")) { Y=A; setnz(Y); }
    else if (!strcmp(m,"TXA")) { A=X; setnz(A); }
    else if (!strcmp(m,"TYA")) { A=Y; setnz(A); }
    else if (!strcmp(m,"TSX")) { X=S; setnz(X); }
    else if (!strcmp(m,"TXS")) { S=X; }
    else if (!strcmp(m,"CLC")) icpu.c=0; else if (!strcmp(m,"SEC")) icpu.c=1;
    else if (!strcmp(m,"CLI")) icpu.i=0; else if (!strcmp(m,"SEI")) icpu.i=1;
    else if (!strcmp(m,"CLD")) icpu.d=0; else if (!strcmp(m,"SED")) icpu.d=1;
    else if (!strcmp(m,"CLV")) icpu.v=0;
    else if (!strcmp(m,"PHA")) push(A); else if (!strcmp(m,"PHX")) push(X);
    else if (!strcmp(m,"PHY")) push(Y); else if (!strcmp(m,"PHP")) push((uint8_t)(pack_p()|0x10));
    else if (!strcmp(m,"PLA")) { A=pull(); setnz(A); } else if (!strcmp(m,"PLX")) { X=pull(); setnz(X); }
    else if (!strcmp(m,"PLY")) { Y=pull(); setnz(Y); } else if (!strcmp(m,"PLP")) unpack_p(pull());
    else if (!strcmp(m,"BPL")) BR(!icpu.n); else if (!strcmp(m,"BMI")) BR(icpu.n);
    else if (!strcmp(m,"BVC")) BR(!icpu.v); else if (!strcmp(m,"BVS")) BR(icpu.v);
    else if (!strcmp(m,"BCC")) BR(!icpu.c); else if (!strcmp(m,"BCS")) BR(icpu.c);
    else if (!strcmp(m,"BNE")) BR(!icpu.z); else if (!strcmp(m,"BEQ")) BR(icpu.z);
    else if (!strcmp(m,"BRA")) next = in.target;
    else if (!strcmp(m,"NOP")) { /* nop (incl. multi-byte CMOS NOPs) */ }
    else if (!strcmp(m,"JMP")) {
        if (in.mode == AM_IND) { next = rd16(in.operand); interp_event = EV_INDJMP; interp_event_addr = next; }
        else if (in.mode == AM_IAX) { next = rd16((uint16_t)(in.operand + X)); interp_event = EV_INDJMP; interp_event_addr = next; }
        else next = in.operand;
    }
    else if (!strcmp(m,"JSR")) { uint16_t r=(uint16_t)(PC+2); push((uint8_t)(r>>8)); push((uint8_t)r); next = in.operand; }
    else if (!strcmp(m,"RTS")) { uint8_t lo=pull(), hi=pull(); next=(uint16_t)(((hi<<8)|lo)+1); }
    else if (!strcmp(m,"RTI")) { unpack_p(pull()); uint8_t lo=pull(), hi=pull(); next=(uint16_t)((hi<<8)|lo); }
    else if (!strcmp(m,"BRK")) { uint16_t r=(uint16_t)(PC+2); push((uint8_t)(r>>8)); push((uint8_t)r); push((uint8_t)(pack_p()|0x10)); icpu.i=1; icpu.d=0; next=rd16(0xFFFE); }
    else if (!strcmp(m,"WAI") || !strcmp(m,"STP")) { interp_event = EV_HALT; }
    else { fprintf(stderr, "interp: unimpl opcode %02X (%s) at $%04X\n", in.opcode, m, PC); return -1; }

    PC = next;
    return 0;
    #undef BR
}
