/* lynxdec.c - Atari Lynx cartridge boot-block decryptor.
 *
 * Every retail Lynx cart begins with an RSA-encrypted secondary loader (the
 * console's lockout). The boot ROM decrypts it into RAM at $0200 and jumps
 * there; that loader then streams the (plaintext) game off the cart. This file
 * reproduces that decryption so the recompiler can recover real 65SC02 code.
 *
 * Algorithm (matches dhuseby/lynx-encryption-tools, the public reference):
 *   - The cart's first byte is (256 - blockcount); here blockcount is usually 5.
 *   - Then blockcount blocks of 51 encrypted bytes follow.
 *   - Per block: reverse the 51 bytes, treat as a big integer, raise to the
 *     public exponent 3 modulo the public modulus N (RSA, exponent 3), take the
 *     result big-endian (BN_bn2bin: minimal, left-aligned in a zeroed 51-byte
 *     buffer), and run a byte accumulator over buf[50..1]:
 *         acc = (acc + buf[i]) & 0xFF;  out[next++] = acc;
 *     acc is carried across all blocks of the frame (initial 0).
 *   - Output is blockcount*50 plaintext bytes = the loader (250 bytes for 5).
 *
 * The bignum work is done with shift-and-add modular arithmetic (modular
 * double + conditional subtract), so there is no multiword multiply or division
 * to get wrong - only add/compare on 51-byte values.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"

#define KLEN 51                 /* modulus / block size in bytes        */
#define BLEN 52                 /* working buffers: 1 guard byte + KLEN  */

/* Public modulus N, big-endian (N[0]=0x35 is most significant). */
static const unsigned char NMOD[KLEN] = {
    0x35,0xB5,0xA3,0x94,0x28,0x06,0xD8,0xA2,0x26,0x95,0xD7,0x71,0xB2,0x3C,0xFD,0x56,
    0x1C,0x4A,0x19,0xB6,0xA3,0xB0,0x26,0x00,0x36,0x5A,0x30,0x6E,0x3C,0x4D,0x63,0x38,
    0x1B,0xD4,0x1C,0x13,0x64,0x89,0x36,0x4C,0xF2,0xBA,0x2A,0x58,0xF4,0xFE,0xE1,0xFD,
    0xAC,0x7E,0x79
};

/* big-endian 52-byte modulus (guard byte 0). */
static unsigned char Nbe[BLEN];

static void bn_set(unsigned char *r, const unsigned char *src_be51) {
    r[0] = 0;
    memcpy(r + 1, src_be51, KLEN);
}
static void bn_zero(unsigned char *r) { memset(r, 0, BLEN); }

/* compare a,b (BLEN big-endian). <0,0,>0 */
static int bn_cmp(const unsigned char *a, const unsigned char *b) {
    return memcmp(a, b, BLEN);
}
/* r -= m  (r >= m). */
static void bn_sub(unsigned char *r, const unsigned char *m) {
    int borrow = 0;
    for (int i = BLEN - 1; i >= 0; i--) {
        int v = r[i] - m[i] - borrow;
        borrow = (v < 0);
        r[i] = (unsigned char)(v & 0xFF);
    }
}
/* r += a  (full BLEN add, carry kept in guard byte). */
static void bn_add(unsigned char *r, const unsigned char *a) {
    int carry = 0;
    for (int i = BLEN - 1; i >= 0; i--) {
        int v = r[i] + a[i] + carry;
        carry = v >> 8;
        r[i] = (unsigned char)(v & 0xFF);
    }
}
/* r = (r + a) mod N, assuming r,a < N (sum < 2N -> one conditional subtract). */
static void bn_addmod(unsigned char *r, const unsigned char *a) {
    bn_add(r, a);
    if (bn_cmp(r, Nbe) >= 0) bn_sub(r, Nbe);
}
/* test bit `bit` (0 = LSB) of a BLEN big-endian number. */
static int bn_bit(const unsigned char *x, int bit) {
    int byte = BLEN - 1 - (bit >> 3);
    return (x[byte] >> (bit & 7)) & 1;
}

