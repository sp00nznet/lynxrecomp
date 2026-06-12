/*
 * platform_sdl.c - SDL2 window / renderer / audio / input for lynxrecomp.
 * See lynxrecomp/platform.h. Mirrors snesrecomp's platform_sdl, adapted to the
 * Lynx's 160x102 display, rotation, and JOYSTICK/SWITCHES input.
 */
#include "lynxrecomp/platform.h"
#include "lynxrecomp/menu_overlay.h"
#include "lynxrecomp/input.h"
#include "lynxrecomp/mikey.h"   /* LYNX_SCREEN_W / LYNX_SCREEN_H */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define AUDIO_RATE 44100

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture  *s_texture  = NULL;
static SDL_AudioDeviceID s_audio = 0;
static SDL_GameController *s_pad[2] = { NULL, NULL };
static uint64_t s_frame_start = 0;
static int  s_scale  = 4;
static int  s_filter = -1;
static int  s_rotate = 0;
static bool s_headless = false;

/* Computed by the last poll(): the local player-1 input. The single-player path
 * applies it directly; the netplay runner reads it and exchanges instead. */
static uint8_t s_local_joy = 0, s_local_sw = 0;

/* Map a Lynx menu-button index to its JOYSTICK ($FCB0) bit (0 for Pause, which
 * lives in SWITCHES). Indexed by LYNX_MENU_BTN_*. */
static uint8_t joy_bit(int b) {
    switch (b) {
        case LYNX_MENU_BTN_A:       return LYNX_BTN_A;
        case LYNX_MENU_BTN_B:       return LYNX_BTN_B;
        case LYNX_MENU_BTN_OPTION1: return LYNX_BTN_OPTION1;
        case LYNX_MENU_BTN_OPTION2: return LYNX_BTN_OPTION2;
        case LYNX_MENU_BTN_UP:      return LYNX_BTN_UP;
        case LYNX_MENU_BTN_DOWN:    return LYNX_BTN_DOWN;
        case LYNX_MENU_BTN_LEFT:    return LYNX_BTN_LEFT;
        case LYNX_MENU_BTN_RIGHT:   return LYNX_BTN_RIGHT;
        default:                    return 0;   /* Pause -> switches */
    }
}

static void recreate_texture(int filter) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter ? "1" : "0");
    if (s_texture) SDL_DestroyTexture(s_texture);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, LYNX_SCREEN_W, LYNX_SCREEN_H);
    s_filter = filter;
}

/* Window size for the current scale + rotation (90/270 swaps W/H). */
static void window_size_for(int scale, int rotate, int *w, int *h) {
    int gw = LYNX_SCREEN_W * scale, gh = LYNX_SCREEN_H * scale;
    if (rotate == 1 || rotate == 3) { *w = gh; *h = gw; }
    else                            { *w = gw; *h = gh; }
}

bool lynx_platform_init(const char *title, int scale) {
    if (getenv("LYNX_HEADLESS")) { s_headless = true; return true; }
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO |
                 SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    int win_w, win_h;
    window_size_for(scale, 0, &win_w, &win_h);
    s_window = SDL_CreateWindow(title ? title : "lynxrecomp",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!s_window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return false; }

    s_renderer = SDL_CreateRenderer(s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_renderer) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
                       SDL_DestroyWindow(s_window); SDL_Quit(); return false; }

    recreate_texture(1);
    if (!s_texture) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
                      SDL_DestroyRenderer(s_renderer); SDL_DestroyWindow(s_window); SDL_Quit(); return false; }

    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 1024; want.callback = NULL;   /* push mode */
    s_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio > 0) SDL_PauseAudioDevice(s_audio, 0);

    /* Adopt the first two connected gamepads (P1, P2). */
    for (int i = 0, p = 0; i < SDL_NumJoysticks() && p < 2; i++)
        if (SDL_IsGameController(i)) s_pad[p++] = SDL_GameControllerOpen(i);

    s_scale = scale;
    s_filter = 1;
    s_frame_start = SDL_GetPerformanceCounter();

    /* ImGui menu (no-op when disabled). Adopt persisted scale/filter/rotate. */
    menu_overlay_init(s_window, s_renderer);
    int ms = menu_overlay_get_scale();
    s_rotate = menu_overlay_get_rotate();
    if (ms != s_scale || s_rotate != 0) {
        s_scale = ms;
        window_size_for(s_scale, s_rotate, &win_w, &win_h);
        SDL_SetWindowSize(s_window, win_w, win_h);
    }
    if (menu_overlay_get_filter() != s_filter) recreate_texture(menu_overlay_get_filter());
    return true;
}

