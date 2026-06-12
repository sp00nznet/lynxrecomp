# lynxrecomp

**A static-recompilation toolkit for Atari Lynx games — the first one there is.**

The Lynx (1989) is a near-perfect static-recompilation target and, somehow, has
never had a recompiler. It runs a single, fully-documented CPU — a **WDC 65SC02**
(the CMOS 6502) clocked up to ~4 MHz — with all the graphics, audio and timing
handled by two *fixed-function* support chips, **Suzy** and **Mikey**. That is
exactly the shape that recompiles cleanly: translate the one CPU to native C,
and emulate the support chips as peripherals. No second processor to chase, no
JIT, no interpreter in the hot path.

`lynxrecomp` is the reusable toolkit. The first game brought up on it lives in a
separate repo, [`chipschallenge-lynx-recomp`](https://github.com/sp00nznet/chipschallenge-lynx-recomp),
which consumes this one as a submodule — that split is deliberate: the toolkit
is the thing other people fork to recompile *their* Lynx game.

> **No game data here.** ROMs (`.lnx`) are `.gitignore`d. This repo is the
> recompiler, the runtime, and docs — bring your own cartridge dump.

## How it works

```
  cart.lnx
     │
     ▼
  ┌────────────────────┐   parses the BLL .lnx header, decodes 65SC02,
  │  m65c02recomp      │   discovers functions, translates each routine to
  │  (tools/)          │   readable C (lynx_func_<addr>)
  └────────────────────┘
     │  generated/recomp_funcs.c
     ▼
  ┌────────────────────┐   the runtime the generated C links against:
  │  lynxrecomp (lib)  │   CPU state · memory dispatch · Suzy · Mikey ·
  │  (src/, include/)  │   timers · input
  └────────────────────┘
     │
     ▼   (+ a tiny per-game host: window, audio, input — in the game repo)
  native Lynx game executable
```

## Status — phases 1–3

The full pipeline runs: **encrypted cart → decrypt → discover functions → emit
readable C → compile → execute with correct effects.** What works today:

- **`m65c02recomp` builds and runs.** Subcommands: `info`, `dis`,
  `decrypt`/`loader` (recover the boot loader), **`recomp`** (decrypt + discover
  + emit C), **`recompbin`** (same, on a raw image), `emit`.
- **Boot-block decryption (phase 2).** RSA exponent-3, fixed 51-byte modulus,
  shift-and-add modular arithmetic. **Verified** byte-identical to an independent
  reference; recovered loader (entry `$0200`) is coherent 65SC02. See
  [`docs/BOOT.md`](docs/BOOT.md).
- **Function discovery + C emitter (phase 3).** Recursive-descent discovery
  carves the code into functions; the emitter writes one readable
  `lynx_func_<addr>` per routine — every line annotated with its address and
  disassembly — lowered to centralized, flag-correct runtime helpers
  (`recomp_rt.h`). Intra-function flow becomes labels + `goto`, `JSR` a C call,
  escapes runtime hooks.
- **Proven by execution.** `ctest` runs a synthetic-fixture pipeline test
  (recompile → compile the generated C → run it → assert hardware/memory
  effects) plus decoder/ALU unit tests. The same path recompiles the real
  Chip's Challenge loader routine `$02C9` and executes it correctly.
- **Full cart→RAM image** (`lynxexec`). A 65SC02 executor boots the cart with
  the real boot ROM + a modeled cart-read interface and snapshots RAM at the
  loader's hand-off — recovering Chip's Challenge's game entry (`$18B7`) and a
  64 KiB image. `recompbin` then discovers and emits the **game's** functions
  (reset, IRQ handler, main loop, …) as compilable C — not just the loader. See
  [`docs/IMAGE.md`](docs/IMAGE.md).
- **Runtime peripherals** the recompiled game drives, each unit-tested with
  synthetic inputs (no game data): **Mikey timers + interrupts** (frame
  cadence), **Mikey video readout** (framebuffer + palette → RGB), the **Suzy
  sprite blitter** (SCB walk, packed/literal sprites, 1–4 bpp, flip), and the
  **Suzy math unit** (multiply/divide).
- **Complete, validated WDC 65SC02 decoder** — all 256 opcodes incl. the
  CMOS-only set, with correct mode lengths and branch targets.
- **`.lnx` container parser** and the **runtime hardware model in code** (64 KiB
  map + Suzy/Mikey dispatch, CPU state + flags). Peripherals are register-file
  stubs at this stage.

What's *not* done yet (see [`ROADMAP.md`](ROADMAP.md)):

- Jump-table / computed-jump target resolution (the game's `JMP ($1897,X)`
  dispatch is an external hook today); better function-boundary discovery on
  game images; the hints format.
- Audio (4 channels); blitter hardware scaling/stretch/tilt + collision; signed
  math. Then: an execution driver that runs the recompiled game against these
  peripherals (frame loop + IRQ dispatch) to put Chip's Challenge on screen.

```c
// m65c02recomp recomp "Chip's Challenge (USA, Europe).lnx" out/  ->
/* lynx_func_02C9: $02C9-$02DD (21 bytes) */
void lynx_func_02C9(void) {
    /* 02C9: LDY #$1F   */ lynx_ldy(0x1F);
    /* 02CB: LDA #$00   */ lynx_lda(0x00);
L_02CD:
    /* 02CD: STA $FDA0,Y */ lynx_mem_write((uint16_t)(0xFDA0 + lynx_cpu.y), lynx_cpu.a);
    /* 02D0: DEY        */ lynx_dey();
    /* 02D1: BPL $02CD  */ if (!lynx_cpu.n) goto L_02CD;
    /* 02D3: LDA #$04   */ lynx_lda(0x04);
    /* 02D5: STA $FD8C  */ lynx_mem_write(0xFD8C, lynx_cpu.a);
    /* 02DD: RTS        */ return;
}
```

## Building

Requires CMake and a C compiler (MSVC on Windows; gcc/clang elsewhere).

```powershell
cmake -S . -B build
cmake --build build --config Release
# -> build/tools/m65c02recomp/Release/m65c02recomp.exe
# -> the lynxrecomp runtime static library
```

## Why Lynx (and why this is new ground)

The Lynx has emulators (Handy, Mednafen, the libretro cores) but **no static
recompiler** — nobody has turned a Lynx game into a native executable before.
The hardware makes it tractable in a way most consoles aren't: one documented
CPU, fixed-function video/audio, cartridge ROMs that are small and
self-contained. It's the same recipe that makes the Virtual Boy a clean target,
on a platform with a deeper and better-loved library.

## Repository layout

```
include/lynxrecomp/   runtime API (cpu, mem, suzy, mikey, timer, input)
src/                  runtime implementation
tools/m65c02recomp/   the recompiler: lnx parse · decode · analyze · emit
docs/                 ARCHITECTURE · RECOMPILER · BOOT
scripts/              corpus sweep (recompile-all harness)
```

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Lynx hardware, memory map, Suzy/Mikey.
- [`docs/RECOMPILER.md`](docs/RECOMPILER.md) — how `m65c02recomp` works and the readability goals.
- [`docs/BOOT.md`](docs/BOOT.md) — the encrypted boot block and how we get to runnable code.
- [`docs/IMAGE.md`](docs/IMAGE.md) — booting the cart to a full RAM image + game entry (`lynxexec`).
- [`ROADMAP.md`](ROADMAP.md) — phased plan.

## Credits

- Cross-validation against IDA Pro / Ghidra (a 6502 processor module) as an
  independent disassembly oracle.
- Handy and Mednafen as behavioural references for Suzy/Mikey (reference only —
  all code here is original).

## License

MIT — see [`LICENSE`](LICENSE).
