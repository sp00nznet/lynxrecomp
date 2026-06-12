# lynxrecomp

**A static-recompilation toolkit for Atari Lynx games — the first one we know of.**

The Lynx (1989) is a near-perfect static-recompilation target and, somehow, has
never had a recompiler. It runs a single, fully-documented CPU — a **WDC 65SC02**
(the CMOS 6502) clocked up to ~4 MHz — with all the graphics, audio and timing
handled by two *fixed-function* support chips, **Suzy** and **Mikey**. That is
exactly the shape that recompiles cleanly: translate the one CPU to native C,
and emulate the support chips as peripherals. No second processor to chase, no
JIT, no interpreter in the hot path.

`lynxrecomp` is the reusable toolkit. The games brought up on it live in
separate repos that consume this one as a submodule — that split is deliberate:
the toolkit is the thing other people fork to recompile *their* Lynx game.

- [`chipschallenge-lynx-recomp`](https://github.com/sp00nznet/chipschallenge-lynx-recomp)
  — *Chip's Challenge* (the first; renders the title + plays the music).
- [`crystalmines2-lynx-recomp`](https://github.com/sp00nznet/crystalmines2-lynx-recomp)
  — *Crystal Mines II* (the second; a generalization test — a different game ran
  with zero toolkit changes).

> **No game data here.** ROMs (`.lnx`) are `.gitignore`d. This repo is the
> recompiler, the runtime, and docs — bring your own cartridge dump.

## How it works

```
  cart.lnx ──► lynxexec ──► ram.bin        boot (RSA decrypt + cart load) to a
              (boot ROM +    (game image,   resident RAM image, the game's jump
               cart model)    tables built)  tables populated
                  │
                  ▼
          ┌────────────────────┐   decode 65SC02, discover functions (following
          │  m65c02recomp      │   computed-jump tables), translate each routine
          │  (tools/)          │   to readable C: lynx_func_<addr>
          └────────────────────┘
                  │  generated/recomp_funcs.c
                  ▼
          ┌────────────────────┐   the runtime the generated C links against:
          │  lynxrecomp (lib)  │   CPU helpers · dispatch · tick/IRQ · Suzy
          │  (src/, include/)  │   (blitter, math) · Mikey (timers, video, audio)
          └────────────────────┘
                  │
                  ▼   (+ a per-game host: load image, register, run, present)
          native executable — the recompiled game runs
```

## Status — it runs

The whole pipeline works end to end: **encrypted cart → RSA boot decrypt → full
RAM image → discover functions → emit readable C → compile → run the recompiled
C → rendered frames + audio.** Brought up on *Chip's Challenge*, the **recompiled
C** (not an interpreter) renders the title screen and the 4-channel attract music
plays. The CPU is genuinely recompiled to native C; only the fixed-function
Suzy/Mikey hardware is emulated as a runtime — the same split as N64Recomp.

**The recompiler** (`m65c02recomp`)
- Complete, validated **WDC 65SC02 decoder** — all 256 opcodes incl. the CMOS-only
  set, correct mode lengths + branch targets.
- **Boot-block decryption** — RSA exponent-3, fixed 51-byte modulus, shift-and-add
  modular arithmetic. Verified byte-identical to an independent reference.
  ([`docs/BOOT.md`](docs/BOOT.md))
- **Recursive-descent discovery** that follows computed-jump tables read from the
  image, + **a C emitter** producing one readable `lynx_func_<addr>` per routine —
  every line annotated with its address and disassembly, lowered to centralized
  flag-correct runtime helpers (`recomp_rt.h`). On Chip's Challenge: **317
  functions, ~62 KB of code, zero dispatch gaps.**
- Subcommands: `info`, `dis`, `decrypt`/`loader`, `recomp`/`recompbin`, `emit`.

**Getting to runnable code** (`lynxexec`, `lynxrun --snapshot`)
- A 65SC02 executor boots the cart with the real boot ROM + a modeled cart-read
  interface and snapshots RAM (game resident, jump tables built). The recompiled
  game then runs from that image. ([`docs/IMAGE.md`](docs/IMAGE.md))

**The runtime** (the library the recompiled C links) — each unit-tested with
synthetic inputs, no game data:
- **CPU** state + flag-correct 65C02 semantic helpers; an `addr → function`
  dispatch table for computed jumps / IRQ vectors; a cooperative tick model that
  drives timers + delivers interrupts into the recompiled handler.
- **Suzy** — the sprite blitter (SCB walk, packed/literal, 1–4 bpp, H/V flip,
  per-type transparency + XOR) and the math unit (multiply/divide).
- **Mikey** — timers + interrupts (frame cadence), video readout (framebuffer +
  palette → RGB), and **4-channel audio** (LFSR + volume → PCM/WAV).

**Running it** (`lynxrun`, and a per-game host)
- `lynxrun` interprets a game against the runtime — the bring-up oracle — with
  `--play` (live window), `--capture` (frame sequence), `--snapshot`, `--audio`.
  The per-game host (in the game repo) runs the **recompiled** C directly.
  ([`docs/RUN.md`](docs/RUN.md))

What's *not* done yet (see [`ROADMAP.md`](ROADMAP.md)): blitter hardware
scaling/stretch/tilt + collision; signed math; stereo/attenuation (Howard);
live audio in `--play`; a full-fidelity recompiled cold boot. None are
foundational — they're polish on a working pipeline.

```c
// each 65SC02 routine becomes a readable C function, every line annotated:
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

The Lynx has emulators (Handy, Mednafen, the libretro cores) but — as far as we
can find — **no static recompiler**: static-recompilation tools exist for the
N64, PSX and NES, but nobody seems to have turned a Lynx game into a native
executable before. The hardware makes it tractable in a way most consoles
aren't: one documented
CPU, fixed-function video/audio, cartridge ROMs that are small and
self-contained. It's the same recipe that makes the Virtual Boy a clean target,
on a platform with a deeper and better-loved library.

## Repository layout

```
include/lynxrecomp/   runtime API (cpu, mem, recomp_rt, suzy, mikey, timer,
                      input, audio)
src/                  runtime: CPU helpers + dispatch/tick, Suzy blitter+math,
                      Mikey timers/video/audio
tools/m65c02recomp/   recompiler (lnx·decode·analyze·emit·lynxdec) + the
                      interp core, lynxexec (boot→image), lynxrun (driver)
tests/                ctest: decoder, ALU, blitter, math, audio, the recompile→
                      run pipeline, and computed-jump dispatch
docs/                 ARCHITECTURE · RECOMPILER · BOOT · IMAGE · RUN
```

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Lynx hardware, memory map, Suzy/Mikey.
- [`docs/RECOMPILER.md`](docs/RECOMPILER.md) — how `m65c02recomp` works and the readability goals.
- [`docs/BOOT.md`](docs/BOOT.md) — the encrypted boot block and how we get to runnable code.
- [`docs/IMAGE.md`](docs/IMAGE.md) — booting the cart to a full RAM image + game entry (`lynxexec`).
- [`docs/RUN.md`](docs/RUN.md) — the execution driver (`lynxrun`): running a game to rendered frames.
- [`ROADMAP.md`](ROADMAP.md) — phased plan.

## Credits & references

All code here is original, but it stands on a lot of prior reverse-engineering
and documentation. With thanks to:

- **[lynx-encryption-tools](https://github.com/dhuseby/lynx-encryption-tools)**
  (Dave Huseby et al.) — the public reference for the Lynx boot-block RSA
  decryption: the exponent-3 scheme, the public modulus, and the block framing.
  The encryption was reverse-engineered and released publicly in 2001;
  `lynxdec.c` reimplements the algorithm with its own bignum arithmetic and is
  cross-checked against it.
- **[cc65](https://github.com/cc65/cc65)** — its `_suzy.h` / `_mikey.h` headers
  were the reference for the Suzy/Mikey hardware register layouts and bit fields.
- **[Handy](https://github.com/libretro/libretro-handy)** and **Mednafen** —
  behavioural references for the Suzy sprite-data format and per-type
  transparency, the Mikey audio LFSR feedback taps, and the cartridge-read
  protocol. Reference only; no code copied.
- The **Epyx/Atari Lynx hardware specification** ("Handy Rev P") and Bastian
  Schick's / the *Diary of an Atari Lynx developer* documentation for the
  hardware details.
- **IDA Pro / Ghidra** (with a 6502 processor module) as an independent
  disassembly oracle for cross-validation.

> Running a game needs Atari's 512-byte Lynx **boot ROM** (`lynxboot.img`) — it
> is Atari's copyright and is **not** distributed here; supply your own (it's the
> same image other Lynx emulators use). The RSA modulus baked into `lynxdec.c` is
> Atari's published public key (a fact, not code). No ROMs or game data ship in
> this repo. Game screenshots in the reference-game repo are renders shown for
> documentation under fair use.

## License

MIT — see [`LICENSE`](LICENSE). Independent, non-commercial preservation work;
not affiliated with or endorsed by Atari.