void lynx_platform_present(const uint32_t *rgba) {
    if (s_headless || !s_texture || !rgba) return;

    int want_scale = menu_overlay_get_scale();
    int want_rot   = menu_overlay_get_rotate();
    if (want_scale != s_scale || want_rot != s_rotate) {
        s_scale = want_scale; s_rotate = want_rot;
        int w, h; window_size_for(s_scale, s_rotate, &w, &h);
        SDL_SetWindowSize(s_window, w, h);
    }
    int want_filter = menu_overlay_get_filter();
    if (want_filter != s_filter) recreate_texture(want_filter);

    SDL_UpdateTexture(s_texture, NULL, rgba, LYNX_SCREEN_W * 4);

    /* Place the game below the menu bar so the menu never covers the picture. */
    int menu_h = menu_overlay_get_menubar_height();
    int out_w = 0, out_h = 0;
    SDL_GetRendererOutputSize(s_renderer, &out_w, &out_h);
    SDL_Rect dst = { 0, menu_h, out_w, out_h - menu_h };

    int br = 0, bg = 0, bb = 0;
    if (menu_h > 0) menu_overlay_get_bar_color(&br, &bg, &bb);
    SDL_SetRenderDrawColor(s_renderer, (Uint8)br, (Uint8)bg, (Uint8)bb, 255);
    SDL_RenderClear(s_renderer);

    /* Rotation: 90/270 deg about the picture centre. */
    double angle = s_rotate * 90.0;
    SDL_RenderCopyEx(s_renderer, s_texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);

    menu_overlay_render(s_renderer);
    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
    SDL_RenderPresent(s_renderer);
}

void lynx_platform_queue_audio(const int16_t *pcm, int nsamples) {
    if (s_headless || s_audio <= 0 || !pcm || nsamples <= 0) return;
    if (SDL_GetQueuedAudioSize(s_audio) > (Uint32)(AUDIO_RATE * 2 * 4)) return;  /* ~4 frames cap */
    float vol = menu_overlay_get_volume();
    if (vol >= 0.999f) {
        SDL_QueueAudio(s_audio, pcm, (Uint32)(nsamples * (int)sizeof(int16_t)));
    } else {
        static int16_t scaled[8192];
        int n = nsamples > 8192 ? 8192 : nsamples;
        for (int i = 0; i < n; i++) scaled[i] = (int16_t)((int)pcm[i] * vol);
        SDL_QueueAudio(s_audio, scaled, (Uint32)(n * (int)sizeof(int16_t)));
    }
}

/* Compute one player's input from the keyboard + gamepad bindings. */
static void read_player(int player, uint8_t *joy, uint8_t *sw) {
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    SDL_GameController *pad = s_pad[player - 1];
    uint8_t j = 0, s = 0;
    for (int b = 0; b < LYNX_MENU_BTN_COUNT; b++) {
        int pressed = 0;
        int sc = menu_overlay_key_for_button(player, b);
        if (sc >= 0 && sc < SDL_NUM_SCANCODES && ks[sc]) pressed = 1;
        if (!pressed && pad) {
            int pb = menu_overlay_pad_button_for_button(player, b);
            if (pb >= 0 && SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)pb)) pressed = 1;
        }
        if (!pressed) continue;
        if (b == LYNX_MENU_BTN_PAUSE) s |= LYNX_SW_PAUSE;
        else                          j |= joy_bit(b);
    }
    *joy = j; *sw = s;
}

bool lynx_platform_poll(void) {
    if (s_headless) return true;
    SDL_Event ev;
    bool keep = true;
    while (SDL_PollEvent(&ev)) {
        int consumed = menu_overlay_process_event(&ev);
        if (ev.type == SDL_QUIT) keep = false;
        else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
            for (int p = 0; p < 2; p++)
                if (!s_pad[p]) { s_pad[p] = SDL_GameControllerOpen(ev.cdevice.which); break; }
        }
        else if (ev.type == SDL_KEYDOWN && !consumed) {
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_ESCAPE) keep = false;
        }
    }
    /* Player-1 input for the single-player path (the netplay runner overrides). */
    if (menu_overlay_is_active()) { s_local_joy = 0; s_local_sw = 0; }
    else read_player(1, &s_local_joy, &s_local_sw);
    lynx_input_set(s_local_joy, s_local_sw);
    if (menu_overlay_quit_requested()) keep = false;
    return keep;
}

void lynx_platform_frame_sync(void) {
    if (s_headless) return;
    const double FRAME_MS = 1000.0 / 60.0;
    uint64_t now = SDL_GetPerformanceCounter();
    double elapsed = (double)(now - s_frame_start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    double remaining = FRAME_MS - elapsed;
    if (remaining > 1.0) SDL_Delay((uint32_t)(remaining - 0.5));
    s_frame_start = SDL_GetPerformanceCounter();
}

int lynx_platform_audio_rate(void) { return AUDIO_RATE; }

/* Exposed for the netplay runner: the most recent locally-read input. */
uint8_t lynx_platform_local_joystick(void) { return s_local_joy; }
uint8_t lynx_platform_local_switches(void) { return s_local_sw; }

void lynx_platform_shutdown(void) {
    if (s_headless) return;
    menu_overlay_shutdown();
    for (int p = 0; p < 2; p++) if (s_pad[p]) SDL_GameControllerClose(s_pad[p]);
    if (s_audio > 0)  SDL_CloseAudioDevice(s_audio);
    if (s_texture)    SDL_DestroyTexture(s_texture);
    if (s_renderer)   SDL_DestroyRenderer(s_renderer);
    if (s_window)     SDL_DestroyWindow(s_window);
    SDL_Quit();
    s_texture = NULL; s_renderer = NULL; s_window = NULL;
}
