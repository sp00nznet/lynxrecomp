/*
 * menu_overlay.cpp - Dear ImGui menu system for the lynxrecomp launcher.
 *
 * A persistent main menu bar (File / Graphics / Sound / Controller /
 * Multiplayer / Help) drawn over the running game via the SDL2 + SDLRenderer2
 * ImGui backends. Modelled on sp00nznet/snesrecomp's menu_overlay, adapted to
 * the Atari Lynx: nine buttons (A / B / Option 1 / Option 2 / D-pad / Pause),
 * a screen-rotation option for the rotate-to-play titles, and ComLynx-style
 * 2-player netplay. Supports keyboard AND gamepad bindings for both players.
 * Config (settings + bindings) saves to lynx_config.ini.
 */

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lynxrecomp/menu_overlay.h"
#include "lynxrecomp/mp_session.h"

#define LYNX_BTN_COUNT  LYNX_MENU_BTN_COUNT
#define LYNX_CONFIG_PATH "lynx_config.ini"
#define LYNX_REPO_URL   "https://github.com/sp00nznet/lynxrecomp"

/* Button display names, indexed by LYNX_MENU_BTN_*. */
static const char *kBtnNames[LYNX_BTN_COUNT] = {
    "A", "B", "Option1", "Option2", "Up", "Down", "Left", "Right", "Pause"
};

struct MenuState {
    bool enabled        = false;
    bool quit_requested = false;

    /* Graphics */
    int  scale     = 4;
    int  vsync     = 1;
    int  filter    = 0;   /* 0=nearest (the Lynx LCD is crisp), 1=linear */
    int  scanlines = 0;
    int  show_fps  = 0;
    int  rotate    = 0;   /* 0=normal, 1=CW90, 2=180, 3=CCW90 (e.g. Gauntlet) */

    /* Sound */
    float volume = 1.0f;
    bool  mute   = false;

    /* Controller bindings: [player 0..1][button 0..8] */
    int  key[2][LYNX_BTN_COUNT];   /* SDL_Scancode, -1 = unset */
    int  pad[2][LYNX_BTN_COUNT];   /* SDL_GameControllerButton, -1 = unset */

    /* Windows */
    bool show_about      = false;
    bool show_controller = false;
    bool show_mp         = false;

    /* Multiplayer (netplay) connection fields. */
    char mp_ip[64] = "127.0.0.1";
    int  mp_port   = 7878;

    /* Rebind capture. rebind_btn = -1 when idle. */
    int  rebind_player = 0;
    int  rebind_btn    = -1;
    int  rebind_pad    = 0;   /* 0 = keyboard, 1 = gamepad */

    int  menubar_h = 0;

    /* One-shot File-menu requests (consumed by the host). */
    bool reset_req = false;
    bool save_req  = false;
    bool load_req  = false;
};

static MenuState g;

/* Xbox-style face mapping: Lynx A(action)=A, B=X, options on the shoulders,
 * Pause=Start, D-pad=dpad. */
static void set_default_pad(MenuState &m, int p) {
    m.pad[p][LYNX_MENU_BTN_A]       = SDL_CONTROLLER_BUTTON_A;
    m.pad[p][LYNX_MENU_BTN_B]       = SDL_CONTROLLER_BUTTON_X;
    m.pad[p][LYNX_MENU_BTN_OPTION1] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    m.pad[p][LYNX_MENU_BTN_OPTION2] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    m.pad[p][LYNX_MENU_BTN_PAUSE]   = SDL_CONTROLLER_BUTTON_START;
    m.pad[p][LYNX_MENU_BTN_UP]      = SDL_CONTROLLER_BUTTON_DPAD_UP;
    m.pad[p][LYNX_MENU_BTN_DOWN]    = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    m.pad[p][LYNX_MENU_BTN_LEFT]    = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    m.pad[p][LYNX_MENU_BTN_RIGHT]   = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
}

