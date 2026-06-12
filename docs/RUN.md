# Running a game (`lynxrun`)

`lynxrun` is the execution driver: it runs a Lynx game against the real runtime
peripherals and renders frames. It's the bring-up vehicle — the first thing that
actually puts a game on screen — and the reference oracle for running the
*recompiled* C later.

```
lynxrun <cart.lnx> <lynxboot.img> <out.ppm> [maxInsns] [traceN] [traceAtIRQ]
```

## How it fits together

```
  interp.c (65SC02)  ──drives──►  runtime peripherals (the tested library)
        │                              suzy.c   blitter + math
   bus_read/write                      mikey.c  registers
        │                              timer.c  8 timers + IRQ latch
   boot ROM @ $FE00                    video.c  framebuffer + palette -> RGB
   cart-read model
        │
   reset ──► boot ROM decrypts loader ──► game runs ──► frames
```

The driver boots the cart exactly like `lynxexec` (real boot ROM + cart-read
model), but routes `$FC00-$FDFF` to the **runtime** Suzy/Mikey instead of a
shadow array — so the game's `SPRGO` writes run the real blitter, its timer
writes run the real timers, and the display reads the real framebuffer. Each
instruction advances ~1 µs of emulated time (approximate); when a timer
underflow latches an interrupt and the CPU's I flag is clear, the driver
delivers an IRQ through the game's RAM vector (`$FFFE`). After the budget it
writes the framebuffer to a PPM.

## Result on Chip's Challenge

```
$ lynxrun "Chip's Challenge (USA, Europe).lnx" lynxboot.img frame.ppm
reset -> $FF80, pagesize 512
ran 40000000 insns, game entry $18B7 ..., 9477 IRQs delivered
DISPADR=$C000 ...
wrote frame -> frame.ppm
```

`frame.ppm` is the **Chip's Challenge credits screen** — readable text on the
Epyx circuit-board background. The whole chain runs: RSA boot decrypt → game
load → CPU execution → Suzy blitter drawing sprites → Mikey timer IRQs driving
the frame loop → video readout. (Needs your cart dump + `lynxboot.img`; neither
is shipped.)

## Two bugs this bring-up surfaced (both fixed)

- **Suzy/video are DMA engines.** They read/write the physical DRAM directly,
  bypassing the MAPCTL hardware overlay. A framebuffer placed at `$E000`
  (extending past `$FC00`) must write RAM, not the `$FC00+` registers — the
  blitter was corrupting the timer registers until it accessed `lynx_ram[]`
  directly.
- **Timer link chain.** Linked timers count on a specific predecessor's
  underflow in the order `0→2→4→1→3→5→7`, not `i→i+1`. Timer 2 (VBL) is linked
  to timer 0 (HBL); with the wrong chain the frame interrupt never fired and the
  game spun forever waiting on its vblank flag.

## Status / what's next

`lynxrun` interprets the game. The end goal is to run the **recompiled** C
(`lynx_func_*`) against these same peripherals; that needs computed-jump
resolution (the game's `JMP ($1897,X)` dispatch) and an execution model for the
main loop + IRQ re-entry. `lynxrun` is the oracle that makes that tractable, and
already exercises the full runtime. Remaining peripheral work: audio, and the
blitter's hardware scaling/tilt + collision.
