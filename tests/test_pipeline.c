/* test_pipeline.c - end-to-end recompiler proof on a synthetic fixture.
 *
 * The build recompiles tests/fixture.bin to generated/recomp_funcs.{c,h} via
 * `m65c02recomp recompbin`. This harness executes the recompiled function and
 * checks it had the exact effect the original 65SC02 routine would:
 *   LDX #3 / STZ $FD00,X (x4) / LDA #$AA / STA $0050 / INC $0050  ->  $0050 = $AB
 * and the four Mikey timer regs $FD00-$FD03 cleared.
 */
#include <stdio.h>
#include "lynxrecomp/recomp_rt.h"
#include "lynxrecomp/mikey.h"
#include "recomp_funcs.h"

int main(void) {
    lynx_mem_init();
    lynx_mikey_init();
    for (int i = 0; i < 4; i++) lynx_mikey.reg[i] = 0xFF;   /* poison $FD00-$FD03 */
    lynx_ram[0x0050] = 0x00;
    lynx_cpu_reset();

    lynx_func_0200();   /* recompiled 65SC02 running as native C */

    int ok = 1;
    for (int i = 0; i < 4; i++)
        if (lynx_mikey.reg[i] != 0) { ok = 0; printf("  $FD0%X = %02X (expected 00)\n", i, lynx_mikey.reg[i]); }
    if (lynx_ram[0x0050] != 0xAB) { ok = 0; printf("  $0050 = %02X (expected AB)\n", lynx_ram[0x0050]); }

    printf(ok ? "PASS: pipeline (fixture.bin -> C -> executed) correct\n" : "FAIL\n");
    return ok ? 0 : 1;
}
