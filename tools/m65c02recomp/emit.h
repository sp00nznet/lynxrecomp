/* emit.h - C code emission for the recompiler.
 *
 * Phase 1 emits a *skeleton* generated/recomp_funcs.{c,h}: the file header,
 * includes against the lynxrecomp runtime, and a documented placeholder. Real
 * per-function translation (one C function per discovered 65SC02 routine,
 * every line annotated with its address + disassembly, in the readable style
 * of vbrecomp) lands in phase 2 once function discovery (analyze.c) and a
 * decrypted code image (see docs/BOOT.md) are in place.
 */
#ifndef M65C02_EMIT_H
#define M65C02_EMIT_H

#include "lnx.h"

/* Write generated/recomp_funcs.{c,h} skeletons into `outdir`. Returns 0 ok. */
int emit_skeleton(const char *outdir, const lnx_info_t *info,
                  const char *rom_label);

#endif /* M65C02_EMIT_H */
