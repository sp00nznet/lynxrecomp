/* lynxrun - execution driver: run a Lynx game against the real runtime.
 *
 * Boots the cart with the shared interpreter (interp.c) over the real boot ROM
 * + cart-read model, but routes Suzy/Mikey to the tested runtime peripherals so
 * the blitter, timers and video actually run. Steps time, delivers the Mikey
 * timer interrupt into the game's RAM handler, and presents the framebuffer.
 *
 * Modes:
 *   lynxrun <cart.lnx> <boot.img> <out.ppm> [maxInsns] [traceN] [traceAtIRQ]
 *       headless: run a budget, write one frame.
 *   lynxrun --capture <cart.lnx> <boot.img> <outdir> [nframes] [stride] [btnHex] [atFrame] [holdFrames]
 *       run continuously, dump a PPM per display flip (optionally injecting a
 *       scripted joystick press) - proves the game runs frame-by-frame + input.
 *   lynxrun --play <cart.lnx> <boot.img>           (Windows: live window)
 *
 * See docs/RUN.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnx.h"
#include "interp.h"
#include "lynxrecomp/mem.h"
#include "lynxrecomp/suzy.h"
#include "lynxrecomp/mikey.h"
#include "lynxrecomp/timer.h"
#include "lynxrecomp/input.h"
#include "lynxrecomp/audio.h"

static uint8_t  bootrom[512];
static const uint8_t *cart;
static size_t   cart_size;
static unsigned pagesize = 2048;
static uint8_t  mapctl = 0;
static unsigned cart_block = 0, cart_pos = 0;
static uint8_t  iodat = 0;
static int      strobe_prev = 0;
static int      g_flip = 0;       /* set when DISPADR ($FD94/95) is written */
static long     g_irqs = 0;

static uint8_t cart_read(void) {
    size_t a = (size_t)cart_block * pagesize + cart_pos;
    cart_pos++;
    return (a < cart_size) ? cart[a] : 0xFF;
}

uint8_t bus_read(uint16_t addr) {
    if (addr == 0xFCB2) return cart_read();
    if (addr == 0xFFF9) return mapctl;
    if (addr >= 0xFE00) {
        int vectors = (addr >= 0xFFFA);
        int as_ram = vectors ? (mapctl & 0x08) : (mapctl & 0x04);
        if (!as_ram && addr != 0xFFF8) return bootrom[addr - 0xFE00];
    }
    return lynx_mem_read(addr);
}

void bus_write(uint16_t addr, uint8_t v) {
    if (addr >= 0xFC00 && addr <= 0xFDFF) {
        if (addr == 0xFD8B) iodat = v;
        else if (addr == 0xFD87) {
            int strobe = v & 1;
            if (strobe && !strobe_prev) {
                cart_block = ((cart_block << 1) | ((iodat >> 1) & 1)) & 0xFF;
                cart_pos = 0;
            }
            strobe_prev = strobe;
        }
        else if (addr == 0xFD94 || addr == 0xFD95) g_flip = 1;  /* display swap */
        lynx_mem_write(addr, v);
        return;
    }
    if (addr == 0xFFF9) { mapctl = v; lynx_mem_write(addr, v); return; }
    lynx_mem_write(addr, v);
}

static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f=fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long s0=ftell(f); fseek(f,0,SEEK_SET);
    if(s0<=0){fclose(f);return NULL;}
    uint8_t *b=malloc((size_t)s0);
    if(fread(b,1,(size_t)s0,f)!=(size_t)s0){free(b);fclose(f);return NULL;}
    fclose(f); *n=(size_t)s0; return b;
}

/* Boot the machine; leaves PC at the boot ROM reset vector. Returns 0 ok. */
static int setup(const char *cartpath, const char *bootpath) {
    size_t csz=0, bsz=0;
    uint8_t *cdata = read_file(cartpath, &csz);
    uint8_t *bdata = read_file(bootpath, &bsz);
    if (!cdata || !bdata || bsz < 512) { fprintf(stderr, "load error\n"); return -1; }
    static lnx_info_t info; lnx_parse(cdata, csz, &info);
    cart = info.rom; cart_size = info.rom_size;
    if (info.valid && info.page_size_bank0) pagesize = info.page_size_bank0;
    memcpy(bootrom, bdata, 512);

    lynx_mem_init(); lynx_suzy_init(); lynx_mikey_init(); lynx_timer_init();
    lynx_input_set(0x00, 0x00);
    mapctl = 0; cart_block = cart_pos = 0; strobe_prev = 0; g_flip = 0; g_irqs = 0;
    interp_reset_pc((uint16_t)(bus_read(0xFFFC) | (bus_read(0xFFFD) << 8)));
    return 0;
}

