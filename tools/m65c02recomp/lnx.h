/* lnx.h - parser for the BLL ".lnx" Atari Lynx ROM container.
 *
 * The header is 64 bytes, little-endian:
 *   off  size  field
 *   0    4     magic "LYNX"
 *   4    2     page_size_bank0   (bytes per page, e.g. 512)
 *   6    2     page_size_bank1
 *   8    2     version           (1)
 *   10   32    cartname (NUL-padded)
 *   42   16    manufname (NUL-padded)
 *   58   1     rotation          (0 none, 1 left, 2 right)
 *   59   1     audin / spare
 *   60   4     spare
 * The remaining bytes are the raw cartridge image (bank0 then bank1).
 */
#ifndef LNX_H
#define LNX_H

#include <stdint.h>
#include <stddef.h>

#define LNX_HEADER_SIZE 64

typedef struct {
    int      valid;            /* header magic matched           */
    uint16_t page_size_bank0;
    uint16_t page_size_bank1;
    uint16_t version;
    char     cartname[33];
    char     manufname[17];
    uint8_t  rotation;
    const uint8_t *rom;        /* points into the loaded file    */
    size_t   rom_size;         /* bytes after the 64-byte header */
} lnx_info_t;

/* Parse a .lnx file already loaded into `data` (size `size`). On success
 * fills `out` (rom pointer aliases into `data`) and returns 0; -1 on error.
 * If the file has no LYNX magic it is treated as a raw, headerless image
 * (rom = data, rom_size = size, valid = 0). */
int lnx_parse(const uint8_t *data, size_t size, lnx_info_t *out);

#endif /* LNX_H */
