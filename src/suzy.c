/* suzy.c - Suzy peripheral (sprite engine + math). Phase-1 register file only.
 * The blitter (SPRGO) and the multiply/divide unit are implemented in a later
 * phase; for now reads/writes land in a flat register array so recompiled code
 * links and runs up to the point it kicks the blitter. See suzy.h / ROADMAP. */
#include "lynxrecomp/suzy.h"

/* Provided by input.c. */
uint8_t lynx_input_joystick(void);
uint8_t lynx_input_switches(void);

lynx_suzy_t lynx_suzy;

void lynx_suzy_init(void) {
    for (int i = 0; i < 0x100; i++) lynx_suzy.reg[i] = 0;
    lynx_suzy.busy = 0;
}

uint8_t lynx_suzy_read(uint8_t off) {
    switch (off) {
        case SUZY_JOYSTICK: return lynx_input_joystick();
        case SUZY_SWITCHES: return lynx_input_switches();
        case SUZY_SPRSYS:   return lynx_suzy.busy ? 0x01 : 0x00; /* busy bit */
        default:            return lynx_suzy.reg[off];
    }
}

void lynx_suzy_write(uint8_t off, uint8_t val) {
    lynx_suzy.reg[off] = val;
    if (off == SUZY_SPRGO && (val & 1)) {
        /* TODO(phase 2): walk the SCB list at SCBNEXT and blit. */
        lynx_suzy.busy = 0;   /* pretend the blit completes instantly */
    }
}
