/* suzy.h - Suzy: the Lynx sprite engine + math coprocessor ($FC00-$FCFF).
 *
 * Suzy is fixed-function hardware, not a second CPU - which is exactly why the
 * Lynx is a good static-recompilation target: we recompile the one 65SC02 and
 * *emulate* Suzy as a peripheral the recompiled code pokes at. Suzy does:
 *   - hardware sprite blitting (SCB = Sprite Control Block lists in RAM,
 *     started by writing SPRGO at $FC91), with per-pixel scaling/tilt/stretch;
 *   - a 16x16 multiply and 16/16 divide unit (the MATH registers).
 *
 * Phase 1 declares the register map and a state struct; the blitter and math
 * unit are implemented in later phases (see ROADMAP). Register offsets are
 * from $FC00.
 */
#ifndef LYNXRECOMP_SUZY_H
#define LYNXRECOMP_SUZY_H

#include <stdint.h>

enum {
    SUZY_TMPADRL  = 0x00, SUZY_TMPADRH  = 0x01,
    SUZY_TILTACUM = 0x02, /* ... */
    SUZY_HOFFL    = 0x04, SUZY_HOFFH    = 0x05,
    SUZY_VOFFL    = 0x06, SUZY_VOFFH    = 0x07,
    SUZY_VIDBASL  = 0x08, SUZY_VIDBASH  = 0x09,  /* video buffer base   */
    SUZY_COLLBASL = 0x0A, SUZY_COLLBASH = 0x0B,  /* collision buffer    */
    SUZY_SCBNEXTL = 0x10, SUZY_SCBNEXTH = 0x11,  /* next SCB pointer    */
    SUZY_SPRDLINE = 0x12,
    SUZY_SPRCTL0  = 0x80, SUZY_SPRCTL1  = 0x81,
    SUZY_SPRCOLL  = 0x82,
    SUZY_SPRINIT  = 0x83,
    SUZY_SUZYBUSEN= 0x90,
    SUZY_SPRGO    = 0x91,  /* write 1 -> start the blitter             */
    SUZY_SPRSYS   = 0x92,  /* status: blitter busy, math busy, etc.    */
    SUZY_JOYSTICK = 0xB0,  /* read: D-pad + A/B/Option buttons         */
    SUZY_SWITCHES = 0xB1   /* read: pause + cart strobe etc.           */
};

typedef struct {
    uint8_t reg[0x100];
    int     busy;          /* blitter running                          */
} lynx_suzy_t;

extern lynx_suzy_t lynx_suzy;

uint8_t lynx_suzy_read(uint8_t off);
void    lynx_suzy_write(uint8_t off, uint8_t val);
void    lynx_suzy_init(void);

#endif /* LYNXRECOMP_SUZY_H */
