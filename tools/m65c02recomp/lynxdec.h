/* lynxdec.h - Atari Lynx boot-block decryptor. See lynxdec.c.
 *
 * Recovers the RSA-encrypted secondary loader the boot ROM decrypts to $0200.
 * `rom`/`rom_size` are the cart image (after the .lnx header). Writes up to
 * 5*50 = 250 plaintext bytes into `out`; returns the byte count or -1. */
#ifndef LYNXDEC_H
#define LYNXDEC_H

#include <stddef.h>

/* The decrypted loader's entry point (where the boot ROM jumps after decrypt). */
#define LYNX_LOADER_BASE 0x0200

int lynx_decrypt_loader(const unsigned char *rom, size_t rom_size,
                        unsigned char *out, size_t out_cap);

#endif /* LYNXDEC_H */
