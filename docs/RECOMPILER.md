# m65c02recomp — the recompiler

`m65c02recomp` turns a Lynx cartridge's 65SC02 code into native C. It is built
in the standard recompiler shape — **decode → analyze → emit** — and is meant to
be small and legible. Phase 1 ships the decoder, the `.lnx` parser, the linear
analyzer and a skeleton emitter; phases 2–3 add the real discovery and emission.

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

Phase 3: recursive-descent discovery. Seed a worklist from the reset/IRQ/NMI
vectors and user hints; pop an address, decode forward until a terminator
(`RTS`/`RTI`/`JMP`/`BRK`), push call/branch targets, and record `[start,end)`
per function. On a 6502 two things make this harder than on a RISC: code and
data interleave with no alignment, and computed jumps (`JMP (abs,X)` over a jump
table) hide targets in data. Both are handled with hints + jump-table detection,
the same way vbrecomp handles V810 dispatch tables.

## emit (`emit.c`)

Phase 1: `emit_skeleton()` writes a `generated/recomp_funcs.{c,h}` placeholder so
the per-game target links today.

Phase 3: one C function per discovered routine, `lynx_func_<addr>`, operating on
`lynx_cpu` and calling `lynx_mem_read/write`. The output is **meant to be read**:
every emitted line carries its source address and original disassembly as a
comment, flag math is factored into named helpers, and each function gets a
header comment with its range and role (e.g. IRQ handler). Same philosophy as
vbrecomp's generated V810 C.

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
m65c02recomp info <file.lnx>                 # parse + print the header
m65c02recomp dis  <file.lnx> [start] [count] # linear-sweep disassemble
m65c02recomp emit <file.lnx> <outdir>        # write recomp_funcs skeleton
```
