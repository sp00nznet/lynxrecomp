/* test_dispatch.c - run recompiled C through a computed jump (no game data).
 *
 * The build recompiles tests/fixture_jmp.bin (seeded at the 3 handlers + main):
 *   $0200  LDX #$04 / JMP ($0210,X)     computed jump through a jump table
 *   $0210  table -> handler0/1/2 at $0220/$0228/$0230
 *   handlers: LDA #$11/$22/$33 ; STA $00 ; RTS
 * This harness registers the recompiled functions, runs lynx_func_0200, and
 * checks the computed jump dispatched to handler2 (index 2 -> writes $33).
 */
#include <stdio.h>
#include "lynxrecomp/recomp_rt.h"
#include "lynxrecomp/mem.h"
#include "recomp_funcs.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    lynx_mem_init();
    lynx_dispatch_reset();
    lynx_recomp_register();              /* generated: registers all funcs */

    /* the jump table lives in RAM at $0210 (data, not code) */
    lynx_ram[0x0210] = 0x20; lynx_ram[0x0211] = 0x02;  /* -> $0220 */
    lynx_ram[0x0212] = 0x28; lynx_ram[0x0213] = 0x02;  /* -> $0228 */
    lynx_ram[0x0214] = 0x30; lynx_ram[0x0215] = 0x02;  /* -> $0230 */

    CHECK(lynx_has_func(0x0200) && lynx_has_func(0x0230), "functions registered");

    /* run main: LDX #4 -> JMP ($0214) -> handler2 -> $00 = $33 */
    lynx_ram[0x00] = 0;
    lynx_func_0200();
    CHECK(lynx_cpu.x == 4, "LDX set X=%u", lynx_cpu.x);
    CHECK(lynx_ram[0x00] == 0x33, "computed jump dispatched to handler2: $00=%02X (exp 33)", lynx_ram[0x00]);

    /* dispatch each handler directly via lynx_call_addr */
    lynx_ram[0x00] = 0; lynx_call_addr(0x0220);
    CHECK(lynx_ram[0x00] == 0x11, "call_addr($0220)=handler0: $00=%02X", lynx_ram[0x00]);
    lynx_ram[0x00] = 0; lynx_call_addr(0x0228);
    CHECK(lynx_ram[0x00] == 0x22, "call_addr($0228)=handler1: $00=%02X", lynx_ram[0x00]);

    /* unregistered address: no-op, recorded (doesn't crash) */
    extern uint16_t lynx_last_ext_addr;
    lynx_call_addr(0x9999);
    CHECK(lynx_last_ext_addr == 0x9999, "unresolved dispatch recorded");

    printf(fails ? "FAIL: %d\n" : "PASS: dispatch (recompiled C runs through a computed jump)\n", fails);
    return fails ? 1 : 0;
}
