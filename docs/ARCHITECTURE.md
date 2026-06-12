# Atari Lynx hardware (recompiler's-eye view)

The Lynx is two custom chips around a CPU core. For recompilation the useful
framing is: **one CPU we translate, two peripherals we emulate.**

## CPU — WDC 65SC02

- CMOS 6502, integrated into the Mikey die, clocked up to ~4 MHz (software-
  selectable via a timer).
- Standard 6502 programmer's model: `A`, `X`, `Y`, `S` (stack in page 1), `PC`,
  and the `NV-BDIZC` status flags.
- CMOS additions over the NMOS 6502 that the decoder must handle: `BRA`,
  `PHX/PHY/PLX/PLY`, `STZ`, `TRB/TSB`, `(zp)` indirect, `JMP (abs,X)`, `BIT`
  immediate and `BIT zp,X/abs,X`, `INC A`/`DEC A`, Rockwell `RMBn/SMBn/BBRn/BBSn`,
  and `WAI/STP`. No undocumented opcodes — unused encodings are defined NOPs.
- Decimal mode exists but is rarely used; recompiled `ADC/SBC` honour the `D`
  flag for completeness.

## Memory map (64 KiB)

```
$0000-$00FF  zero page
$0100-$01FF  stack
$0200-$FBFF  DRAM — game code+data after the boot loader copies it here
$FC00-$FCFF  Suzy  registers      (when mapped by MAPCTL)
$FD00-$FDFF  Mikey registers      (when mapped by MAPCTL)
$FE00-$FFF7  boot ROM (512 B)     (when mapped)
$FFF8        reserved
$FFF9        MAPCTL — per-region select: hardware vs the DRAM underneath
$FFFA-$FFFF  NMI / RESET / IRQ vectors (when boot ROM mapped)
```

`MAPCTL` ($FFF9) lets software swap the DRAM hidden beneath each top region in
and out. The common runtime configuration has Suzy/Mikey mapped in; games
toggle the boot-ROM/vector region. The runtime models the common case first
(see `src/mem.c`) and the per-bit shadowing is a documented refinement.

The cartridge is **not** in the CPU address space linearly. It is read through
Mikey's cart-address strobe/shift interface a byte at a time; the boot loader
streams pages into DRAM. This is the single biggest difference from a console
like the Virtual Boy (where ROM is memory-mapped and the reset vector is a fixed
location). See [`BOOT.md`](BOOT.md).

## Suzy ($FC00) — sprite engine + math

Fixed-function, not a CPU:

- **Sprite blitter.** Software builds Sprite Control Blocks (SCBs) in RAM — a
  linked list describing source bitmap, position, palette, and scaling/tilt —
  then writes `SPRGO` ($FC91) to start it. Suzy walks the list and draws into
  the video buffer, with per-scanline horizontal/vertical scaling and stretch
  (the "stretch/tilt" registers) that produce the Lynx's signature hardware
  zoom. `SPRSYS` ($FC92) reports busy/collision status.
- **Math unit.** A 16×16→32 multiply and a 32÷16 divide, accessed through the
  `MATH*` registers — used heavily by the pseudo-3D games (Blue Lightning,
  S.T.U.N. Runner) and far less by tile/puzzle games (Chip's Challenge).
- **Input.** `JOYSTICK` ($FCB0) and `SWITCHES` ($FCB1) read the buttons.

## Mikey ($FD00) — timers, video, audio, glue

- **8 timers**, each counting down from a backup value at a selectable clock,
  with optional reload and chaining. Two are special: the horizontal and
  vertical refresh timers. The vertical timer's underflow is the ~60 Hz frame
  interrupt the main loop synchronises on. Timers also set the CPU clock and
  drive the UART (ComLynx).
- **Video DMA.** Streams the framebuffer from DRAM to the LCD. The display is
  **160×102, 4 bits per pixel** through a 16-entry palette. `DISPADR`
  ($FD94/95) points at the framebuffer; `DISPCTL` ($FD92) enables DMA and
  selects flip.
- **Palette.** 16 entries, each a 12-bit GRB colour split across `PALGREEN`
  ($FDA0..) and `PALBLUERED` ($FDB0..).
- **Audio.** 4 channels (the later "Howard" variants add stereo), poll/IRQ
  driven off the timer system.
- **Interrupts.** `INTSET`/`INTRST` ($FD80/81) gate the timer IRQ sources into
  the 65SC02 IRQ line.

## Why this recompiles cleanly

Everything stateful at speed is either (a) the one CPU, which we translate to C,
or (b) a register-poked peripheral, which we emulate. There is no second
instruction stream to recover, no self-modifying coprocessor microcode. The
hard, game-specific surface is Suzy's blitter (scaling math must match exactly)
and the timer/IRQ cadence — both well-documented and shared across the whole
library, so fixing them once benefits every game.