static void set_default_keys(MenuState &m) {
    /* Player 1 - arrows + Z/X, options on A/S, Pause on Enter. */
    m.key[0][LYNX_MENU_BTN_UP]      = SDL_SCANCODE_UP;
    m.key[0][LYNX_MENU_BTN_DOWN]    = SDL_SCANCODE_DOWN;
    m.key[0][LYNX_MENU_BTN_LEFT]    = SDL_SCANCODE_LEFT;
    m.key[0][LYNX_MENU_BTN_RIGHT]   = SDL_SCANCODE_RIGHT;
    m.key[0][LYNX_MENU_BTN_A]       = SDL_SCANCODE_Z;
    m.key[0][LYNX_MENU_BTN_B]       = SDL_SCANCODE_X;
    m.key[0][LYNX_MENU_BTN_OPTION1] = SDL_SCANCODE_A;
    m.key[0][LYNX_MENU_BTN_OPTION2] = SDL_SCANCODE_S;
    m.key[0][LYNX_MENU_BTN_PAUSE]   = SDL_SCANCODE_RETURN;
    /* Player 2 - right-hand cluster (gamepad is the expected P2 device). */
    m.key[1][LYNX_MENU_BTN_UP]      = SDL_SCANCODE_KP_8;
    m.key[1][LYNX_MENU_BTN_DOWN]    = SDL_SCANCODE_KP_5;
    m.key[1][LYNX_MENU_BTN_LEFT]    = SDL_SCANCODE_KP_4;
    m.key[1][LYNX_MENU_BTN_RIGHT]   = SDL_SCANCODE_KP_6;
    m.key[1][LYNX_MENU_BTN_A]       = SDL_SCANCODE_K;
    m.key[1][LYNX_MENU_BTN_B]       = SDL_SCANCODE_L;
    m.key[1][LYNX_MENU_BTN_OPTION1] = SDL_SCANCODE_I;
    m.key[1][LYNX_MENU_BTN_OPTION2] = SDL_SCANCODE_O;
    m.key[1][LYNX_MENU_BTN_PAUSE]   = SDL_SCANCODE_KP_ENTER;
}

static void set_default_binds(MenuState &m) { set_default_keys(m); set_default_pad(m, 0); set_default_pad(m, 1); }

static void reset_defaults(MenuState &m) {
    m.scale = 4; m.vsync = 1; m.filter = 0; m.scanlines = 0; m.show_fps = 0; m.rotate = 0;
    m.volume = 1.0f; m.mute = false;
    set_default_binds(m);
}

/* ---- Config persistence (simple key=value ini) ---- */

static void config_save(void) {
    FILE *f = fopen(LYNX_CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "# lynxrecomp launcher settings\n");
    fprintf(f, "scale=%d\n",     g.scale);
    fprintf(f, "vsync=%d\n",     g.vsync);
    fprintf(f, "filter=%d\n",    g.filter);
    fprintf(f, "scanlines=%d\n", g.scanlines);
    fprintf(f, "show_fps=%d\n",  g.show_fps);
    fprintf(f, "rotate=%d\n",    g.rotate);
    fprintf(f, "volume=%d\n",    (int)(g.volume * 100.0f + 0.5f));
    fprintf(f, "mute=%d\n",      g.mute ? 1 : 0);
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < LYNX_BTN_COUNT; i++) {
            fprintf(f, "p%d_key_%s=%d\n", p + 1, kBtnNames[i], g.key[p][i]);
            fprintf(f, "p%d_pad_%s=%d\n", p + 1, kBtnNames[i], g.pad[p][i]);
        }
    fclose(f);
}

static int btn_index_by_name(const char *name) {
    for (int i = 0; i < LYNX_BTN_COUNT; i++)
        if (strcmp(name, kBtnNames[i]) == 0) return i;
    return -1;
}

static void config_load(void) {
    FILE *f = fopen(LYNX_CONFIG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char keybuf[80]; int val;
        if (sscanf(line, "%79[^=]=%d", keybuf, &val) != 2) continue;
        if      (!strcmp(keybuf, "scale"))     g.scale = val;
        else if (!strcmp(keybuf, "vsync"))     g.vsync = val ? 1 : 0;
        else if (!strcmp(keybuf, "filter"))    g.filter = val;
        else if (!strcmp(keybuf, "scanlines")) g.scanlines = val ? 1 : 0;
        else if (!strcmp(keybuf, "show_fps"))  g.show_fps = val ? 1 : 0;
        else if (!strcmp(keybuf, "rotate"))    g.rotate = ((val % 4) + 4) % 4;
        else if (!strcmp(keybuf, "volume"))    g.volume = val / 100.0f;
        else if (!strcmp(keybuf, "mute"))      g.mute = val != 0;
        else if (keybuf[0] == 'p' && (keybuf[1] == '1' || keybuf[1] == '2') && keybuf[2] == '_') {
            int p = keybuf[1] - '1';
            const char *rest = keybuf + 3;
            if      (!strncmp(rest, "key_", 4)) { int bi = btn_index_by_name(rest + 4); if (bi >= 0) g.key[p][bi] = val; }
            else if (!strncmp(rest, "pad_", 4)) { int bi = btn_index_by_name(rest + 4); if (bi >= 0) g.pad[p][bi] = val; }
        }
    }
    fclose(f);
    if (g.scale < 1) g.scale = 1;
    if (g.scale > 8) g.scale = 8;
}

