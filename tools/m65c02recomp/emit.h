/* emit.h - C code emission for the recompiler.
 *
 * emit_functions() translates a discovered function table into readable C: one
 * `lynx_func_<addr>` per routine, each instruction annotated with its address
 * and disassembly, calling the lynxrecomp runtime helpers (recomp_rt.h). This
 * is the heart of the static recompiler. emit_skeleton() remains for the
 * pre-discovery placeholder.
 */
#ifndef M65C02_EMIT_H
#define M65C02_EMIT_H

#include "lnx.h"
#include "analyze.h"

/* Write generated/recomp_funcs.{c,h} skeletons into `outdir`. Returns 0 ok. */
int emit_skeleton(const char *outdir, const lnx_info_t *info,
                  const char *rom_label);

/* Translate the discovered functions to C in `outdir`/recomp_funcs.{c,h}.
 * `rom` is the code image mapped at `base`. Returns 0 on success. */
int emit_functions(const char *outdir, const uint8_t *rom, size_t rom_size,
                   uint16_t base, const func_table_t *t, const char *rom_label);

#endif /* M65C02_EMIT_H */
