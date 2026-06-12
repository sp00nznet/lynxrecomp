/* decode.h - WDC 65SC02 (CMOS 6502) instruction decoder.
 *
 * The Atari Lynx's CPU is a 65SC02 core integrated into the "Mikey" chip,
 * running at up to ~4 MHz. It is the CMOS variant of the 6502: it adds BRA,
 * PHX/PHY/PLX/PLY, STZ, TRB/TSB, INC/DEC A, the (zp) indirect mode, JMP
 * (abs,X), BIT immediate, the Rockwell bit ops (RMBn/SMBn/BBRn/BBSn) and
 * WAI/STP. There are NO undocumented opcodes - every unused encoding is a
 * well-defined multi-byte NOP.
 *
 * This is the foundation of the recompiler: decode -> analyze -> emit. In
 * phase 1 it powers a disassembler; later phases reuse the same table to
 * translate each instruction to C.
 */
#ifndef M65C02_DECODE_H
#define M65C02_DECODE_H

#include <stdint.h>

/* Addressing modes. Operand byte-count is derived from the mode. */
typedef enum {
    AM_IMP,   /* implied                         (1 byte)  */
    AM_ACC,   /* accumulator (e.g. ASL A)        (1 byte)  */
    AM_IMM,   /* #imm                            (2 bytes) */
    AM_ZP,    /* zp                              (2 bytes) */
    AM_ZPX,   /* zp,X                            (2 bytes) */
    AM_ZPY,   /* zp,Y                            (2 bytes) */
    AM_IZX,   /* (zp,X)                          (2 bytes) */
    AM_IZY,   /* (zp),Y                          (2 bytes) */
    AM_IZP,   /* (zp)            [65C02]         (2 bytes) */
    AM_REL,   /* relative branch                 (2 bytes) */
    AM_ABS,   /* abs                             (3 bytes) */
    AM_ABX,   /* abs,X                           (3 bytes) */
    AM_ABY,   /* abs,Y                           (3 bytes) */
    AM_IND,   /* (abs)         JMP indirect      (3 bytes) */
    AM_IAX,   /* (abs,X)       JMP [65C02]       (3 bytes) */
    AM_ZPREL, /* zp,rel        BBRn/BBSn [65C02] (3 bytes) */
    AM__COUNT
} addr_mode_t;

/* Control-flow class - drives function discovery in analyze.c. */
typedef enum {
    CF_NORMAL,  /* falls through                          */
    CF_BRANCH,  /* conditional branch (Bxx)               */
    CF_JMP,     /* unconditional jump (JMP, BRA)          */
    CF_CALL,    /* JSR                                    */
    CF_RET,     /* RTS / RTI                              */
    CF_BREAK,   /* BRK                                    */
    CF_STOP     /* STP / WAI                              */
} cflow_t;

typedef struct {
    const char *mnemonic;
    uint8_t     mode;   /* addr_mode_t */
    uint8_t     cflow;  /* cflow_t     */
} opinfo_t;

/* One decoded instruction. */
typedef struct {
    uint16_t pc;        /* address of the opcode byte           */
    uint8_t  opcode;    /* raw opcode                           */
    uint8_t  len;       /* total instruction length in bytes    */
    uint8_t  mode;      /* addr_mode_t                          */
    uint8_t  cflow;     /* cflow_t                              */
    const char *mnemonic;
    uint16_t operand;   /* decoded operand value (addr or imm)  */
    uint16_t target;    /* resolved branch/jump target, else 0  */
    uint8_t  bit;       /* bit index for RMB/SMB/BBR/BBS, else 0 */
} insn_t;

/* 256-entry opcode table, indexed by opcode byte. */
extern const opinfo_t m65c02_optab[256];

/* Bytes consumed by an instruction of the given addressing mode. */
int m65c02_mode_len(addr_mode_t mode);

/* Decode one instruction. `mem` points at the opcode byte; `pc` is its
 * address (used to resolve relative branch targets). Returns insn->len. */
int m65c02_decode(const uint8_t *mem, uint16_t pc, insn_t *out);

/* Format a decoded instruction as a disassembly string (no address column). */
void m65c02_format(const insn_t *in, char *buf, int bufsz);

#endif /* M65C02_DECODE_H */
