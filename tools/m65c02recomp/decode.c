/* decode.c - WDC 65SC02 opcode table + decoder. See decode.h. */
#include "decode.h"
#include <stdio.h>
#include <string.h>

#define N  CF_NORMAL
#define BR CF_BRANCH
#define JP CF_JMP
#define CL CF_CALL
#define RT CF_RET
#define BK CF_BREAK
#define SP CF_STOP

/* The full 256-entry WDC 65SC02 matrix. Unused encodings on the 65C02 are
 * defined NOPs of various widths (NOP/NOP#/NOP zp/NOP zp,X/NOP abs), so there
 * are no "illegal" gaps. */
const opinfo_t m65c02_optab[256] = {
/* x0            x1              x2            x3          x4            x5            x6            x7          x8            x9            xA            xB          xC            xD            xE            xF        */
/*0x*/
{"BRK",AM_IMP,BK},{"ORA",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"TSB",AM_ZP,N},{"ORA",AM_ZP,N},{"ASL",AM_ZP,N},{"RMB0",AM_ZP,N},{"PHP",AM_IMP,N},{"ORA",AM_IMM,N},{"ASL",AM_ACC,N},{"NOP",AM_IMP,N},{"TSB",AM_ABS,N},{"ORA",AM_ABS,N},{"ASL",AM_ABS,N},{"BBR0",AM_ZPREL,BR},
/*1x*/
{"BPL",AM_REL,BR},{"ORA",AM_IZY,N},{"ORA",AM_IZP,N},{"NOP",AM_IMP,N},{"TRB",AM_ZP,N},{"ORA",AM_ZPX,N},{"ASL",AM_ZPX,N},{"RMB1",AM_ZP,N},{"CLC",AM_IMP,N},{"ORA",AM_ABY,N},{"INC",AM_ACC,N},{"NOP",AM_IMP,N},{"TRB",AM_ABS,N},{"ORA",AM_ABX,N},{"ASL",AM_ABX,N},{"BBR1",AM_ZPREL,BR},
/*2x*/
{"JSR",AM_ABS,CL},{"AND",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"BIT",AM_ZP,N},{"AND",AM_ZP,N},{"ROL",AM_ZP,N},{"RMB2",AM_ZP,N},{"PLP",AM_IMP,N},{"AND",AM_IMM,N},{"ROL",AM_ACC,N},{"NOP",AM_IMP,N},{"BIT",AM_ABS,N},{"AND",AM_ABS,N},{"ROL",AM_ABS,N},{"BBR2",AM_ZPREL,BR},
/*3x*/
{"BMI",AM_REL,BR},{"AND",AM_IZY,N},{"AND",AM_IZP,N},{"NOP",AM_IMP,N},{"BIT",AM_ZPX,N},{"AND",AM_ZPX,N},{"ROL",AM_ZPX,N},{"RMB3",AM_ZP,N},{"SEC",AM_IMP,N},{"AND",AM_ABY,N},{"DEC",AM_ACC,N},{"NOP",AM_IMP,N},{"BIT",AM_ABX,N},{"AND",AM_ABX,N},{"ROL",AM_ABX,N},{"BBR3",AM_ZPREL,BR},
/*4x*/
{"RTI",AM_IMP,RT},{"EOR",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"NOP",AM_ZP,N},{"EOR",AM_ZP,N},{"LSR",AM_ZP,N},{"RMB4",AM_ZP,N},{"PHA",AM_IMP,N},{"EOR",AM_IMM,N},{"LSR",AM_ACC,N},{"NOP",AM_IMP,N},{"JMP",AM_ABS,JP},{"EOR",AM_ABS,N},{"LSR",AM_ABS,N},{"BBR4",AM_ZPREL,BR},
/*5x*/
{"BVC",AM_REL,BR},{"EOR",AM_IZY,N},{"EOR",AM_IZP,N},{"NOP",AM_IMP,N},{"NOP",AM_ZPX,N},{"EOR",AM_ZPX,N},{"LSR",AM_ZPX,N},{"RMB5",AM_ZP,N},{"CLI",AM_IMP,N},{"EOR",AM_ABY,N},{"PHY",AM_IMP,N},{"NOP",AM_IMP,N},{"NOP",AM_ABS,N},{"EOR",AM_ABX,N},{"LSR",AM_ABX,N},{"BBR5",AM_ZPREL,BR},
/*6x*/
{"RTS",AM_IMP,RT},{"ADC",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"STZ",AM_ZP,N},{"ADC",AM_ZP,N},{"ROR",AM_ZP,N},{"RMB6",AM_ZP,N},{"PLA",AM_IMP,N},{"ADC",AM_IMM,N},{"ROR",AM_ACC,N},{"NOP",AM_IMP,N},{"JMP",AM_IND,JP},{"ADC",AM_ABS,N},{"ROR",AM_ABS,N},{"BBR6",AM_ZPREL,BR},
/*7x*/
{"BVS",AM_REL,BR},{"ADC",AM_IZY,N},{"ADC",AM_IZP,N},{"NOP",AM_IMP,N},{"STZ",AM_ZPX,N},{"ADC",AM_ZPX,N},{"ROR",AM_ZPX,N},{"RMB7",AM_ZP,N},{"SEI",AM_IMP,N},{"ADC",AM_ABY,N},{"PLY",AM_IMP,N},{"NOP",AM_IMP,N},{"JMP",AM_IAX,JP},{"ADC",AM_ABX,N},{"ROR",AM_ABX,N},{"BBR7",AM_ZPREL,BR},
/*8x*/
{"BRA",AM_REL,JP},{"STA",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"STY",AM_ZP,N},{"STA",AM_ZP,N},{"STX",AM_ZP,N},{"SMB0",AM_ZP,N},{"DEY",AM_IMP,N},{"BIT",AM_IMM,N},{"TXA",AM_IMP,N},{"NOP",AM_IMP,N},{"STY",AM_ABS,N},{"STA",AM_ABS,N},{"STX",AM_ABS,N},{"BBS0",AM_ZPREL,BR},
/*9x*/
{"BCC",AM_REL,BR},{"STA",AM_IZY,N},{"STA",AM_IZP,N},{"NOP",AM_IMP,N},{"STY",AM_ZPX,N},{"STA",AM_ZPX,N},{"STX",AM_ZPY,N},{"SMB1",AM_ZP,N},{"TYA",AM_IMP,N},{"STA",AM_ABY,N},{"TXS",AM_IMP,N},{"NOP",AM_IMP,N},{"STZ",AM_ABS,N},{"STA",AM_ABX,N},{"STZ",AM_ABX,N},{"BBS1",AM_ZPREL,BR},
/*Ax*/
{"LDY",AM_IMM,N},{"LDA",AM_IZX,N},{"LDX",AM_IMM,N},{"NOP",AM_IMP,N},{"LDY",AM_ZP,N},{"LDA",AM_ZP,N},{"LDX",AM_ZP,N},{"SMB2",AM_ZP,N},{"TAY",AM_IMP,N},{"LDA",AM_IMM,N},{"TAX",AM_IMP,N},{"NOP",AM_IMP,N},{"LDY",AM_ABS,N},{"LDA",AM_ABS,N},{"LDX",AM_ABS,N},{"BBS2",AM_ZPREL,BR},
/*Bx*/
{"BCS",AM_REL,BR},{"LDA",AM_IZY,N},{"LDA",AM_IZP,N},{"NOP",AM_IMP,N},{"LDY",AM_ZPX,N},{"LDA",AM_ZPX,N},{"LDX",AM_ZPY,N},{"SMB3",AM_ZP,N},{"CLV",AM_IMP,N},{"LDA",AM_ABY,N},{"TSX",AM_IMP,N},{"NOP",AM_IMP,N},{"LDY",AM_ABX,N},{"LDA",AM_ABX,N},{"LDX",AM_ABY,N},{"BBS3",AM_ZPREL,BR},
/*Cx*/
{"CPY",AM_IMM,N},{"CMP",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"CPY",AM_ZP,N},{"CMP",AM_ZP,N},{"DEC",AM_ZP,N},{"SMB4",AM_ZP,N},{"INY",AM_IMP,N},{"CMP",AM_IMM,N},{"DEX",AM_IMP,N},{"WAI",AM_IMP,SP},{"CPY",AM_ABS,N},{"CMP",AM_ABS,N},{"DEC",AM_ABS,N},{"BBS4",AM_ZPREL,BR},
/*Dx*/
{"BNE",AM_REL,BR},{"CMP",AM_IZY,N},{"CMP",AM_IZP,N},{"NOP",AM_IMP,N},{"NOP",AM_ZPX,N},{"CMP",AM_ZPX,N},{"DEC",AM_ZPX,N},{"SMB5",AM_ZP,N},{"CLD",AM_IMP,N},{"CMP",AM_ABY,N},{"PHX",AM_IMP,N},{"STP",AM_IMP,SP},{"NOP",AM_ABS,N},{"CMP",AM_ABX,N},{"DEC",AM_ABX,N},{"BBS5",AM_ZPREL,BR},
/*Ex*/
{"CPX",AM_IMM,N},{"SBC",AM_IZX,N},{"NOP",AM_IMM,N},{"NOP",AM_IMP,N},{"CPX",AM_ZP,N},{"SBC",AM_ZP,N},{"INC",AM_ZP,N},{"SMB6",AM_ZP,N},{"INX",AM_IMP,N},{"SBC",AM_IMM,N},{"NOP",AM_IMP,N},{"NOP",AM_IMP,N},{"CPX",AM_ABS,N},{"SBC",AM_ABS,N},{"INC",AM_ABS,N},{"BBS6",AM_ZPREL,BR},
/*Fx*/
{"BEQ",AM_REL,BR},{"SBC",AM_IZY,N},{"SBC",AM_IZP,N},{"NOP",AM_IMP,N},{"NOP",AM_ZPX,N},{"SBC",AM_ZPX,N},{"INC",AM_ZPX,N},{"SMB7",AM_ZP,N},{"SED",AM_IMP,N},{"SBC",AM_ABY,N},{"PLX",AM_IMP,N},{"NOP",AM_IMP,N},{"NOP",AM_ABS,N},{"SBC",AM_ABX,N},{"INC",AM_ABX,N},{"BBS7",AM_ZPREL,BR},
};

