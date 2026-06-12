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

## Phase 3 — function discovery + emitter

- [ ] Full RAM image: model the loader + boot-ROM cart-read (or emulator
      snapshot) → complete cart→RAM load map + true game entry.
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
