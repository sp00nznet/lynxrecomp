/* analyze.h - control-flow analysis over decoded 65SC02 code.
 *
 * Phase 1 shipped a linear sweep. Phase 3 adds recursive-descent function
 * discovery: seed from entry points, follow calls/branches/jumps, and carve the
 * code into functions the emitter turns into C. The hard part on a 6502 is that
 * code and data interleave and computed jumps hide targets; discovery is seeded
 * from known entries (and later, hints) and does not follow into addresses
 * outside the supplied image (e.g. the boot ROM at $FE00+).
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

/* ---- function discovery ---- */

#define MAX_FUNCS   1024
#define MAX_LABELS  256

typedef struct {
    uint16_t start;                 /* first address                       */
    uint16_t end;                   /* one past the last instruction byte  */
    uint16_t labels[MAX_LABELS];    /* in-function branch targets (sorted) */
    int      nlabels;
    int      reaches_ret;           /* contains an RTS/RTI                 */
} func_t;

typedef struct {
    func_t   funcs[MAX_FUNCS];
    int      nfuncs;
    /* external call/jump targets (e.g. boot ROM $FE00) - recorded, not emitted */
    uint16_t ext[MAX_FUNCS];
    int      next;
} func_table_t;

/* Recursive-descent discovery. Code image is `rom`/`rom_size` mapped at `base`.
 * Seeds are CPU addresses to start from (e.g. the loader entry). Targets that
 * fall outside [base, base+rom_size) are recorded as external, not followed.
 * Returns the number of functions discovered. */
int analyze_discover(const uint8_t *rom, size_t rom_size, uint16_t base,
                     const uint16_t *seeds, size_t nseeds,
                     func_table_t *out);

/* Find the function starting exactly at `addr`, or NULL. */
const func_t *func_table_find(const func_table_t *t, uint16_t addr);

#endif /* M65C02_ANALYZE_H */
