/* test_state.c - save state round-trip (no game data). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lynxrecomp/state.h"
#include "lynxrecomp/mem.h"
#include "lynxrecomp/cpu.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/audio.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void) {
    lynx_mem_init(); lynx_suzy_init(); lynx_mikey_init(); lynx_timer_init();

    /* set a distinctive machine state */
    lynx_ram[0x0040] = 0xAB; lynx_ram[0xC123] = 0x5C; lynx_ram[0xFFFE] = 0x40;
    lynx_cpu.a = 0x12; lynx_cpu.x = 0x34; lynx_cpu.s = 0xF0; lynx_cpu.pc = 0x18B7; lynx_cpu.c = 1;
    lynx_suzy.reg[SUZY_VIDBASL] = 0x00; lynx_suzy.reg[SUZY_VIDBASH] = 0xC0;
    lynx_mikey.reg[MIKEY_DISPADRH] = 0xC0;
    lynx_mikey.reg[0] = 0x9E; lynx_mikey.reg[1] = TCTLA_COUNT | TCTLA_RELOAD; lynx_mikey.reg[2] = 5;
    lynx_audio_step(100);   /* evolve some audio state */

    /* snapshot */
    size_t sz = lynx_state_size();
    uint8_t *snap = malloc(sz);
    size_t n = lynx_state_save(snap, sz);
    CHECK(n == sz, "save wrote %zu of %zu bytes", n, sz);

    /* clobber everything */
    lynx_ram[0x0040] = 0; lynx_ram[0xC123] = 0; lynx_ram[0xFFFE] = 0;
    lynx_cpu.a = lynx_cpu.x = lynx_cpu.s = 0; lynx_cpu.pc = 0; lynx_cpu.c = 0;
    lynx_suzy.reg[SUZY_VIDBASH] = 0; lynx_mikey.reg[MIKEY_DISPADRH] = 0;
    lynx_mikey.reg[0] = lynx_mikey.reg[2] = 0;

    /* restore */
    CHECK(lynx_state_load(snap, sz) == 0, "load succeeded");
    CHECK(lynx_ram[0x0040] == 0xAB && lynx_ram[0xC123] == 0x5C && lynx_ram[0xFFFE] == 0x40, "RAM restored");
    CHECK(lynx_cpu.a == 0x12 && lynx_cpu.x == 0x34 && lynx_cpu.s == 0xF0 && lynx_cpu.pc == 0x18B7 && lynx_cpu.c == 1, "CPU restored");
    CHECK(lynx_suzy.reg[SUZY_VIDBASH] == 0xC0, "Suzy reg restored");
    CHECK(lynx_mikey.reg[MIKEY_DISPADRH] == 0xC0 && lynx_mikey.reg[0] == 0x9E && lynx_mikey.reg[2] == 5, "Mikey reg restored");

    /* a corrupt/short blob is rejected */
    CHECK(lynx_state_load(snap, sz - 1) != 0, "short blob rejected");
    uint8_t bad[16]; memset(bad, 0, sizeof(bad));
    CHECK(lynx_state_load(bad, sizeof(bad)) != 0, "bad-magic blob rejected");

    /* file round-trip */
    CHECK(lynx_state_save_file("state_test.sav") == 0, "save to file");
    lynx_ram[0x0040] = 0;
    CHECK(lynx_state_load_file("state_test.sav") == 0 && lynx_ram[0x0040] == 0xAB, "file load restores");

    free(snap);
    printf(fails ? "FAIL: %d\n" : "PASS: save-state round-trip\n", fails);
    return fails ? 1 : 0;
}