/* p = (a * b) mod N, with a,b < N. Shift-and-add over the bits of b. */
static void bn_mulmod(unsigned char *p, const unsigned char *a, const unsigned char *b) {
    unsigned char acc[BLEN];
    bn_zero(acc);
    for (int bit = KLEN * 8 - 1; bit >= 0; bit--) {
        bn_addmod(acc, acc);            /* acc = 2*acc mod N */
        if (bn_bit(b, bit))
            bn_addmod(acc, a);          /* acc = acc + a mod N */
    }
    memcpy(p, acc, BLEN);
}
/* r = x mod N for an arbitrary BLEN x (bitwise reduce). */
static void bn_reduce(unsigned char *r, const unsigned char *x) {
    unsigned char acc[BLEN], one[BLEN];
    bn_zero(acc); bn_zero(one); one[BLEN - 1] = 1;
    for (int bit = KLEN * 8 - 1; bit >= 0; bit--) {
        bn_addmod(acc, acc);
        if (bn_bit(x, bit)) bn_addmod(acc, one);
    }
    memcpy(r, acc, BLEN);
}

/* Decrypt one 51-byte encrypted block; returns the updated accumulator.
 * Writes PLAINTEXT_BLOCK_SIZE (50) bytes to `out`. */
static int decrypt_block(unsigned char *out, const unsigned char *enc, int acc) {
    /* load_reverse: reverse the 51 bytes into a big-endian bignum. */
    unsigned char blk[BLEN], r[BLEN], t[BLEN], res[BLEN];
    bn_zero(blk);
    for (int i = 0; i < KLEN; i++)
        blk[1 + i] = enc[KLEN - 1 - i];

    bn_reduce(r, blk);          /* block mod N (block may exceed N) */
    bn_mulmod(t, r, r);         /* block^2 mod N */
    bn_mulmod(res, t, r);       /* block^3 mod N */

    /* BN_bn2bin: minimal big-endian, left-aligned into a zeroed 51-byte buf. */
    unsigned char buf[KLEN];
    memset(buf, 0, KLEN);
    int first = 0;
    while (first < BLEN && res[first] == 0) first++;
    int nbytes = BLEN - first;            /* significant bytes */
    if (nbytes > KLEN) nbytes = KLEN;     /* (res < N <= 51 bytes) */
    memcpy(buf, res + (BLEN - nbytes), nbytes);

    /* accumulator over buf[50..1] (buf[0] is carry cruft, dropped). */
    for (int i = 50; i > 0; i--) {
        acc = (acc + buf[i]) & 0xFF;
        *out++ = (unsigned char)acc;
    }
    return acc;
}

/* Decrypt the whole boot frame from `rom` into `out` (caller-sized).
 * Returns the number of plaintext bytes written, or -1 on error. */
int lynx_decrypt_loader(const unsigned char *rom, size_t rom_size,
                        unsigned char *out, size_t out_cap) {
    if (rom_size < 1) return -1;
    int blocks = (256 - rom[0]) & 0xFF;
    if (blocks < 1 || blocks > 5) return -1;
    if ((size_t)(1 + blocks * KLEN) > rom_size) return -1;
    if (out_cap < (size_t)(blocks * 50)) return -1;

    bn_set(Nbe, NMOD);
    int acc = 0;
    const unsigned char *e = rom + 1;
    unsigned char *d = out;
    for (int b = 0; b < blocks; b++) {
        acc = decrypt_block(d, e, acc);
        d += 50;
        e += KLEN;
    }
    return blocks * 50;
}

#ifdef LYNXDEC_MAIN
static unsigned char *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    if (s <= 0) { fclose(f); return NULL; }
    unsigned char *b = (unsigned char *)malloc((size_t)s);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { free(b); fclose(f); return NULL; }
    fclose(f); *n = (size_t)s; return b;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <cart.lnx> <loader.bin>\n", argv[0]);
        return 2;
    }
    size_t sz = 0;
    unsigned char *data = read_file(argv[1], &sz);
    if (!data) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    lnx_info_t info;
    if (lnx_parse(data, sz, &info) != 0) { free(data); return 1; }

    unsigned char out[5 * 50];
    int n = lynx_decrypt_loader(info.rom, info.rom_size, out, sizeof(out));
    if (n < 0) { fprintf(stderr, "decrypt failed\n"); free(data); return 1; }

    FILE *f = fopen(argv[2], "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", argv[2]); free(data); return 1; }
    fwrite(out, 1, (size_t)n, f);
    fclose(f);
    printf("decrypted %d-byte loader (%d blocks) -> %s\n", n, n / 50, argv[2]);
    free(data);
    return 0;
}
#endif
