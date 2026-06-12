/*
 * menu_overlay_stub.c - no-op menu overlay.
 *
 * Compiled instead of menu_overlay.cpp when LYNXRECOMP_MENU is off, so the SDL
 * platform layer links without Dear ImGui. Every call is safe and inert; the
 * accessors return sensible defaults so the platform behaves as if the menu is
 * simply disabled. Bindings mirror the C++ menu's defaults so a no-menu build
 * still has working keyboard controls.
 */
#include "lynxrecomp/menu_overlay.h"
#include <SDL.h>

void menu_overlay_init(struct SDL_Window *w, struct SDL_Renderer *r) { (void)w; (void)r; }
void menu_overlay_shutdown(void) { }
int  menu_overlay_process_event(const void *e) { (void)e; return 0; }
void menu_overlay_render(struct SDL_Renderer *r) { (void)r; }
int  menu_overlay_is_active(void) { return 0; }
int  menu_overlay_get_menubar_height(void) { return 0; }
void menu_overlay_get_bar_color(int *r, int *g, int *b) { *r = 0; *g = 0; *b = 0; }
int  menu_overlay_quit_requested(void) { return 0; }
int  menu_overlay_take_reset(void) { return 0; }
int  menu_overlay_take_save_state(void) { return 0; }
int  menu_overlay_take_load_state(void) { return 0; }
int  menu_overlay_get_scale(void) { return 4; }
int  menu_overlay_get_vsync(void) { return 1; }
int  menu_overlay_get_filter(void) { return 0; }
int  menu_overlay_get_scanlines(void) { return 0; }
int  menu_overlay_get_show_fps(void) { return 0; }
int  menu_overlay_get_rotate(void) { return 0; }
float menu_overlay_get_volume(void) { return 1.0f; }

/* Default keyboard binds so a no-menu build is still playable (P1 only). */
int menu_overlay_key_for_button(int player, int idx) {
    if (player != 1) return -1;
    switch (idx) {
        case LYNX_MENU_BTN_UP:      return SDL_SCANCODE_UP;
        case LYNX_MENU_BTN_DOWN:    return SDL_SCANCODE_DOWN;
        case LYNX_MENU_BTN_LEFT:    return SDL_SCANCODE_LEFT;
        case LYNX_MENU_BTN_RIGHT:   return SDL_SCANCODE_RIGHT;
        case LYNX_MENU_BTN_A:       return SDL_SCANCODE_Z;
        case LYNX_MENU_BTN_B:       return SDL_SCANCODE_X;
        case LYNX_MENU_BTN_OPTION1: return SDL_SCANCODE_A;
        case LYNX_MENU_BTN_OPTION2: return SDL_SCANCODE_S;
        case LYNX_MENU_BTN_PAUSE:   return SDL_SCANCODE_RETURN;
        default:                    return -1;
    }
}
int menu_overlay_pad_button_for_button(int player, int idx) { (void)player; (void)idx; return -1; }
