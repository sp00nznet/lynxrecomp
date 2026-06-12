/* input.h - Lynx controls, surfaced through Suzy's JOYSTICK/SWITCHES regs.
 *
 * The Lynx has a D-pad, A and B, Option 1 and Option 2, Pause, and a "flip"
 * for the rotate-screen games. Because the console could be held either way,
 * hardware can flip the D-pad bits; recompiled code reads SUZY_JOYSTICK
 * ($FCB0). The host maps real input into this byte each frame. */
#ifndef LYNXRECOMP_INPUT_H
#define LYNXRECOMP_INPUT_H

#include <stdint.h>

/* JOYSTICK ($FCB0) bit layout (unflipped). */
enum {
    LYNX_BTN_UP     = 0x80,
    LYNX_BTN_DOWN   = 0x40,
    LYNX_BTN_LEFT   = 0x20,
    LYNX_BTN_RIGHT  = 0x10,
    LYNX_BTN_OPTION1= 0x08,
    LYNX_BTN_OPTION2= 0x04,
    LYNX_BTN_B      = 0x02,
    LYNX_BTN_A      = 0x01
};

/* SWITCHES ($FCB1). */
enum {
    LYNX_SW_PAUSE   = 0x01,
    LYNX_SW_CART0   = 0x02,
    LYNX_SW_CART1   = 0x04
};

/* Host sets the current button/switch state (called once per frame). */
void lynx_input_set(uint8_t joystick, uint8_t switches);

#endif /* LYNXRECOMP_INPUT_H */
