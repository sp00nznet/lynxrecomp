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

## Phase 2 — runnable code image

The blocker before any game logic runs: retail carts boot through an encrypted
256-byte loader (see [`docs/BOOT.md`](docs/BOOT.md)).

- [ ] Decrypt/bypass the boot block to obtain the loader, then follow the cart
      directory to the real game code, producing a flat code+data image with a
      known load map (which cart pages land at which RAM addresses).
- [ ] Reset/IRQ/NMI vector extraction to seed analysis.

## Phase 3 — function discovery + emitter

- [ ] Recursive-descent discovery from vectors + hints → function table.
- [ ] C emitter: one `lynx_func_<addr>` per routine, every line annotated with
      its address and original disassembly (readability is a goal, not an
      afterthought — the output is meant to be read).
- [ ] Jump-table and computed-jump (`JMP (abs,X)`) resolution.
- [ ] Hints format (force-code, force-data, function names, rename/HLE).

## Phase 4 — the peripherals

- [ ] Mikey timers + interrupt model (frame cadence comes from the vertical
      timer underflow).
- [ ] Suzy sprite blitter (SCB list walk, scaling/tilt) + math unit
      (multiply/divide).
- [ ] Mikey video DMA readout → 160×102×4bpp framebuffer + palette.
- [ ] Audio (4 channels).

## Phase 5 — bring-up & corpus

- [ ] Boot Chip's Challenge to first pixels, then playable.
- [ ] `scripts/sweep` over the whole Lynx library as a correctness corpus
      (recompile-all, like the vbrecomp approach) — each ROM that fails is a
      concrete decoder/analysis/codegen bug.
- [ ] IDA/Ghidra cross-validation of the discovered function table.
