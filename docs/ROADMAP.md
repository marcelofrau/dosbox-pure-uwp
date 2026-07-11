# Roadmap — DOSBox Pure Unleashed UWP

Implementation phases ordered by dependency. Each phase is a runnable (or at least buildable) increment.

Legend: ✅ done  🏗️ partial  🔲 not started  ⏸️ blocked  ❌ broken

---

## Phase 0 — Scaffold + Infrastructure
**Goal: DirectX 11 UWP template compiling + SDL2 + build tooling**
**Status: ✅ DONE**

- [x] DirectX 11 UWP `App` + `IFrameworkView` loop
- [x] `DeviceResources` (DXGI swap chain, D3D/D2D/DWrite devices)
- [x] `StepTimer` for fixed/variable timestep
- [x] SDL_GameController + haptic + audio init
- [x] UWP `Gamepad` API fallback (Xbox)
- [x] Build scripts, signing cert, .gitignore

---

## Phase 1 — Core Integration (Build)
**Goal: dosbox-pure compiles as part of the UWP app (0 errors)**
**Status: ✅ DONE**

- [x] Submodule `extern/dosbox-pure` at `e166344`
- [x] Selective copy of `extern/libretro-common/` (VFS + UWP helpers)
- [x] ~150 source files from core added to vcxproj
- [x] 5 local patches: `cross.cpp`, `midi.cpp`, `dbp_serialize.cpp`, `drive_cache.cpp`, `dosbox_pure_libretro.cpp`
- [x] Per-file overrides: `CompileAsWinRT=false`, `/bigobj`, `DisableSpecificWarnings=4703`
- [x] Build succeeds 0 errors (Release|x64)

---

## Phase 2 — Core Lifecycle
**Goal: ROM loads, core initializes, frames render**
**Status: ✅ DONE**

- [x] `RetroCore`: init, load, run, unload, deinit
- [x] `retro_env`: GET_VFS_INTERFACE, GET_SYSTEM_DIR, GET_SAVE_DIR, SET_PIXEL_FORMAT, SET_HW_RENDER (return 0), SET_MESSAGE_EXT, SET_KEYBOARD_CALLBACK, GET_THROTTLE_STATE, SHUTDOWN
- [x] Smoke test: boot, F1 picker, load, video renders

---

## Phase 3 — Video Pipeline
**Goal: Core framebuffer renders on screen**
**Status: ✅ DONE**

- [x] `retro_video_refresh_cb` with pitch==0 guard
- [x] D2D bitmap: XRGB8888 + ALPHA_IGNORE + GetDpi() + CopyFromMemory
- [x] BeginDraw/DrawBitmap/EndDraw with letterboxing
- [x] D2DERR_RECREATE_TARGET handling

---

## Phase 4 — Input
**Goal: Keyboard + gamepad + mouse input working**
**Status: ✅ DONE**

- [x] `RETROK_*` mapping (VirtualKey → libretro keyboard codes)
- [x] `s_keyboardState[RETROK_LAST]` — full keyboard state
- [x] `s_joypadState[16]` — separate from keyboard (fixes state leak bug)
- [x] UWP Gamepad API → DPad/Buttons/Triggers
- [x] Mouse input: CoreWindow pointer events → SetMouseMove/SetPointer/SetMouseButton/SetMouseWheel
- [x] Mouse wheel scroll for file browser
- [x] PageUp/PageDown (keyboard + LB/RB shoulder buttons)
- [x] `SET_KEYBOARD_CALLBACK` pushes held keys to core

---

## Phase 5 — Audio Pipeline
**Goal: DOSBox sound plays through speakers**
**Status: ✅ DONE**

- [x] `XAudio2Output` class: alloc-per-submit ring buffer, `OnBufferEnd` callback
- [x] `retro_audio_sample_batch` → `Submit()` → `IXAudio2SourceVoice`
- [x] Queue-depth cap at 882 frames (~20ms) — flush/restart cycle
- [x] Multi-retro_run per visual frame (1.17× per 60fps frame → 70fps aggregate)
- [x] QPC-based frame pacing with `CreateWaitableTimerEx(HIGH_RESOLUTION)`
- [x] Queue-based accumulator scaling (replaces DRC)
- [x] Emergency catch-up when queue < 500 frames
- [x] Queue-headroom-based `maxRetroRuns` (prevents overshoot)

---

## Phase 6 — FrontendMenu (BIOS-style overlay)
**Goal: DOS-style menu system with gamepad navigation**
**Status: ✅ DONE**

- [x] D2D-rendered menu overlay (RGUI-inspired)
- [x] Menu tree: Continue Game, Load Game, Puremenu, Settings, Exit
- [x] Gamepad: DPad navigate, A confirm, B back
- [x] Mouse: click select + confirm
- [x] PageUp/PageDown scroll items by MAX_VISIBLE
- [x] Semi-transparent background, VGA/DOS color palette
- [x] `OPEN_FILE` action opens FileBrowser
- [x] `OPEN_PUREMENU` action opens core's OSD
- [x] Boot animation with beep + memory count

