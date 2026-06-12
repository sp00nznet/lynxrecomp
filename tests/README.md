# Tests

Self-contained — **no game data**. `fixture.bin` is a hand-authored, public-domain
65SC02 routine used to exercise the whole pipeline.

- **test_units** — pure unit tests: the decoder on known opcodes (incl. CMOS-only
  forms) and the runtime ALU/flag helpers (`adc`/`sbc`/shifts/`cmp`).
- **test_pipeline** — the end-to-end recompiler proof. At build time the
  recompiler turns `fixture.bin` into C (`recompbin`); this test compiles that
  generated C, executes it as native code, and asserts the hardware/memory
  effects match what the original 65SC02 would do.

`fixture.bin` (base `$0200`):

```
        LDX #$03
loop:   STZ $FD00,X     ; clear 4 Mikey timer regs
        DEX
        BPL loop
        LDA #$AA
        STA $0050
        INC $0050       ; $0050 -> $AB
        RTS
```

Run with `ctest` after building, or run the `test_*` executables directly.
