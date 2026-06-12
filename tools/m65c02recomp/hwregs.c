/* hwregs.c - Lynx hardware register names. See hwregs.h. */
#include "hwregs.h"
#include <stdio.h>

/* Returned to callers in a small rotating set of static buffers so a single
 * printf can use a couple of these at once. */
static const char *buf(const char *s) {
    static char ring[4][24];
    static int i = 0;
    i = (i + 1) & 3;
    snprintf(ring[i], sizeof(ring[i]), "%s", s);
    return ring[i];
}

const char *lynx_reg_name(uint16_t a) {
    /* --- Suzy $FC00-$FCFF --- */
    if (a >= 0xFC00 && a <= 0xFCFF) {
        switch (a) {
            case 0xFC00: return "TMPADRL";  case 0xFC01: return "TMPADRH";
            case 0xFC04: return "HOFFL";    case 0xFC05: return "HOFFH";
            case 0xFC06: return "VOFFL";    case 0xFC07: return "VOFFH";
            case 0xFC08: return "VIDBASL";  case 0xFC09: return "VIDBASH";
            case 0xFC0A: return "COLLBASL"; case 0xFC0B: return "COLLBASH";
            case 0xFC0C: return "VIDADRL";  case 0xFC0E: return "COLLADRL";
            case 0xFC10: return "SCBNEXTL"; case 0xFC11: return "SCBNEXTH";
            case 0xFC12: return "SPRDLINEL";case 0xFC14: return "HPOSSTRTL";
            case 0xFC16: return "VPOSSTRTL";case 0xFC18: return "SPRHSIZL";
            case 0xFC1A: return "SPRVSIZL"; case 0xFC1C: return "STRETCHL";
            case 0xFC1E: return "TILTL";    case 0xFC20: return "SPRDOFFL";
            case 0xFC22: return "SPRVPOSL"; case 0xFC52: return "MATHD";
            case 0xFC53: return "MATHC";    case 0xFC54: return "MATHB";
            case 0xFC55: return "MATHA";    case 0xFC56: return "MATHP";
            case 0xFC57: return "MATHN";    case 0xFC60: return "MATHH";
            case 0xFC61: return "MATHG";    case 0xFC62: return "MATHF";
            case 0xFC63: return "MATHE";    case 0xFC6C: return "MATHM";
            case 0xFC6E: return "MATHK";    case 0xFC80: return "SPRCTL0";
            case 0xFC81: return "SPRCTL1";  case 0xFC82: return "SPRCOLL";
            case 0xFC83: return "SPRINIT";  case 0xFC88: return "SUZYHREV";
            case 0xFC90: return "SUZYBUSEN";case 0xFC91: return "SPRGO";
            case 0xFC92: return "SPRSYS";   case 0xFCB0: return "JOYSTICK";
            case 0xFCB1: return "SWITCHES"; case 0xFCB2: return "RCART0";
            case 0xFCB3: return "RCART1";   case 0xFCC0: return "LEDS";
            default: return 0;
        }
    }
    /* --- Mikey $FD00-$FDFF --- */
    if (a >= 0xFD00 && a <= 0xFDFF) {
        uint8_t o = (uint8_t)(a & 0xFF);
        if (o <= 0x1F) {                              /* 8 timers x 4 bytes */
            static const char *fld[4] = { "BKUP", "CTLA", "CNT", "CTLB" };
            static char t[16]; snprintf(t, sizeof(t), "TIM%d%s", o >> 2, fld[o & 3]);
            return buf(t);
        }
        if (o >= 0x20 && o <= 0x3F) {                 /* 4 audio channels x 8 */
            static const char *fld[8] = { "VOL","FEED","DAC","SHIFTLO","RELOAD","CTL","CNT","MISC" };
            static char t[16]; snprintf(t, sizeof(t), "AUD%c%s", 'A' + ((o - 0x20) >> 3), fld[o & 7]);
            return buf(t);
        }
        if (o >= 0xA0 && o <= 0xAF) { static char t[12]; snprintf(t,sizeof(t),"GREEN%X",o&0xF); return buf(t); }
        if (o >= 0xB0 && o <= 0xBF) { static char t[12]; snprintf(t,sizeof(t),"BLUERED%X",o&0xF); return buf(t); }
        switch (a) {
            case 0xFD40: return "ATTENA"; case 0xFD41: return "ATTENB";
            case 0xFD42: return "ATTENC"; case 0xFD43: return "ATTEND";
            case 0xFD44: return "MPAN";   case 0xFD50: return "MSTEREO";
            case 0xFD80: return "INTRST"; case 0xFD81: return "INTSET";
            case 0xFD86: return "AUDIN";  case 0xFD87: return "SYSCTL1";
            case 0xFD8A: return "IODIR";  case 0xFD8B: return "IODAT";
            case 0xFD8C: return "SERCTL"; case 0xFD8D: return "SERDAT";
            case 0xFD90: return "SDONEACK";case 0xFD91: return "CPUSLEEP";
            case 0xFD92: return "DISPCTL";case 0xFD93: return "PBKUP";
            case 0xFD94: return "DISPADRL";case 0xFD95: return "DISPADRH";
            default: return 0;
        }
    }
    if (a == 0xFFF9) return "MAPCTL";
    if (a == 0xFFFA) return "NMI_VEC_L"; if (a == 0xFFFB) return "NMI_VEC_H";
    if (a == 0xFFFC) return "RESET_VEC_L"; if (a == 0xFFFD) return "RESET_VEC_H";
    if (a == 0xFFFE) return "IRQ_VEC_L"; if (a == 0xFFFF) return "IRQ_VEC_H";
    return 0;
}

const char *lynx_reg_note(uint16_t a) {
    switch (a) {
        case 0xFC91: return "start blitter";
        case 0xFC10: case 0xFC11: return "first SCB ptr";
        case 0xFC08: case 0xFC09: return "video buffer base";
        case 0xFCB0: return "D-pad + buttons";
        case 0xFD80: return "ack interrupts";
        case 0xFD81: return "pending interrupts";
        case 0xFD92: return "video DMA enable/flip";
        case 0xFD94: case 0xFD95: return "framebuffer base (display)";
        case 0xFFF9: return "map hardware vs RAM";
        case 0xFFFE: case 0xFFFF: return "IRQ handler addr";
        default: return 0;
    }
}
