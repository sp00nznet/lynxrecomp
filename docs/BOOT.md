# The Lynx boot block (why you can't just disassemble a cart)

Disassemble the first bytes of any retail `.lnx` and you get noise:

```
$ m65c02recomp dis "Chip's Challenge (USA, Europe).lnx" 0 8
0200  FB NOP
0201  C5 CMP $BF
0203  A3 NOP
...
```

That's not 65SC02 code — it's the **encrypted boot block**, and it's the one
real obstacle between a Lynx ROM and recompilable code.

## How a Lynx cart boots

1. On reset the CPU runs Mikey's internal **512-byte boot ROM** at $FE00.
2. The boot ROM reads the **first ~256 bytes** of the cart through Mikey's cart
   strobe interface. These bytes are **encrypted** with Atari's scheme (a custom
   public-key-style signature check: the cart stores values that the boot ROM
   transforms with a fixed exponent/modulus and accumulates into a small
   plaintext loader). This was Atari's lockout — only Atari could produce a cart
   that the boot ROM would accept.
3. The decrypted result is a tiny **secondary loader** placed in RAM (around
   $00xx–$02xx). The boot ROM jumps to it.
4. That loader streams the rest of the cart's pages into DRAM (using the page
   size from the header — 512 bytes here) and jumps into the real game.

So the cart image is: `[encrypted boot block][plaintext game pages...]`. The
game code itself, after the boot block, is ordinary unencrypted 65SC02 — but you
need the load map (which page goes to which RAM address) that the loader carries.

## What this means for recompilation

Phase 2's job is to get from `cart.lnx` to **a flat code+data image with a known
load map**. Two routes:

- **Decrypt the boot block.** The scheme was reverse-engineered long ago; the
  modulus/exponent are public and tools (`lynx_encrypt`/`lynx_decrypt`,
  Handy/Mednafen's loader) already do it. We reimplement the decrypt to recover
  the secondary loader, then interpret it to learn the page→RAM mapping.
- **Trace a known-good emulator load.** Run the cart in a reference emulator to a
  post-boot point and snapshot RAM + the reset/IRQ vectors; use that as the
  image the recompiler analyzes. Useful as an oracle even once we decrypt
  directly.

The `dis`/`info` tools work on whatever image you hand them, so once phase 2
produces the decrypted, page-mapped image, the existing decoder/analyzer apply
unchanged — only the *input* changes, not the recompiler.

## Notes

- Homebrew and some dumps ship with a recognizable plaintext loader or a
  pre-decrypted layout; those are a useful early bring-up path before the full
  decrypt is wired in.
- The 64-byte `.lnx` header (BLL format) is **not** part of the cart and not
  encrypted; it carries the bank page sizes we need to walk pages once decoded.