---

## Phase 7 — FileBrowser (In-app file explorer)
**Goal: Replace UWP FileOpenPicker with gamepad-navigable file browser**
**Status: ✅ DONE**

- [x] DOS/BIOS-style D2D overlay panel
- [x] Gamepad: DPad navigate, A confirm, B back, LB/RB page
- [x] Mouse: wheel scroll, click select+confirm
- [x] Keyboard: arrows, Enter, Escape, PageUp/PageDown
- [x] Supported extensions: `.zip .dosz .exe .com .bat .iso .chd .cue .img .ima .vhd .conf`
- [x] Marquee scroll on long filenames, text ellipsis trimming
- [x] `broadFileSystemAccess` + `runFullTrust` capabilities
- [x] Ctrl+L fallback to system FileOpenPicker
- [x] Xbox B button (BackRequested) routes to file browser or menu
- [x] Async file copy to `LocalFolder/temp/` before `QueueLoadRom`

---

## Phase 8 — PUREMENU Integration
**Goal: Core's OSD menu (PUREMENU) works and persists changes**
**Status: ✅ DONE**

- [x] `RETRO_ENVIRONMENT_SET_VARIABLE` → stores in `s_optionValues` map
- [x] `RETRO_ENVIRONMENT_GET_VARIABLE` → returns from map
- [x] `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` → returns dirty flag
- [x] OSD overlay renders directly onto framebuffer (DBP_STANDALONE fix)
- [x] PUREMENU option changes propagate to core via `check_variables()`

---

## Phase 9 — Dynamic Recompiler (JIT)
**Goal: Enable dynarec for 5-10× speedup**
**Status: ✅ DONE**

- [x] Patched `dyn_cache.h`: `VirtualAllocFromApp` + `VirtualProtectFromApp`
- [x] `cache_make_writable()`/`cache_make_executable()` around code generation
- [x] `DISABLE_DYNAREC` removed from PreprocessorDefinitions
- [x] Include path priority: local patch before submodule

---

## Phase 10 — xb-xray Diagnostics
**Goal: Remote debugging for Xbox Dev Mode**
**Status: ✅ DONE**

- [x] `uwp-xray-depot` submodule integrated
- [x] `XB_INSPECTOR_ENABLED` guard (Debug only, zero overhead in Release)
- [x] TCP socket on port 9000-9009
- [x] Live binds: `audio_queued`, `fps`, `target_fps`, `frame_ms`, timing breakdown
- [x] spdlog → file + TCP + OutputDebugString

---

## Future Phases

### Phase 11 — Save States
**Goal: Save/load game state**
**Status: 🔲 NOT STARTED**

- [ ] `retro_serialize_size()`, `retro_serialize()`, `retro_unserialize()`
- [ ] `DBPS_RequestSaveLoad`, `DBPS_HaveSaveSlot`
- [ ] Hotkeys: F5 save, F7 load
- [ ] Auto-save on suspend/shutdown

### Phase 12 — Disk Control
**Goal: Multi-disc games (eject/swap)**
**Status: 🔲 NOT STARTED**

- [ ] `RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE`
- [ ] Callbacks: eject, swap, add, remove

### Phase 13 — Video Enhancements
**Goal: Scale modes, config UI**
**Status: 🔲 NOT STARTED**

- [ ] Scale modes: pixel-perfect, integer, stretch, aspect-ratio
- [ ] Core video options via GET_VARIABLE
- [ ] FPS baseline measurement

### Phase 14 — Network Play
**Goal: IPX tunneling for multiplayer**
**Status: 🔲 NOT STARTED**

- [ ] `RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE`
- [ ] IPX over TCP/UDP

---

## Dependencies Between Phases

```
Phase 0 (scaffold) ─────────────────── ✅ done
    │
Phase 1 (core compiles) ────────────── ✅ done
    │
Phase 2 (core lifecycle) ───────────── ✅ done
    │
Phase 3 (video) ────────────────────── ✅ done
    │
Phase 4 (input) ────────────────────── ✅ done
    │
Phase 5 (audio) ────────────────────── ✅ done
    │
Phase 6 (frontend menu) ───────────── ✅ done
    │
Phase 7 (file browser) ────────────── ✅ done
    │
Phase 8 (PUREMENU) ─────────────────── ✅ done
    │
Phase 9 (dynarec) ──────────────────── ✅ done
    │
Phase 10 (xb-xray) ────────────────── ✅ done
    │
Phase 11 (save states) ────────────── 🔲 future
Phase 12 (disk control) ───────────── 🔲 future
Phase 13 (video enhancements) ─────── 🔲 future
Phase 14 (network play) ───────────── 🔲 future
```
