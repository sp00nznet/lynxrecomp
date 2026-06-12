# The Lynx boot block — decrypted (phase 2, done)

Disassemble the first bytes of any retail `.lnx` and you get noise — because the
first ~256 bytes are an **RSA-encrypted secondary loader**, the Lynx's lockout.
Phase 2 implements the decryption, so the recompiler can recover real 65SC02
code. This is now wired in: `m65c02recomp decrypt` / `m65c02recomp loader`, and
the library entry `lynx_decrypt_loader()` (`tools/m65c02recomp/lynxdec.c`).

## How a Lynx cart boots

1. On reset the CPU runs Mikey's internal **512-byte boot ROM** at `$FE00`.
2. The boot ROM reads the start of the cart through Mikey's cart strobe. The
   cart's **first byte is `256 - blockcount`** (so `$FB` → 5 blocks), followed
   by `blockcount` blocks of **51 encrypted bytes** each.
3. It RSA-decrypts the blocks into a **250-byte plaintext loader** and places it
   in RAM at **`$0200`**, then jumps there.
4. That loader brings up the display, then uses boot-ROM helpers to stream the
   (plaintext) game off the cart and runs it.

## The decryption (verified)

RSA with public exponent **3** and a fixed 51-byte public modulus `N`. Per block
(matches `dhuseby/lynx-encryption-tools`, the public reference; our C
implementation is checked byte-for-byte against an independent Python
implementation on the Chip's Challenge loader):

```
blocks = 256 - rom[0]                 # 5 for Chip's Challenge
acc    = 0                            # carried across all blocks
for each 51-byte block:
    x   = the 51 bytes, reversed, as a big integer
    r   = x^3 mod N                   # RSA, exponent 3
    buf = r big-endian, minimal, left-aligned in a zeroed 51-byte buffer
    for i = 50 downto 1:              # buf[0] is carry cruft, dropped
        acc = (acc + buf[i]) & 0xFF
        emit acc
# -> blocks*50 = 250 plaintext bytes = the loader at $0200
```

`N` (big-endian) is in `lynxdec.c`. The bignum work uses shift-and-add modular
arithmetic (modular double + conditional subtract) — only add/compare on 51-byte
values, no multiword multiply or division to get wrong.

## What the recovered loader does

The 250 decrypted bytes are unmistakably real code (entry `$0200`):

```
0200  BRA  $0202
0202  JSR  $02C9        ; clear the 32-byte palette ($FDA0..), set SERCTL/IODAT
0205  STZ  $05
0207  LDA  #$03
0209  STA  $06          ; zero-page load state
020B  JMP  $FE4A        ; into the boot ROM (cart-read helper)
...
0222  STA  $FC11        ; Suzy sprite-engine address setup
0227  LDA  $03E5,X      ; copy a table into Suzy regs $FC00,Y
022D  STA  $FC00,Y
0240  STZ  $FD94
0243  LDA  #$04
0245  STA  $FD95        ; DISPADR = $0400  (framebuffer base in RAM)
02DE  JSR  $FE00        ; boot-ROM cart-read entry
```

It clears the palette, programs Suzy, sets the framebuffer to `$0400`, and calls
boot-ROM helpers (`$FE00`/`$FE4A`) to pull the rest of the cart into RAM.

## From here to a full game image (phase 3)

The loader hands off to **boot-ROM routines** (`$FE00`/`$FE4A`) to read the
plaintext game pages into RAM. To produce the complete, page-mapped RAM image +
true entry the recompiler analyzes, either:

- **Model the loader + boot-ROM cart-read** statically (needs the 512-byte boot
  ROM image; the loader's copy loop and the cart directory then give the
  cart-offset → RAM-address map), or
- **Snapshot a reference emulator** (Handy/Mednafen) just past boot: dump RAM +
  the reset/IRQ/NMI vectors and feed that to the recompiler. The decryptor here
  is what lets us *validate* that snapshot against first principles.

Either way the decoder/analyzer are unchanged — only the input image changes.

## Vectors

After decrypt the effective entry is **`$0200`** (the analyzer's seed). The
`$FFFA-$FFFF` NMI/RESET/IRQ vectors live in the boot ROM at reset; the running
game installs its own IRQ handler in RAM and points Mikey's timer interrupts at
it — those RAM handler addresses are recovered during phase-3 discovery, seeded
from the loader and the timer-setup writes.

## Notes

- Only the first frame (≤255 bytes) is encrypted. Everything after is plaintext
  game code/data (some of it compressed — high entropy but not encrypted).
- Homebrew/`.o` dumps may ship a plaintext loader already; those are a useful
  early bring-up path.
- The 64-byte `.lnx` header is not part of the cart and not encrypted; it
  carries the bank page sizes used to walk pages.