/* ---- Theme ---- */

static void apply_theme(void) {
    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding  = 3.0f;
    s.GrabRounding   = 3.0f;
    ImVec4 *c = s.Colors;
    c[ImGuiCol_MenuBarBg]     = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);  /* opaque (matches frame clear) */
    c[ImGuiCol_Header]        = ImVec4(0.20f, 0.45f, 0.85f, 0.65f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.55f, 0.95f, 0.80f);
}

/* ================= public API ================= */

extern "C" void menu_overlay_init(struct SDL_Window *window, struct SDL_Renderer *renderer) {
    set_default_binds(g);
    if (getenv("LYNX_HEADLESS")) { g.enabled = false; return; }
    if (!window || !renderer)    { g.enabled = false; return; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    apply_theme();
    ImGui_ImplSDL2_InitForSDLRenderer((SDL_Window *)window, (SDL_Renderer *)renderer);
    ImGui_ImplSDLRenderer2_Init((SDL_Renderer *)renderer);

    mp_init();
    config_load();
    g.enabled = true;
}

extern "C" void menu_overlay_shutdown(void) {
    if (!g.enabled) return;
    config_save();
    mp_shutdown();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    g.enabled = false;
}

extern "C" int menu_overlay_process_event(const void *sdl_event) {
    if (!g.enabled) return 0;
    const SDL_Event *ev = (const SDL_Event *)sdl_event;

    /* Rebind capture takes precedence so the menu doesn't swallow it first. */
    if (g.rebind_btn >= 0) {
        if (ev->type == SDL_KEYDOWN) {
            if (ev->key.keysym.scancode == SDL_SCANCODE_ESCAPE) g.rebind_btn = -1;
            else if (g.rebind_pad == 0) { g.key[g.rebind_player][g.rebind_btn] = ev->key.keysym.scancode; g.rebind_btn = -1; }
            return 1;
        }
        if (ev->type == SDL_CONTROLLERBUTTONDOWN && g.rebind_pad == 1) {
            g.pad[g.rebind_player][g.rebind_btn] = ev->cbutton.button;
            g.rebind_btn = -1;
            return 1;
        }
    }

    ImGui_ImplSDL2_ProcessEvent(ev);
    ImGuiIO &io = ImGui::GetIO();
    return (io.WantCaptureKeyboard || io.WantCaptureMouse) ? 1 : 0;
}

static void bind_button_row(int player, int btn) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", kBtnNames[btn]);

    ImGui::TableNextColumn();
    bool kb_rebinding = (g.rebind_btn == btn && g.rebind_player == player && g.rebind_pad == 0);
    const char *kn = kb_rebinding ? "<press a key>" : SDL_GetScancodeName((SDL_Scancode)g.key[player][btn]);
    if (!kn || !kn[0]) kn = "<unset>";
    char kl[64]; snprintf(kl, sizeof(kl), "%s##k%d_%d", kn, player, btn);
    if (ImGui::Button(kl, ImVec2(-1, 0))) { g.rebind_player = player; g.rebind_btn = btn; g.rebind_pad = 0; }

    ImGui::TableNextColumn();
    bool pad_rebinding = (g.rebind_btn == btn && g.rebind_player == player && g.rebind_pad == 1);
    const char *pn = pad_rebinding ? "<press a pad button>"
                   : (g.pad[player][btn] >= 0 ? SDL_GameControllerGetStringForButton((SDL_GameControllerButton)g.pad[player][btn]) : "<unset>");
    if (!pn || !pn[0]) pn = "<unset>";
    char pl[64]; snprintf(pl, sizeof(pl), "%s##p%d_%d", pn, player, btn);
    if (ImGui::Button(pl, ImVec2(-1, 0))) { g.rebind_player = player; g.rebind_btn = btn; g.rebind_pad = 1; }
}

