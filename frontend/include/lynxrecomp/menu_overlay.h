/*
 * menu_overlay - Dear ImGui menu system for the lynxrecomp launcher.
 *
 * A persistent main menu bar (File / Graphics / Sound / Controller /
 * Multiplayer / Help) drawn over the running game, modelled on snesrecomp's
 * menu_overlay but with the Lynx's controls and hardware. Supports keyboard
 * AND gamepad (Xbox-style SDL_GameController) bindings for both ComLynx
 * players.
 *
 * The C++ implementation (menu_overlay.cpp) exposes this C API so the C SDL
 * platform layer can drive it. A no-op stub (menu_overlay_stub.c) is compiled
 * instead when the menu is disabled, so a frontend without Dear ImGui still
 * links. Disabled automatically in headless/scripted runs (LYNX_HEADLESS) so
 * it never adds overhead or intercepts input during automated capture.
 */
#ifndef LYNXRECOMP_MENU_OVERLAY_H
#define LYNXRECOMP_MENU_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_Window;
struct SDL_Renderer;

/* The Lynx button set, in the bit order of the JOYSTICK/SWITCHES registers.
 * These index the keyboard/gamepad binding tables. Pause lives in SWITCHES
 * ($FCB1); the rest are JOYSTICK ($FCB0). See lynxrecomp/input.h. */
enum {
    LYNX_MENU_BTN_A = 0, LYNX_MENU_BTN_B,
    LYNX_MENU_BTN_OPTION1, LYNX_MENU_BTN_OPTION2,
    LYNX_MENU_BTN_UP, LYNX_MENU_BTN_DOWN, LYNX_MENU_BTN_LEFT, LYNX_MENU_BTN_RIGHT,
    LYNX_MENU_BTN_PAUSE,
    LYNX_MENU_BTN_COUNT
};

/* Lifecycle. init() is a no-op (stays disabled) when LYNX_HEADLESS is set or
 * the window/renderer are NULL. All other calls are safe when disabled. */
void menu_overlay_init(struct SDL_Window *window, struct SDL_Renderer *renderer);
void menu_overlay_shutdown(void);

/* Feed an SDL_Event (void* to keep this header SDL-free).
 * Returns 1 if the menu captured the event (game should ignore it). */
int  menu_overlay_process_event(const void *sdl_event);

/* Build + render the menu for this frame (after the game frame is copied to the
 * renderer, before SDL_RenderPresent). */
void menu_overlay_render(struct SDL_Renderer *renderer);

/* 1 while the menu is capturing input - the game must ignore keyboard/pad. */
int  menu_overlay_is_active(void);

/* Height in pixels of the main menu bar (0 when disabled). The platform draws
 * the game below this so the menu never covers the top of the picture. */
int  menu_overlay_get_menubar_height(void);

/* Menu-bar background colour (0-255 RGB; 0,0,0 when disabled). The platform
 * clears the frame to this so any strip the bar doesn't paint matches it. */
void menu_overlay_get_bar_color(int *r, int *g, int *b);

/* Set by File -> Quit. */
int  menu_overlay_quit_requested(void);

/* One-shot emulator-control requests from the File menu (return 1 once and
 * clear). The host consumes these between frames:
 *   Restart    -> reload the RAM image and re-enter the game
 *   Save State -> lynx_state_save_file()
 *   Load State -> lynx_state_load_file()
 */
int  menu_overlay_take_reset(void);
int  menu_overlay_take_save_state(void);
int  menu_overlay_take_load_state(void);

/* ---- Settings accessors (polled by the platform each frame) ---- */
int   menu_overlay_get_scale(void);
int   menu_overlay_get_vsync(void);
int   menu_overlay_get_filter(void);      /* 0=nearest, 1=linear */
int   menu_overlay_get_scanlines(void);
int   menu_overlay_get_show_fps(void);
int   menu_overlay_get_rotate(void);      /* 0=normal, 1=CW 90, 2=180, 3=CCW 90 */
float menu_overlay_get_volume(void);      /* 0.0..1.0 (already 0 when muted) */

/* ---- Per-player input bindings (player = 1 or 2) ---- */
/* Keyboard SDL_Scancode bound to a Lynx button (LYNX_MENU_BTN_*), or -1. */
int menu_overlay_key_for_button(int player, int lynx_btn);
/* Gamepad SDL_GameControllerButton bound to a Lynx button, or -1. */
int menu_overlay_pad_button_for_button(int player, int lynx_btn);

#ifdef __cplusplus
}
#endif

#endif /* LYNXRECOMP_MENU_OVERLAY_H */