int m65c02_mode_len(addr_mode_t mode) {
    switch (mode) {
        case AM_IMP: case AM_ACC:
            return 1;
        case AM_IMM: case AM_ZP: case AM_ZPX: case AM_ZPY:
        case AM_IZX: case AM_IZY: case AM_IZP: case AM_REL:
            return 2;
        case AM_ABS: case AM_ABX: case AM_ABY:
        case AM_IND: case AM_IAX: case AM_ZPREL:
            return 3;
        default:
            return 1;
    }
}

int m65c02_decode(const uint8_t *mem, uint16_t pc, insn_t *out) {
    uint8_t op = mem[0];
    const opinfo_t *oi = &m65c02_optab[op];

    memset(out, 0, sizeof(*out));
    out->pc       = pc;
    out->opcode   = op;
    out->mode     = oi->mode;
    out->cflow    = oi->cflow;
    out->mnemonic = oi->mnemonic;
    out->len      = (uint8_t)m65c02_mode_len((addr_mode_t)oi->mode);

    /* RMBn/SMBn/BBRn/BBSn encode the bit index in the mnemonic's last char. */
    if (oi->mnemonic[0] == 'R' || oi->mnemonic[0] == 'S' || oi->mnemonic[0] == 'B') {
        const char *m = oi->mnemonic;
        if ((m[0]=='R'&&m[1]=='M') || (m[0]=='S'&&m[1]=='M') ||
            (m[0]=='B'&&m[1]=='B')) {
            char c = m[3];
            if (c >= '0' && c <= '7') out->bit = (uint8_t)(c - '0');
        }
    }

    switch (oi->mode) {
        case AM_IMM: case AM_ZP: case AM_ZPX: case AM_ZPY:
        case AM_IZX: case AM_IZY: case AM_IZP:
            out->operand = mem[1];
            break;
        case AM_REL: {
            int8_t d = (int8_t)mem[1];
            out->operand = mem[1];
            out->target  = (uint16_t)(pc + 2 + d);
            break;
        }
        case AM_ZPREL: {                 /* zp byte, then signed rel byte */
            int8_t d = (int8_t)mem[2];
            out->operand = mem[1];       /* zero-page address tested      */
            out->target  = (uint16_t)(pc + 3 + d);
            break;
        }
        case AM_ABS: case AM_ABX: case AM_ABY:
        case AM_IND: case AM_IAX:
            out->operand = (uint16_t)(mem[1] | (mem[2] << 8));
            if (oi->mode == AM_ABS &&
                (oi->cflow == CF_JMP || oi->cflow == CF_CALL))
                out->target = out->operand;
            break;
        default:
            break;
    }
    return out->len;
}