/* Run until the next display flip (or the budget). Returns instructions run. */
static long run_until_flip(long budget) {
    long i = 0;
    g_flip = 0;
    for (; i < budget; i++) {
        if (interp_step() < 0) break;
        lynx_timer_step(1);                       /* ~1us / instruction */
        if (lynx_irq_pending() && !icpu.i) { interp_irq(); g_irqs++; }
        if (g_flip) { i++; break; }
    }
    return i;
}

/* ---- headless: run a budget, write one frame ---- */
static int run_headless(int argc, char **argv) {
    long maxi = (argc > 4) ? strtol(argv[4], NULL, 0) : 40000000L;
    interp_trace = (argc > 5) ? strtol(argv[5], NULL, 0) : 0;
    long trace_at = (argc > 6) ? strtol(argv[6], NULL, 0) : 0;
    printf("reset -> $%04X, pagesize %u\n", icpu.pc, pagesize);
    long i = 0; uint16_t entry = 0; long entry_at = -1;
    for (; i < maxi; i++) {
        if (interp_step() < 0) break;
        if (interp_event == EV_INDJMP && entry_at < 0) { entry_at = i; entry = interp_event_addr; }
        lynx_timer_step(1);
        if (lynx_irq_pending() && !icpu.i) {
            interp_irq(); g_irqs++;
            if (g_irqs == trace_at && interp_trace == 0 && argc > 5) interp_trace = strtol(argv[5], NULL, 0);
        }
    }
    printf("ran %ld insns, game entry $%04X at insn %ld, %ld IRQs, pc=$%04X\n",
           i, entry, entry_at, g_irqs, icpu.pc);
    if (lynx_video_write_ppm(argv[3]) == 0) printf("wrote frame -> %s\n", argv[3]);
    return 0;
}

/* ---- capture: dump a PPM sequence, with a scripted joystick press ---- */
static int run_capture(const char *outdir, int nframes, int stride,
                       uint8_t btn, int at_frame, int hold) {
    char path[1024];
    printf("capture: %d frames (stride %d) -> %s/  press $%02X @frame %d for %d\n",
           nframes, stride, outdir, btn, at_frame, hold);
    int dumped = 0;
    for (int f = 0; f < nframes; f++) {
        uint8_t joy = (btn && f >= at_frame && f < at_frame + hold) ? btn : 0x00;
        lynx_input_set(joy, 0x00);
        run_until_flip(2000000);                 /* one displayed frame */
        if ((f % stride) == 0) {
            snprintf(path, sizeof(path), "%s/frame_%03d.ppm", outdir, dumped);
            lynx_video_write_ppm(path);
            dumped++;
        }
    }
    printf("dumped %d frames, %ld IRQs total\n", dumped, g_irqs);
    return 0;
}

#ifdef _WIN32
#include <windows.h>
static volatile int g_quit = 0;
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE || m == WM_DESTROY) { g_quit = 1; PostQuitMessage(0); return 0; }
    return DefWindowProc(h, m, w, l);
}
static int run_play(void) {
    const int S = 4, W = LYNX_SCREEN_W, H = LYNX_SCREEN_H;
    WNDCLASS wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "lynxrun"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    RECT r = { 0, 0, W * S, H * S };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindow("lynxrun", "lynxrun - Lynx",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 1;

    BITMAPINFO bmi; memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W; bmi.bmiHeader.biHeight = -H;   /* top-down */
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    static uint32_t fb[LYNX_SCREEN_W * LYNX_SCREEN_H];

    printf("play: arrows = D-pad, Z = A, X = B, A/S = Option1/2, Enter = Pause\n");
    while (!g_quit) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        uint8_t joy = 0, sw = 0;
        if (GetAsyncKeyState(VK_UP)    & 0x8000) joy |= LYNX_BTN_UP;
        if (GetAsyncKeyState(VK_DOWN)  & 0x8000) joy |= LYNX_BTN_DOWN;
        if (GetAsyncKeyState(VK_LEFT)  & 0x8000) joy |= LYNX_BTN_LEFT;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) joy |= LYNX_BTN_RIGHT;
        if (GetAsyncKeyState('Z')      & 0x8000) joy |= LYNX_BTN_A;
        if (GetAsyncKeyState('X')      & 0x8000) joy |= LYNX_BTN_B;
        if (GetAsyncKeyState('A')      & 0x8000) joy |= LYNX_BTN_OPTION1;
        if (GetAsyncKeyState('S')      & 0x8000) joy |= LYNX_BTN_OPTION2;
        if (GetAsyncKeyState(VK_RETURN)& 0x8000) sw  |= LYNX_SW_PAUSE;
        lynx_input_set(joy, sw);

        run_until_flip(2000000);
        lynx_video_render(fb);
        HDC dc = GetDC(hwnd);
        StretchDIBits(dc, 0, 0, W * S, H * S, 0, 0, W, H, fb, &bmi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd, dc);
        Sleep(15);
    }
    return 0;
}
#else
static int run_play(void) { fprintf(stderr, "--play needs Windows; use --capture or headless.\n"); return 1; }
#endif

