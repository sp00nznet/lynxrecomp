/* state.c - Lynx save states. See state.h. */
#include "lynxrecomp/state.h"
#include "lynxrecomp/mem.h"
#include "lynxrecomp/cpu.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/audio.h"
#include "lynxrecomp/serial.h"
#include <string.h>
#include <stdio.h>

#define STATE_MAGIC   "LYNXSAVE"      /* 8 bytes */
#define STATE_VERSION 2               /* v2 adds the ComLynx UART section */

/* Section sizes. Timer/audio are serialized through their modules; the rest are
 * plain globals copied wholesale. */
#define SZ_RAM    LYNX_RAM_SIZE
#define SZ_CPU    (sizeof(lynx_cpu))
#define SZ_SUZY   (sizeof(lynx_suzy))
#define SZ_MIKEY  (sizeof(lynx_mikey))
#define HDR       12                  /* magic(8) + version(1) + pad(3) */

size_t lynx_state_size(void) {
    return HDR + SZ_RAM + SZ_CPU + SZ_SUZY + SZ_MIKEY
         + lynx_timer_state_size() + lynx_audio_state_size()
         + lynx_serial_state_size();
}

size_t lynx_state_save(uint8_t *buf, size_t cap) {
    size_t need = lynx_state_size();
    if (cap < need) return 0;
    uint8_t *p = buf;
    memcpy(p, STATE_MAGIC, 8); p[8] = STATE_VERSION; p[9] = p[10] = p[11] = 0; p += HDR;
    memcpy(p, lynx_ram,   SZ_RAM);   p += SZ_RAM;
    memcpy(p, &lynx_cpu,  SZ_CPU);   p += SZ_CPU;
    memcpy(p, &lynx_suzy, SZ_SUZY);  p += SZ_SUZY;
    memcpy(p, &lynx_mikey,SZ_MIKEY); p += SZ_MIKEY;
    p += lynx_timer_state_save(p);
    p += lynx_audio_state_save(p);
    p += lynx_serial_state_save(p);
    return (size_t)(p - buf);
}

int lynx_state_load(const uint8_t *buf, size_t size) {
    if (size < lynx_state_size()) return -1;
    if (memcmp(buf, STATE_MAGIC, 8) != 0 || buf[8] != STATE_VERSION) return -1;
    const uint8_t *p = buf + HDR;
    memcpy(lynx_ram,   p, SZ_RAM);   p += SZ_RAM;
    memcpy(&lynx_cpu,  p, SZ_CPU);   p += SZ_CPU;
    memcpy(&lynx_suzy, p, SZ_SUZY);  p += SZ_SUZY;
    memcpy(&lynx_mikey,p, SZ_MIKEY); p += SZ_MIKEY;
    p += lynx_timer_state_load(p);
    p += lynx_audio_state_load(p);
    p += lynx_serial_state_load(p);
    return 0;
}

int lynx_state_save_file(const char *path) {
    static uint8_t buf[80 * 1024];
    size_t n = lynx_state_save(buf, sizeof(buf));
    if (!n) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = fwrite(buf, 1, n, f) == n;
    fclose(f);
    return ok ? 0 : -1;
}

int lynx_state_load_file(const char *path) {
    static uint8_t buf[80 * 1024];
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return lynx_state_load(buf, n);
}
