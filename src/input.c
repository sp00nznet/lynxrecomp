/* input.c - host input -> Suzy JOYSTICK/SWITCHES bytes. See input.h. */
#include "lynxrecomp/input.h"

static uint8_t s_joystick = 0;
static uint8_t s_switches = 0;

void lynx_input_set(uint8_t joystick, uint8_t switches) {
    s_joystick = joystick;
    s_switches = switches;
}

uint8_t lynx_input_joystick(void) { return s_joystick; }
uint8_t lynx_input_switches(void) { return s_switches; }
