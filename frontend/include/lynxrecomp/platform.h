/*
 * platform - SDL2 window / renderer / audio / input frontend for lynxrecomp.
 *
 * This is the interactive shell a recompiled game links against. The core
 * runtime library (the lynxrecomp target) stays dependency-free; THIS layer
 * pulls in SDL2 (and, when the menu is enabled, Dear ImGui) and is opt-in via
 * the LYNXRECOMP_FRONTEND CMake option.
 *
 * A game host typically:
 *   lynx_platform_init("My Game", 4);
 *   ... in its frame hook (called at each display flip):
 *       lynx_platform_present(rgba);     // 160x102 RGBA from lynx_video_render
 *       lynx_platform_queue_audio(pcm, n);
 *       if (!lynx_platform_poll()) quit; // also pushes input to lynx_input_set
 *       lynx_platform_frame_sync();
 *   lynx_platform_shutdown();
 *
 * Or it calls lynx_frontend_run() (frontend_run.c) which wires all of the above
 * around the recompiled game loop, including the menu's save/load/quit.
 */
#ifndef LYNXRECOMP_PLATFORM_H
#define LYNXRECOMP_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Create the window/renderer/audio. `scale` is the initial integer zoom of the
 * 160x102 picture (1..8; the menu can change it live). Returns false on
 * failure. A no-op-success in LYNX_HEADLESS mode (no window created). */
bool lynx_platform_init(const char *title, int scale);
void lynx_platform_shutdown(void);

/* Present one 160x102 RGBA8888 frame (0xAARRGGBB host order, as produced by
 * lynx_video_render). Applies live scale/filter/rotation from the menu. */
void lynx_platform_present(const uint32_t *rgba);

/* Queue mono 16-bit PCM for playback, scaled by the menu's master volume. */
void lynx_platform_queue_audio(const int16_t *pcm, int nsamples);

/* Pump the OS/SDL event queue and the menu. Updates the current Lynx input
 * (joystick + switches) from the bound keys/pad unless the menu is capturing.
 * Returns false when the user asked to quit (window close / Esc / File->Quit). */
bool lynx_platform_poll(void);

/* Sleep to hold ~60 fps (the Lynx default refresh; games can reprogram it, but
 * this is a sane cap for the host). No-op when vsync is on. */
void lynx_platform_frame_sync(void);

/* The Lynx audio sample rate the platform opened the device at. */
int  lynx_platform_audio_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* LYNXRECOMP_PLATFORM_H */
