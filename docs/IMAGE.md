# From cart to a runnable RAM image (`lynxexec`)

Decryption (see [`BOOT.md`](BOOT.md)) recovers the boot loader, but the *game*
only exists in RAM after the loader streams it off the cart — across multiple
encrypted frames, using boot-ROM helper routines, ending in a `JMP ($004E)` to
an entry the loader computes at runtime. Reconstructing that by hand is fragile.
So `lynxexec` just **runs it**: a small 65SC02 executor boots the cart with the
real boot ROM and a modeled cart-read interface, then snapshots RAM and the
entry the moment the loader hands off to the game.

```
lynxexec <cart.lnx> <lynxboot.img> <ram.bin> [stopHex] [maxInsns] [traceN]
```

`lynxexec` is a build-time tool (it reuses only the shared decoder), not the
game runtime.

## What it models

- **CPU**: a full 65SC02 interpreter (same decoder the recompiler uses).
- **Boot ROM** mapped at `$FE00-$FFFF` from `lynxboot.img`. Reset starts at the
  ROM's reset vector; the real boot ROM does the RSA decrypt itself (our phase-2
  decryptor cross-checks it). `MAPCTL` ($FFF9) gates the ROM/vector overlay.
- **Cart read** (from the boot-ROM disassembly + Lynx hardware notes):
  - `$FE00` shifts an 8-bit block number into a register MSB-first via
    `IODAT`($FD8B).bit1 (data) and `SYSCTL1`($FD87).bit0 (strobe); the strobe's
    rising edge shifts a bit and resets the position counter.
  - reading `RCART0` ($FCB2) returns `cart[block*pagesize + position]` and
    post-increments `position`. `pagesize` comes from the `.lnx` header.
- **Suzy/Mikey** registers are a shadow array, with `SUZYHREV` ($FC88) seeded
  non-zero so the boot ROM's "Suzy present?" check passes. Timers/video aren't
  needed to load.

## Stop condition

It runs until the loader executes its `JMP ($004E)` indirect jump — the
documented hand-off to the game — then records the entry and dumps a 64 KiB RAM
image. (A `stopHex` PC and an instruction cap are also available.)

## Result on Chip's Challenge

```
$ lynxexec "Chip's Challenge (USA, Europe).lnx" lynxboot.img ram.bin
reset vector -> $FF80, cart pagesize 512
stopped after ~10.0M insns (reason 1), pc=$18B7
game entry (JMP indirect) -> $18B7
wrote 64KB RAM image -> ram.bin
```

(The ~10M instructions are mostly the boot ROM doing the RSA decrypt in 6502.)
Disassembling `ram.bin` at `$18B7` shows real game startup — set stack, program
Suzy/Mikey, map vectors into RAM (`MAPCTL |= $08`), install the IRQ handler at
`$1C40`, then enter the main loop. That image + entry feed the recompiler:

```
m65c02recomp recompbin ram.bin 0x0000 out/ 0x18B7 0x1C40
```

which discovers the game's functions (reset, IRQ handler, main loop, …) and
emits compilable C — the actual game, not just the loader.

## Notes / honesty

- `lynxexec` needs the 512-byte boot ROM (`lynxboot.img`) and your cart dump;
  neither is shipped here (no game data).
- This is a *load-time* model: it implements enough hardware to bring the game
  into RAM, not to play it. Suzy blitting, Mikey timers/IRQs, video and audio
  are the game-runtime work (still ahead).
- Function-boundary discovery on the game image is good but not perfect (6502
  code/data interleave); the hints format (planned) will pin the hard cases.
