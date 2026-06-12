# m65c02recomp — the recompiler

`m65c02recomp` turns a Lynx cartridge's 65SC02 code into native C. It is built
in the standard recompiler shape — **decode → analyze → emit** — and is meant to
be small and legible. Phase 1 ships the decoder, the `.lnx` parser, the linear
analyzer and a skeleton emitter; phases 2–3 add the real discovery and emission.

## decrypt (`lynxdec.c`)

Before anything can be decoded, the cart's RSA-encrypted boot loader has to be
recovered — retail Lynx code does not exist in the clear until the boot ROM
decrypts it. `lynx_decrypt_loader()` reproduces that step (exponent-3 RSA, fixed
51-byte modulus); the result is the 250-byte secondary loader at `$0200`, which
is real 65SC02 the rest of the pipeline can consume. Full detail and the
verification are in [`BOOT.md`](BOOT.md).

## decode (`decode.c`)

A 256-entry table maps each opcode to `{mnemonic, addressing-mode, control-flow
class}`. `m65c02_decode()` reads up to three bytes and returns a fully-resolved
`insn_t`: length, operand, and — for branches and absolute jumps/calls — the
**resolved target address**. The control-flow class (`CALL`/`JMP`/`BRANCH`/
`RET`/`BREAK`/`STOP`/`NORMAL`) is what phase 3 discovery walks.

The table is the whole WDC 65SC02, validated against hand-assembled vectors
including the CMOS-only forms (`STZ`, `PHX`, `BRA`, `BBR0` with the `zp,rel`
mode, `JMP (abs,X)`).

## analyze (`analyze.c`)

Phase 1: `analyze_linear()` — disassemble from an offset for N instructions,
calling back per instruction. Enough to dump code and shake out decoder bugs.

`analyze_discover()` (done): seed a worklist from known entries (and later
hints); pop an address, decode forward extending past intra-function jumps until
a terminator (`RTS`/`RTI`/`JMP`/`BRK`) with no pending forward branch targets,
recording `[start,end)` and the in-function labels; push `JSR` targets as new
functions; record targets outside the image (boot ROM, computed jumps) as
external rather than following them. On the decrypted Chip's Challenge loader it
recovers exactly the five functions and four external targets a hand analysis
finds. Still ahead: computed-jump (`JMP (abs,X)` jump-table) resolution and the
hints format for the cases static analysis can't win.

## emit (`emit.c`)

`emit_functions()` (done): one C function per discovered routine,
`lynx_func_<addr>`, operating on `lynx_cpu` and reaching memory through
`lynx_mem_read/write`. The output is **meant to be read** — every emitted line
carries its source address and original disassembly as a comment, and flag math
is factored into the named runtime helpers in `recomp_rt.h` (`lynx_lda`,
`lynx_adc`, `lynx_alu_lsr`, …) rather than inlined. Intra-function control flow
becomes labels + `goto`, `JSR` becomes a C call to the target function,
`RTS/RTI` becomes `return`, and control that leaves the image (boot-ROM calls,
computed/indirect jumps) becomes an overridable runtime hook
(`lynx_boot_call`, `lynx_jmp_indirect`, …). Same philosophy as vbrecomp's
generated V810 C. `emit_skeleton()` remains for the pre-discovery placeholder.

## hints (phase 3)

A per-game `*.hints.txt` overrides the analyzer where static analysis can't win:

```
code   $0400            # force-disassemble here
data   $1000 $1200      # this range is data, don't decode it
name   $0450 vblank_isr # give a routine a real name
rename $0480 my_hle_fn  # intercept this routine with hand-written C (HLE)
```

`rename` is the HLE hook — a hand-written `src/main.c` in the game repo can
replace a recompiled routine (decompressors, math-heavy inner loops) with native
code while everything else stays recompiled.

## cross-validation

The recomp's function table is diffed against an independent IDA Pro / Ghidra
6502 disassembly (load the image at its true RAM addresses, let the tool analyze,
diff the two function sets) to catch missed functions, data-as-code, and boundary
disagreements — and to source real symbol names. This mirrors the IDA toolkit
already used against the PSX/N64 projects.

## usage

```
m65c02recomp info    <file.lnx>                 # parse + print the header
m65c02recomp dis     <file.lnx> [start] [count] # linear-sweep disassemble
m65c02recomp decrypt <file.lnx> <loader.bin>    # recover the boot loader
m65c02recomp loader  <file.lnx> [count]         # decrypt + disassemble loader
m65c02recomp emit    <file.lnx> <outdir>        # write recomp_funcs skeleton
```
