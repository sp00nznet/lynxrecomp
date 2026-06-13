# The interactive frontend (SDL2 + Dear ImGui)

`lynxrecomp`'s core — the runtime library and the recompiler — is deliberately
dependency-free. The **frontend** is a separate, opt-in layer that gives a
recompiled game a real window: scaled video, audio, keyboard/gamepad input,
save states, and an on-screen menu. It links SDL2 and (for the menu) a vendored
copy of Dear ImGui.

It is modelled on [`snesrecomp`](https://github.com/sp00nznet/snesrecomp)'s
launcher, re-fitted to the Lynx: a 160×102 display, screen rotation for the
rotate-to-play titles, the Lynx's nine buttons, and ComLynx-style multiplayer
(see [`MULTIPLAYER.md`](MULTIPLAYER.md)).

## Building it

The frontend is **off by default**. Turn it on and point CMake at SDL2 (vcpkg
is the easy path on Windows):

```powershell
cmake -S . -B build-fe -DLYNXRECOMP_FRONTEND=ON `
      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
      -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build-fe --config Release
# -> build-fe/frontend/Release/lynxrecomp_frontend.lib
```

A game links `lynxrecomp_frontend` (which transitively brings in `lynxrecomp`
and SDL2). The menu can be disabled with `-DLYNXRECOMP_MENU=OFF`, which compiles
a no-op stub in place of the ImGui overlay so the platform still links without
ImGui (keyboard controls keep working from built-in defaults).

## What it provides

`frontend/include/lynxrecomp/platform.h` is the host API:

| call | does |
|---|---|
| `lynx_platform_init(title, scale)` | open the window / renderer / audio |
| `lynx_platform_present(rgba)` | show one 160×102 frame (from `lynx_video_render`) |
| `lynx_platform_queue_audio(pcm, n)` | push PCM, scaled by the menu's volume |
| `lynx_platform_poll()` | pump events + menu; set `lynx_input_set` from binds |
| `lynx_platform_frame_sync()` | hold ~60 fps |
| `lynx_platform_shutdown()` | tear everything down |

A game's frame hook calls present → queue audio → poll → frame_sync.

## The menu

A persistent menu bar over the game (no-op in headless/`LYNX_HEADLESS` runs):

- **File** — Restart, Save State (F5), Load State (F8), settings, Quit.
- **Graphics** — window scale 1×–8×, screen rotation (Normal / 90 CW / 180 /
  90 CCW), V-Sync, nearest/linear filter, scanlines, FPS overlay.
- **Sound** — master volume, mute.
- **Controller** — rebind every Lynx button for **keyboard and gamepad**, per
  player (P1/P2), live; click a cell and press the new key/pad button.
- **Multiplayer** — host / join a ComLynx netplay session
  ([`MULTIPLAYER.md`](MULTIPLAYER.md)).
- **Help** — about, link to the source repo.

Settings and bindings persist to `lynx_config.ini` next to the executable.

### Default controls (Player 1)

| Lynx | Keyboard | Gamepad |
|---|---|---|
| D-pad | Arrow keys | D-pad |
| A | Z | A |
| B | X | X |
| Option 1 / 2 | A / S | LB / RB |
| Pause | Enter | Start |

Player 2 defaults to a right-hand keyboard cluster (IJKL + numpad); it is meant
for a second *local instance* — the Lynx itself had no second controller.

## Save states

The menu's Save/Load (and F5/F8) call the runtime's save-state backend
(`lynxrecomp/state.h`), which serializes the full machine: 64 KiB RAM, CPU, Suzy,
Mikey, the timer/audio internals, and the ComLynx UART. The format is versioned
(`LYNXSAVE`, currently v2) and self-describing in size, so a short or
wrong-magic blob is rejected rather than misread.
