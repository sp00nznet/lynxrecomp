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

## Status — phase 1 (scaffold)

What works today:

- **`m65c02recomp` builds and runs.** Three subcommands: `info` (parse a `.lnx`
  header), `dis` (linear-sweep disassembler), `emit` (write a `recomp_funcs`
  skeleton).
- **A complete, validated WDC 65SC02 decoder** — all 256 opcodes including the
  CMOS-only instructions (`BRA`, `PHX/PHY/PLX/PLY`, `STZ`, `TRB/TSB`, `(zp)`
  indirect, `JMP (abs,X)`, `BIT #`, `RMBn/SMBn/BBRn/BBSn`, `WAI/STP`) with
  correct addressing-mode lengths and branch-target resolution.
- **A working `.lnx` container parser** (BLL header: page sizes, names,
  rotation).
- **The runtime hardware model in code** — the 64 KiB memory map with Suzy
  ($FC00) / Mikey ($FD00) dispatch, CPU state + flag pack/unpack, and the Suzy/
  Mikey register maps. Peripherals are register-file stubs at this stage.

What's intentionally *not* done yet (see [`ROADMAP.md`](ROADMAP.md)):

- Function discovery (recursive descent) and the real C emitter.
- Getting an executable code image past the **encrypted boot block** — every
  retail Lynx cart boots through a 256-byte encrypted loader; see
  [`docs/BOOT.md`](docs/BOOT.md).
- The Suzy blitter + math unit, Mikey timers/IRQs, video DMA readout, audio.

```
$ m65c02recomp info "Chip's Challenge (USA, Europe).lnx"
container      : BLL .lnx (header 64 bytes)
cart name      : chipchal.lnx
manufacturer   : Atari
bank0 page size: 512 bytes
cart image     : 131072 bytes
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
- [`ROADMAP.md`](ROADMAP.md) — phased plan.

## Credits

- Cross-validation against IDA Pro / Ghidra (a 6502 processor module) as an
  independent disassembly oracle.
- Handy and Mednafen as behavioural references for Suzy/Mikey (reference only —
  all code here is original).

## License

MIT — see [`LICENSE`](LICENSE).