/* ---- snapshot: run N frames, then dump the full 64 KiB RAM ---- */
static int run_snapshot(const char *outbin, int nframes) {
    for (int f = 0; f < nframes; f++) { lynx_input_set(0, 0); run_until_flip(2000000); }
    FILE *o = fopen(outbin, "wb");
    if (!o) { fprintf(stderr, "cannot write %s\n", outbin); return 1; }
    fwrite(lynx_ram, 1, 0x10000, o);
    fclose(o);
    printf("ran %d frames (%ld IRQs), dumped post-init RAM -> %s\n", nframes, g_irqs, outbin);
    printf("  IRQ vector $FFFE=$%04X  jump table $1897: ",
           (unsigned)(lynx_ram[0xFFFE] | (lynx_ram[0xFFFF] << 8)));
    for (int i = 0; i < 8; i++)
        printf("$%04X ", (unsigned)(lynx_ram[0x1897 + i*2] | (lynx_ram[0x1898 + i*2] << 8)));
    printf("\n");
    return 0;
}

/* ---- audio: run the game and capture the mix to a WAV ---- */
#define AUDIO_RATE 44100
static int run_audio(const char *outwav, double seconds) {
    long total = (long)(seconds * 1000000.0);     /* 1 us per instruction */
    int  cap   = (int)(seconds * AUDIO_RATE) + 16;
    int16_t *pcm = (int16_t *)malloc((size_t)cap * sizeof(int16_t));
    if (!pcm) return 1;
    long ns = 0, sacc = 0;
    for (long i = 0; i < total && ns < cap; i++) {
        if (interp_step() < 0) break;
        lynx_timer_step(1);
        lynx_audio_step(1);
        if (lynx_irq_pending() && !icpu.i) { interp_irq(); g_irqs++; }
        sacc += AUDIO_RATE;                        /* emit a sample every 1e6/RATE us */
        if (sacc >= 1000000) { sacc -= 1000000; pcm[ns++] = lynx_audio_sample(); }
    }
    /* simple loudness stat */
    long peak = 0, nz = 0;
    for (int i = 0; i < ns; i++) { int a = pcm[i] < 0 ? -pcm[i] : pcm[i]; if (a > peak) peak = a; if (pcm[i]) nz++; }
    printf("captured %ld samples (%.1fs @ %dHz), peak %ld, %ld nonzero, %ld IRQs\n",
           ns, (double)ns / AUDIO_RATE, AUDIO_RATE, peak, nz, g_irqs);
    int rc = lynx_audio_write_wav(outwav, pcm, (int)ns, AUDIO_RATE);
    if (rc == 0) printf("wrote audio -> %s\n", outwav);
    free(pcm);
    return rc;
}

int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "--audio")) {
        if (setup(argv[2], argv[3]) != 0) return 1;
        double secs = (argc > 5) ? atof(argv[5]) : 4.0;
        return run_audio(argv[4], secs);
    }
    if (argc >= 5 && !strcmp(argv[1], "--snapshot")) {
        if (setup(argv[2], argv[3]) != 0) return 1;
        int nframes = (argc > 5) ? atoi(argv[5]) : 60;
        return run_snapshot(argv[4], nframes);
    }
    if (argc >= 4 && !strcmp(argv[1], "--capture")) {
        if (setup(argv[2], argv[3]) != 0) return 1;
        const char *outdir = (argc > 4) ? argv[4] : ".";
        int nframes = (argc > 5) ? atoi(argv[5]) : 120;
        int stride  = (argc > 6) ? atoi(argv[6]) : 8;
        uint8_t btn = (argc > 7) ? (uint8_t)strtoul(argv[7], NULL, 0) : 0;
        int atf     = (argc > 8) ? atoi(argv[8]) : 0;
        int hold    = (argc > 9) ? atoi(argv[9]) : 0;
        return run_capture(outdir, nframes, stride, btn, atf, hold);
    }
    if (argc >= 4 && !strcmp(argv[1], "--play")) {
        if (setup(argv[2], argv[3]) != 0) return 1;
        return run_play();
    }
    if (argc < 4) {
        fprintf(stderr,
            "usage:\n"
            "  %s <cart.lnx> <boot.img> <out.ppm> [maxInsns] [traceN] [traceAtIRQ]\n"
            "  %s --capture <cart.lnx> <boot.img> <outdir> [nframes] [stride] [btnHex] [atFrame] [holdFrames]\n"
            "  %s --snapshot <cart.lnx> <boot.img> <out.bin> [nframes]\n"
            "  %s --audio <cart.lnx> <boot.img> <out.wav> [seconds]\n"
            "  %s --play <cart.lnx> <boot.img>\n", argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 2;
    }
    if (setup(argv[1], argv[2]) != 0) return 1;
    return run_headless(argc, argv);
}
