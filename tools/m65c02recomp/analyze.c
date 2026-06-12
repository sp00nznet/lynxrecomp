/* analyze.c - see analyze.h. */
#include "analyze.h"

size_t analyze_linear(const uint8_t *rom, size_t rom_size, uint16_t base,
                      size_t start_off, size_t count,
                      insn_cb cb, void *user) {
    size_t off = start_off;
    size_t n   = 0;
    while (off < rom_size && (count == 0 || n < count)) {
        insn_t in;
        /* Guard the 1-2 trailing operand bytes against running off the end. */
        uint8_t tmp[3] = {0, 0, 0};
        size_t avail = rom_size - off;
        tmp[0] = rom[off];
        if (avail > 1) tmp[1] = rom[off + 1];
        if (avail > 2) tmp[2] = rom[off + 2];

        m65c02_decode(tmp, (uint16_t)(base + off), &in);
        if (in.len > avail) break;     /* truncated final instruction */
        if (cb) cb(&in, user);
        off += in.len;
        n++;
    }
    return n;
}
