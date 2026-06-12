/* lnx.c - BLL ".lnx" container parser. See lnx.h. */
#include "lnx.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

int lnx_parse(const uint8_t *data, size_t size, lnx_info_t *out) {
    memset(out, 0, sizeof(*out));

    if (!data || size == 0)
        return -1;

    if (size >= LNX_HEADER_SIZE && memcmp(data, "LYNX", 4) == 0) {
        out->valid           = 1;
        out->page_size_bank0 = rd16(data + 4);
        out->page_size_bank1 = rd16(data + 6);
        out->version         = rd16(data + 8);
        memcpy(out->cartname,  data + 10, 32); out->cartname[32]  = 0;
        memcpy(out->manufname, data + 42, 16); out->manufname[16] = 0;
        out->rotation        = data[58];
        out->rom             = data + LNX_HEADER_SIZE;
        out->rom_size        = size - LNX_HEADER_SIZE;
        return 0;
    }

    /* Headerless raw image. */
    out->valid    = 0;
    out->rom      = data;
    out->rom_size = size;
    return 0;
}
