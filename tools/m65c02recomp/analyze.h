/* analyze.h - control-flow analysis over decoded 65SC02 code.
 *
 * Phase 1 ships a linear sweep (disassemble byte-after-byte from a start
 * offset) which is enough to dump code and validate the decoder. Phase 2 adds
 * recursive-descent function discovery (follow JSR/JMP/branch targets, build a
 * function table) - the same shape used by vbrecomp/N64Recomp. The hard part
 * on a 6502 is that code and data are freely interleaved and there is no clean
 * ABI, so discovery is seeded from the reset/IRQ vectors plus user hints.
 */
#ifndef M65C02_ANALYZE_H
#define M65C02_ANALYZE_H

#include <stdint.h>
#include <stddef.h>
#include "decode.h"

/* Called once per decoded instruction during a sweep. */
typedef void (*insn_cb)(const insn_t *in, void *user);

/* Linear sweep: decode from rom[start_off] for `count` instructions (0 = to
 * end of buffer). `base` is the CPU address that rom[0] maps to, so branch
 * targets resolve to real addresses. Returns instructions decoded. */
size_t analyze_linear(const uint8_t *rom, size_t rom_size, uint16_t base,
                      size_t start_off, size_t count,
                      insn_cb cb, void *user);

/* TODO(phase 2): recursive-descent discovery seeded from vectors + hints,
 * producing a function table (addr,size,kind). Declared here to fix the API. */
/* int analyze_discover(const uint8_t *rom, size_t rom_size, uint16_t base,
 *                      const uint16_t *seeds, size_t nseeds,
 *                      func_table_t *out); */

#endif /* M65C02_ANALYZE_H */