static void draw_player_tab(int player) {
    if (ImGui::BeginTable("binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Button");
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Gamepad");
        ImGui::TableHeadersRow();
        for (int i = 0; i < LYNX_BTN_COUNT; i++) bind_button_row(player, i);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    char id[32]; snprintf(id, sizeof(id), "Reset P%d defaults", player + 1);
    if (ImGui::Button(id)) { set_default_keys(g); set_default_pad(g, player); }
}

static void draw_controller_window(void) {
    ImGui::SetNextWindowSize(ImVec2(440, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Controller", &g.show_controller)) {
        int npads = 0;
        for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) npads++;
        ImGui::Text("Gamepads connected: %d", npads);
        ImGui::TextDisabled("Click a cell, then press a key / pad button. Esc cancels.");
        ImGui::Separator();
        if (ImGui::BeginTabBar("players")) {
            if (ImGui::BeginTabItem("Player 1")) { draw_player_tab(0); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Player 2")) { draw_player_tab(1); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        if (ImGui::Button("Save")) config_save();
    }
    ImGui::End();
}

static void draw_multiplayer_window(void) {
    ImGui::SetNextWindowSize(ImVec2(380, 290), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Multiplayer (ComLynx netplay)", &g.show_mp)) {
        ImGui::TextWrapped("Lockstep netplay over TCP, standing in for the Lynx's "
            "ComLynx cable. The host shares its game state, then both machines "
            "play in sync. Host = Player 1, client = Player 2.");
        ImGui::Separator();
        ImGui::Text("Status: %s", mp_status_text());
        ImGui::Separator();

        MpState st = mp_get_state();
        if (st == MP_IDLE || st == MP_DISCONNECTED) {
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Port", &g.mp_port);
            if (g.mp_port < 1)     g.mp_port = 1;
            if (g.mp_port > 65535) g.mp_port = 65535;
            if (ImGui::Button("Host  (Player 1)", ImVec2(-1, 0))) mp_host(g.mp_port);
            ImGui::Spacing();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("Host IP", g.mp_ip, sizeof(g.mp_ip));
            if (ImGui::Button("Join  (Player 2)", ImVec2(-1, 0))) mp_join(g.mp_ip, g.mp_port);
        } else {
            if (st == MP_HOSTING)    ImGui::TextDisabled("Waiting for a peer to connect...");
            if (st == MP_CONNECTING) ImGui::TextDisabled("Connecting...");
            if (st == MP_CONNECTED)  ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "Connected - playing in sync.");
            ImGui::Spacing();
            if (ImGui::Button("Disconnect", ImVec2(-1, 0))) mp_disconnect();
        }
    }
    ImGui::End();
}

static void draw_about_window(void) {
    ImGui::SetNextWindowSize(ImVec2(380, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About", &g.show_about)) {
        ImGui::Text("lynxrecomp");
        ImGui::TextDisabled("Atari Lynx static recompilation");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::BulletText("CPU: WDC 65SC02 -> native C");
        ImGui::BulletText("Suzy (blitter + math) / Mikey (timers, video, audio)");
        ImGui::Spacing();
        ImGui::TextUnformatted("Source repository:");
        ImGui::TextWrapped("%s", LYNX_REPO_URL);
        if (ImGui::Button("Open repository in browser")) SDL_OpenURL(LYNX_REPO_URL);
    }
    ImGui::End();
}

extern "C" void menu_overlay_render(struct SDL_Renderer *renderer) {
    if (!g.enabled) return;

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        g.menubar_h = (int)(ImGui::GetWindowSize().y + 0.5f);
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Restart", "")) g.reset_req = true;
            if (ImGui::MenuItem("Save State", "F5")) g.save_req = true;
            if (ImGui::MenuItem("Load State", "F8")) g.load_req = true;
            ImGui::Separator();
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem("Reset settings")) reset_defaults(g);
                if (ImGui::MenuItem("Save settings"))  config_save();
                if (ImGui::MenuItem("Load settings"))  config_load();
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) g.quit_requested = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Graphics")) {
            if (ImGui::BeginMenu("Window Scale")) {
                for (int s = 1; s <= 8; s++) {
                    char lbl[8]; snprintf(lbl, sizeof(lbl), "%dx", s);
                    if (ImGui::MenuItem(lbl, NULL, g.scale == s)) g.scale = s;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Rotate")) {
                const char *rl[4] = { "Normal", "90 CW", "180", "90 CCW" };
                for (int r = 0; r < 4; r++)
                    if (ImGui::MenuItem(rl[r], NULL, g.rotate == r)) g.rotate = r;
                ImGui::EndMenu();
            }
            ImGui::MenuItem("V-Sync", NULL, (bool *)&g.vsync);
            if (ImGui::BeginMenu("Filter")) {
                if (ImGui::MenuItem("Nearest (sharp)", NULL, g.filter == 0)) g.filter = 0;
                if (ImGui::MenuItem("Linear (smooth)", NULL, g.filter == 1)) g.filter = 1;
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Scanlines", NULL, (bool *)&g.scanlines);
            ImGui::MenuItem("Show FPS",  NULL, (bool *)&g.show_fps);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Sound")) {
            ImGui::SetNextItemWidth(140);
            ImGui::SliderFloat("Volume", &g.volume, 0.0f, 1.0f, "%.2f");
            ImGui::MenuItem("Mute", NULL, &g.mute);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Controller")) {
            ImGui::MenuItem("Configure (P1 / P2)...", NULL, &g.show_controller);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Multiplayer")) {
            ImGui::MenuItem("Connect (Host / Join)...", NULL, &g.show_mp);
            ImGui::Separator();
            ImGui::TextDisabled("%s", mp_status_text());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About", NULL, &g.show_about);
            if (ImGui::MenuItem("Source repository")) SDL_OpenURL(LYNX_REPO_URL);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (g.show_controller) draw_controller_window();
    if (g.show_mp)         draw_multiplayer_window();
    if (g.show_about)      draw_about_window();

    if (g.show_fps) {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 90, 24), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("##fps", NULL,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%.0f FPS", io.Framerate);
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), (SDL_Renderer *)renderer);
}

extern "C" int menu_overlay_is_active(void) {
    if (!g.enabled) return 0;
    ImGuiIO &io = ImGui::GetIO();
    return (g.rebind_btn >= 0 || io.WantCaptureKeyboard || io.WantCaptureMouse) ? 1 : 0;
}
extern "C" int  menu_overlay_get_menubar_height(void) { return g.enabled ? g.menubar_h : 0; }
extern "C" void menu_overlay_get_bar_color(int *r, int *gg, int *b) {
    if (g.enabled) {
        ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
        *r = (int)(c.x * 255 + 0.5f); *gg = (int)(c.y * 255 + 0.5f); *b = (int)(c.z * 255 + 0.5f);
    } else { *r = 0; *gg = 0; *b = 0; }
}
extern "C" int   menu_overlay_quit_requested(void) { return g.quit_requested; }
extern "C" int   menu_overlay_take_reset(void)      { int r = g.reset_req; g.reset_req = false; return r; }
extern "C" int   menu_overlay_take_save_state(void) { int r = g.save_req;  g.save_req  = false; return r; }
extern "C" int   menu_overlay_take_load_state(void) { int r = g.load_req;  g.load_req  = false; return r; }
extern "C" int   menu_overlay_get_scale(void)      { return g.enabled ? g.scale : 4; }
extern "C" int   menu_overlay_get_vsync(void)      { return g.vsync; }
extern "C" int   menu_overlay_get_filter(void)     { return g.filter; }
extern "C" int   menu_overlay_get_scanlines(void)  { return g.scanlines; }
extern "C" int   menu_overlay_get_show_fps(void)   { return g.show_fps; }
extern "C" int   menu_overlay_get_rotate(void)     { return g.enabled ? g.rotate : 0; }
extern "C" float menu_overlay_get_volume(void)     { return g.mute ? 0.0f : g.volume; }

extern "C" int menu_overlay_key_for_button(int player, int idx) {
    if (player < 1 || player > 2 || idx < 0 || idx >= LYNX_BTN_COUNT) return -1;
    return g.key[player - 1][idx];
}
extern "C" int menu_overlay_pad_button_for_button(int player, int idx) {
    if (player < 1 || player > 2 || idx < 0 || idx >= LYNX_BTN_COUNT) return -1;
    return g.pad[player - 1][idx];
}