void m65c02_format(const insn_t *in, char *buf, int bufsz) {
    const char *m = in->mnemonic;
    switch (in->mode) {
        case AM_IMP:   snprintf(buf, bufsz, "%s", m); break;
        case AM_ACC:   snprintf(buf, bufsz, "%s A", m); break;
        case AM_IMM:   snprintf(buf, bufsz, "%s #$%02X", m, in->operand); break;
        case AM_ZP:    snprintf(buf, bufsz, "%s $%02X", m, in->operand); break;
        case AM_ZPX:   snprintf(buf, bufsz, "%s $%02X,X", m, in->operand); break;
        case AM_ZPY:   snprintf(buf, bufsz, "%s $%02X,Y", m, in->operand); break;
        case AM_IZX:   snprintf(buf, bufsz, "%s ($%02X,X)", m, in->operand); break;
        case AM_IZY:   snprintf(buf, bufsz, "%s ($%02X),Y", m, in->operand); break;
        case AM_IZP:   snprintf(buf, bufsz, "%s ($%02X)", m, in->operand); break;
        case AM_REL:   snprintf(buf, bufsz, "%s $%04X", m, in->target); break;
        case AM_ABS:   snprintf(buf, bufsz, "%s $%04X", m, in->operand); break;
        case AM_ABX:   snprintf(buf, bufsz, "%s $%04X,X", m, in->operand); break;
        case AM_ABY:   snprintf(buf, bufsz, "%s $%04X,Y", m, in->operand); break;
        case AM_IND:   snprintf(buf, bufsz, "%s ($%04X)", m, in->operand); break;
        case AM_IAX:   snprintf(buf, bufsz, "%s ($%04X,X)", m, in->operand); break;
        case AM_ZPREL: snprintf(buf, bufsz, "%s $%02X,$%04X", m, in->operand, in->target); break;
        default:       snprintf(buf, bufsz, "%s ?", m); break;
    }
}
