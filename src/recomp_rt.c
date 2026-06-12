/* recomp_rt.c - defaults for the recompiler's control-transfer hooks.
 *
 * The arithmetic/flag helpers are all inline in recomp_rt.h. This file only
 * holds the overridable hooks the emitter calls when control leaves the
 * recompiled image: a boot-ROM call, a jump out of the image, a computed
 * indirect jump (e.g. JMP ($004E) into the game entry), and an opcode the
 * emitter hasn't translated yet. Defaults record the last event so a test or
 * host can observe it; a host can replace any hook. */
#include "lynxrecomp/recomp_rt.h"

/* Last external-control event, for tests/diagnostics. */
uint16_t lynx_last_ext_addr = 0;
uint8_t  lynx_last_unimpl   = 0;

static void default_boot_call(uint16_t addr)    { lynx_last_ext_addr = addr; }
static void default_ext_jmp(uint16_t addr)      { lynx_last_ext_addr = addr; }
static void default_jmp_indirect(uint16_t addr) { lynx_last_ext_addr = addr; }
static void default_unimpl(uint8_t opcode)      { lynx_last_unimpl = opcode; }

void (*lynx_hook_boot_call)(uint16_t)    = default_boot_call;
void (*lynx_hook_ext_jmp)(uint16_t)      = default_ext_jmp;
void (*lynx_hook_jmp_indirect)(uint16_t) = default_jmp_indirect;
void (*lynx_hook_unimpl)(uint8_t)        = default_unimpl;

void lynx_boot_call(uint16_t addr)    { lynx_hook_boot_call(addr); }
void lynx_ext_jmp(uint16_t addr)      { lynx_hook_ext_jmp(addr); }
void lynx_jmp_indirect(uint16_t addr) { lynx_hook_jmp_indirect(addr); }
void lynx_unimpl(uint8_t opcode)      { lynx_hook_unimpl(opcode); }
