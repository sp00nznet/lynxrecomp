# Roadmap

Phased plan. Phase 1 is the scaffold in this commit; later phases are the work.

## Phase 1 — scaffold ✅ (this commit)

- [x] Two-repo split: `lynxrecomp` (toolkit) + `chipschallenge-lynx-recomp`
      (reference game, submodules the toolkit).
- [x] `.lnx` container parser (BLL header).
- [x] Complete, validated WDC 65SC02 instruction decoder (all 256 opcodes,
      CMOS extensions, branch-target math).
- [x] `m65c02recomp` CLI: `info` / `dis` / `emit`.
- [x] Runtime hardware model in code: 64 KiB memory map + Suzy/Mikey dispatch,
      CPU state + flag pack/unpack, peripheral register maps.
- [x] Build system (CMake), docs, MIT license.

## Phase 2 — get to real code (boot decrypt) ✅

The blocker before any game logic runs: retail carts boot through an encrypted
256-byte loader (see [`docs/BOOT.md`](docs/BOOT.md)).

- [x] **Boot-block decryptor** (`lynxdec.c`): RSA exponent-3 with the public
      51-byte modulus, shift-and-add modular arithmetic (no multiword multiply/
      divide). Exposed as `m65c02recomp decrypt` / `loader` and the library call
      `lynx_decrypt_loader()`.
- [x] **Verified correct**: C output is byte-identical to an independent Python
      implementation on the Chip's Challenge loader, and the 250 decrypted bytes
      disassemble as coherent 65SC02 (entry `$0200`: palette/Suzy/display setup,
      boot-ROM cart-read calls).
- [x] ROM structure mapped: only the first ≤255 bytes are encrypted; the rest is
      plaintext game code/data.
- [x] Entry point recovered (`$0200`) to seed analysis.

Remaining for a *full* RAM image (rolled into phase 3): model the loader +
boot-ROM cart-read (or snapshot a reference emulator) to get the complete
cart→RAM load map. Decoder/analyzer are unchanged — only the input image is.

## Phase 3 — function discovery + emitter ✅ (core)

- [x] **Recursive-descent discovery** (`analyze_discover`): seed from entries,
      follow calls/branches/jumps, carve functions, record external targets
      (boot ROM, computed jumps). On the decrypted loader it recovers exactly
      the 5 functions + 4 external targets a hand analysis finds.
- [x] **C emitter** (`emit_functions`): one `lynx_func_<addr>` per routine, every
      line annotated with its address + disassembly, lowered to readable
      `lynxrecomp` runtime-helper calls; intra-function flow → labels + `goto`,
      `JSR` → call, `RTS/RTI` → return, escapes → runtime hooks.
- [x] **Runtime semantic helpers** (`recomp_rt.h`): centralized, flag-correct
      65C02 ops (loads/stores/`ADC`/`SBC` incl. BCD/compares/shifts/RMW/stack/
      transfers) so generated code stays legible.
- [x] **Proven end-to-end**: `m65c02recomp recompbin` recompiles code to C that
      compiles and **executes** with correct hardware/memory effects — covered
      by `ctest` (unit tests + a synthetic-fixture pipeline test), and
      demonstrated on the real Chip's Challenge loader routine `$02C9`.
- [x] **Full RAM image + true game entry** (`lynxexec`): a 65SC02 executor boots
      the cart with the real boot ROM + a modeled cart-read interface and
      snapshots RAM at the loader's `JMP ($004E)`. On Chip's Challenge it reaches
      game entry `$18B7` and dumps a 64 KiB image; `recompbin` then discovers and
      emits the **game's** functions (reset, IRQ `$1C40`, main loop, …) as
      compilable C. See [`docs/IMAGE.md`](docs/IMAGE.md).
- [ ] Jump-table / computed-jump (`JMP (abs,X)`, `JMP (zp)`) target resolution
      (the game's `JMP ($1897,X)` dispatch is currently an external hook).
- [ ] Better function-boundary discovery on game images (code/data interleave)
      and the hints format (force-code, force-data, names, rename/HLE).

## Phase 4 — the peripherals (runtime)

- [x] **Mikey timers + interrupt model** (`timer.c`): 8 timers, clock dividers,
      reload, linked cascade, underflow → interrupt latch; INTRST/INTSET. The
      ~60 Hz frame cadence the game waits on.
- [x] **Mikey video readout** (`video.c`): 160×102×4bpp framebuffer at DISPADR →
      16-entry palette → RGBA (+ PPM dump).
- [x] **Suzy sprite blitter** (`suzy.c`): SCB-chain walk, all four reload depths,
      pen palette, packed + literal lines at 1–4 bpp, H/V flip, HOFF/VOFF
      placement. (Hardware scaling/stretch/tilt + collision + per-type pen-0
      transparency still to do.)
- [x] **Suzy math unit** (`suzy.c`): 16×16→32 multiply and 32/16 divide
      (unsigned; signed mode is a refinement).
- [x] All four covered by `ctest` with synthetic inputs (no game data).
- [ ] Audio (4 channels).
- [ ] Blitter scaling/stretch/tilt + collision; signed math.

## Phase 5 — bring-up & corpus

- [x] **First pixels.** `lynxrun` (the execution driver, `docs/RUN.md`) boots the
      cart, runs the game against the real runtime peripherals (blitter, timers/
      IRQs, video), and renders the **Chip's Challenge credits screen** — the
      first Lynx game on screen through this toolkit. Two real hardware bugs
      fixed in the process: Suzy/video DMA must bypass the MAPCTL overlay (write
      DRAM directly), and the timer link chain is `0→2→4→1→3→5→7` (so VBL fires).
- [ ] Run the *recompiled* C (not the interpreter) against the peripherals:
      needs computed-jump (`JMP ($1897,X)`) resolution + a main-loop/IRQ
      execution model. `lynxrun` is the reference oracle.
- [ ] Playable: input wired to a host window; audio.
- [ ] `scripts/sweep` over the whole Lynx library as a correctness corpus
      (recompile-all, like the vbrecomp approach) — each ROM that fails is a
      concrete decoder/analysis/codegen bug.
- [ ] IDA/Ghidra cross-validation of the discovered function table.
