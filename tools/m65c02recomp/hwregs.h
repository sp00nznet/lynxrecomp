/* hwregs.h - names for the Lynx hardware registers ($FC00-$FDFF, MAPCTL).
 *
 * Used to annotate disassembly and recompiled C: a `STA $FC91` reads a lot
 * better as `STA $FC91   ; SPRGO (start blitter)`. Shared by the disassembler
 * and the emitter so both speak the same language. Register names + roles are
 * the documented Suzy/Mikey hardware (cc65 / Handy / the hardware spec). */
#ifndef M65C02_HWREGS_H
#define M65C02_HWREGS_H

#include <stdint.h>

/* The register name for a CPU address, or NULL if it isn't a known hardware
 * register. e.g. 0xFC91 -> "SPRGO", 0xFD80 -> "INTRST". */
const char *lynx_reg_name(uint16_t addr);

/* A short human note for the most important registers (what writing/reading it
 * does), or NULL. e.g. 0xFC91 -> "start blitter". */
const char *lynx_reg_note(uint16_t addr);

#endif /* M65C02_HWREGS_H */
