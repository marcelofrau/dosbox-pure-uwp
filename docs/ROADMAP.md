# Roadmap — DOSBox Pure Unleashed UWP

Implementation phases ordered by dependency. Each phase is a runnable (or at least buildable) increment.

Legend: ✅ done  🏗️ partial  🔲 not started  ⏸️ blocked  ❌ broken

---

## Phase 0 — Scaffold + Infrastructure
**Goal: DirectX 11 UWP template compiling + SDL2 + build tooling**
**Status: ✅ DONE**

### 0.1 — Template
- [x] DirectX 11 UWP `App` + `IFrameworkView` loop compiles
- [x] `DeviceResources` (DXGI swap chain, D3D/D2D/DWrite devices)
- [x] `StepTimer` for fixed/variable timestep
- [x] `Sample3DSceneRenderer` (3D rotating cube fallback)
- [x] `SampleFpsTextRenderer` (DWrite debug overlay)

### 0.2 — SDL2 integration
- [x] `SdlInput` class: SDL init (gamecontroller + haptic + audio)
- [x] SDL_GameController open close add/remove events
- [x] UWP `Gamepad` API fallback (works on Xbox without SDL)
- [x] Button state tracking (m_buttonHeld / m_buttonJustPressed)
- [x] Gamepad A button → beep + background color change (test)

### 0.3 — Audio (test)
- [x] SDL audio device init (44100Hz, AUDIO_S16SYS, mono)
- [x] `PlayBeep()` generates sine wave → `SDL_QueueAudio`

### 0.4 — Build system
- [x] `.sln` with `uwp-dep.props` for SDL paths
- [x] `build.ps1`, `package.ps1`, `new-cert.ps1`, `install.ps1`
- [x] `.gitignore` (certs, build artifacts)
- [x] PFX cert with password `dev` for AppX packaging

---

## Phase 1 — Core Integration (Build)
**Goal: dosbox-pure compiles as part of the UWP app (0 errors)**
**Status: ✅ DONE**

### 1.1 — Submodule
- [x] `git submodule add` — `extern/dosbox-pure` at `e166344` (has UWP compat patches)
- [x] `extern/libretro-common/` — selective copy of VFS + UWP helpers (~10 files)

### 1.2 — Add core sources to vcxproj
- [x] 15 source files from `$(DBPDir)src/`: cpu (callback, core_dyn_x86, core_dynrec, core_full, core_normal, core_prefetch, core_simple, cpu, flags, modrm, paging), dos (cdrom, cdrom_image, dos_classes, dos_devices, dos_execute, dos_files, dos_ioctl, dos_keyboard_layout, dos_memory, dos_misc, dos_mscdex, drives), hardware (adlib, disney, dma, gameblaster, gus, ipx, joystick, mame, mixer, mouse, pci, pcspeaker, serialport, sblaster, tandy_sound, vga_*, voodoo_*, ...), ints, fpu, gui, misc, hardware/serialport, dbp_network, plus more
- [x] `$(DBPDir)dosbox_pure_libretro.cpp` (main libretro bridge — 4073 lines)
- [x] `$(DBPDir)dosbox_pure_ver.h`, `dosbox_pure_pad.h`, `dosbox_pure_run.h`, `dosbox_pure_osd.h` (core OS/menu logic)
- [x] `$(DBPDir)core_options.h` (~1177 lines — 70+ config variables)
- [x] 5 local patches in `dosbox-uwp/local/dosbox-pure/` (cross.cpp, midi.cpp, dbp_serialize.cpp, drive_cache.cpp, dosbox_pure_libretro.cpp)

### 1.3 — Compile flags
- [x] Defines: `__LIBRETRO__`, `DBP_STANDALONE`, `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`, `_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING`
- [x] `CompileAsWinRT=false` for all .cpp from core (legacy C++)
- [x] `/bigobj` for large object files
- [x] Include paths: `$(DBPDir)`, `$(DBPDir)include`, `$(DBPDir)src`, `$(DBPDir)libretro-common/include`, `$(LrCommonDir)`, `$(LrCommonDir)include`
- [x] Optimizations: `MaxSpeed` for core files even in Debug

### 1.4 — Resolve incompatibilities
- [x] `cross.cpp`: UWP paths, `getcwd`/`chdir` replacements via local patch
- [x] `midi.cpp`: disabled MIDI output (no MIDI UWP API)
- [x] `drive_cache.cpp`: std::filesystem compat via local patch
- [x] `dosbox_pure_libretro.cpp`: local copy with UWP tweaks (SET_HW_RENDER handling, etc.)
- [x] VFS: `vfs_implementation_uwp.cpp` from RetroArch — uses `CreateFile2FromAppW`

### 1.5 — Verify
- [x] Build succeeds 0 errors (Release|x64)
- [x] ~~1500 warnings C4244 (cosmetic — "conversion possible loss of data")

---

## Phase A — Core Infra (Changes 1-3)
**Goal: ROMs load, keyboard + gamepad work in core**
**Status: 🏗️ PARTIAL — patch-fopen-vfs DONE**

### Change: `patch-fopen-vfs` ✅
**Files:** `dosbox-uwp/local/dosbox-pure/dosbox_pure_libretro.cpp`, `extern/libretro-common/vfs/vfs_implementation_uwp.cpp`, `dosbox-uwp/local/dosbox-pure/src/misc/cross.cpp`

- [x] `fopen_wrap()` → copy to LocalFolder + CRT fopen (works on UWP)
- [x] VFS interface provided via GET_VFS_INTERFACE (used by fpath_nocase)
- [x] `info->data` fallback (LoadGame accepts in-memory buffer)
- [x] Verify `stat()`/`access()` UWP compat (works on LocalFolder)
- [x] **Result:** ZIP mounts as C: drive, games load
- [x] Task 6 N/A — dosbox-pure workflow: all disks inside ZIP, PUREMENU handles swap

### Change: `fix-keyboard-input`
**Files:** `dosbox-uwp/dosbox_uwpMain.cpp`, `dosbox-uwp/Content/RetroCore.cpp/.h`

- [ ] Map `Windows::System::VirtualKey` → `RETROK_*` (Enter, Escape, Backspace, Tab, Arrows, Shift, Ctrl, Alt, F1-F12, Numpad, punctuation)
- [ ] Expand `s_keyboardState[256]` → `s_keyboardState[RETROK_LAST]` (324)
- [ ] Resolve F1 conflict (picker vs core key)
- [ ] **Result:** Keyboard navigates Puremenu launcher

### Change: `fix-gamepad-input`
**Files:** `dosbox-uwp/Content/SdlInput.cpp/.h`, `dosbox-uwp/Content/RetroCore.cpp`

- [ ] SDL buttons A/B/X/Y/Select/Start → `RETRO_DEVICE_ID_JOYPAD_*`
- [ ] D-pad → `RETRO_DEVICE_ID_JOYPAD_UP/DOWN/LEFT/RIGHT`
- [ ] Left/Right sticks → `RETRO_DEVICE_ANALOG` (LY/LX/RY/RX)
- [ ] Triggers (L2/R2) → `RETRO_DEVICE_ID_JOYPAD_L2/R2`
- [ ] Shoulder (L1/R1) → `RETRO_DEVICE_ID_JOYPAD_L/R`
- [ ] **Result:** Gamepad navigates Puremenu + plays games

---

## Phase B — OSD/Launcher (Changes 4-5)
**Goal: Puremenu visible, configurable, navigable**
**Status: 🔲 NOT STARTED**

### Change: `register-core-options`
**Files:** `dosbox-uwp/Content/RetroCore.cpp`

- [ ] `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2` — expose ~52 vars from `core_options.h`
- [ ] `RETRO_ENVIRONMENT_GET_VARIABLE` — respond with current values
- [ ] `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` — notify changes
- [ ] Fallback v1/v0 if core requests older version
- [ ] **Result:** Puremenu launcher appears with options

### Change: `software-framebuffer`
**Files:** `dosbox-uwp/Content/RetroCore.cpp`

- [ ] `RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER` — expose SW framebuffer pointer
- [ ] Allow Pure OSD to draw overlay on top of game
- [ ] **Result:** OSD overlay visible (menus, notifications)

---

## Phase C — Audio (Change 6)
**Goal: DOSBox sound plays through speaker**
**Status: ✅ DONE** (replaced SDL audio with native XAudio2)

### Change: `wire-audio`
**Files:** `dosbox-uwp/Content/XAudio2Output.h/.cpp`, `dosbox-uwp/dosbox_uwpMain.cpp/.h`, `dosbox-uwp/Content/RetroCore.cpp/.h`, `dosbox-uwp/Content/SdlInput.cpp/.h`

- [x] Removed SDL audio subsystem init from SdlInput
- [x] Created `XAudio2Output` class: alloc-per-submit ring buffer, `OnBufferEnd` callback, file-scope `s_queuedFrames` counter
- [x] Voice `Start(0)` called in `Initialize()` with zero buffers (no pre-buffer)
- [x] `retro_audio()` calls `s_audioOutput->Submit()`
- [x] HUD shows XA2 ready + `GetQueuedFrames()` instead of SDL audio stats
- [x] Queue-depth cap: When `s_queuedFrames > MAX_QUEUE` (882 = 20ms), flush voice and restart with fresh audio — bounds max latency
- [x] Frame pacing: `CreateWaitableTimerEx(HIGH_RESOLUTION)` replaces `Sleep()` for µs-precision timing, fixing QPC/Sleep granularity drift
- [ ] **TODO (future):** Audio-driven pacing using `GetState().SamplesPlayed` to eliminate residual QPC drift entirely — see `docs/discoveries.md`

---

## Phase D — Platform (Changes 7-9)
**Goal: Mouse, joypad binding, multi-disc**
**Status: 🔲 NOT STARTED**

### Change: `dbps-mouse`
**Files:** `dosbox-uwp/App.cpp`, `dosbox-uwp/dosbox_pure_sta.cpp`

- [ ] Connect CoreWindow `PointerPressed/Moved/Released` → `DBPS_GetMouse`
- [ ] Relative (MOUSE) + absolute (POINTER)
- [ ] Scroll wheel support
- [ ] Mouse capture toggle (relative for FPS games)
- [ ] **Result:** Mouse works in DOS games and OSD

### Change: `dbps-platform`
**Files:** `dosbox-uwp/dosbox_pure_sta.cpp`

- [ ] `DBPS_HaveJoy` — return whether gamepad connected
- [ ] `DBPS_GetJoyBind` — return current mapping
- [ ] `DBPS_ApplyConfigOverrides` — read/write `FRONTEND.DBP`
- [ ] `DBPS_IsConfigOverride` / `DBPS_ToggleConfigOverride` / `DBPS_GetConfigOverrideJSON`
- [ ] `DBPS_SubmitOSDFrame` — route OSD frame to renderer
- [ ] `DBPS_StartCaptureJoyBind` — binding UI
- [ ] **Result:** DBPS stubs functional, core integrated with platform

### Change: `disk-control`
**Files:** `dosbox-uwp/Content/RetroCore.cpp`

- [ ] `RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE`
- [ ] Callbacks: `set_eject_state`, `get_eject_state`, `get_image_index`, `set_image_index`, `get_num_images`, `replace_image_index`, `add_image_index`, `set_initial_image`
- [ ] **Result:** Multi-disc games (eject/swap) work

---

## Phase E — UX (Changes 10-11)
**Goal: Configurable visual experience, modern overlay**
**Status: 🔲 NOT STARTED**

### Change: `video-enhancements`
**Files:** `dosbox-uwp/Content/RetroScreenRenderer.cpp/.h`

- [ ] Scale modes: pixel-perfect, integer, stretch, aspect-ratio
- [ ] Core video options exposed via GET_VARIABLE (machine, cga, svga, aspect_correction, overscan)
- [ ] FPS baseline measurement
- [ ] **Result:** User controls scaling and video options

### Change: `rmlui-overlay`
**Files:** New (RMLUI integration)

- [ ] Integrate RMLUI as D2D/D3D overlay
- [ ] Visual ROM picker (replace/supplement native FileOpenPicker)
- [ ] Modern config UI (replace Pure text-mode OSD)
- [ ] Touch-friendly virtual keyboard
- [ ] Visual save state manager
- [ ] **Result:** Modern GUI overlay on DOSBox

---

## Phase F — Save States (Change 12)
**Goal: Save/load game state**
**Status: 🔲 NOT STARTED**

### Change: `save-states`
**Files:** `dosbox-uwp/dosbox_pure_sta.cpp`, `dosbox-uwp/Content/RetroCore.cpp`

- [ ] `retro_serialize_size()` — query state size
- [ ] `retro_serialize(void* data, size_t size)` — serialize state
- [ ] `retro_unserialize(const void* data, size_t size)` — restore state
- [ ] `DBPS_RequestSaveLoad` — trigger save/load from core
- [ ] `DBPS_HaveSaveSlot` — query available slots
- [ ] Hotkeys: F5 save, F7 load (or via Puremenu)
- [ ] Auto-save on suspend/shutdown
- [ ] SRAM (`retro_get_memory_data/size`) if core uses it
- [ ] **Result:** Save states functional

---

## Dependencies Between Changes

```
Phase 0 (scaffold) ────────────────── ✅ done
    │
    ▼
Phase 1 (core compiles) ───────────── ✅ done
    │
    ▼
Phase A ── patch-fopen-vfs ────────── 🔲 BLOCKS EVERYTHING
    │         │
    │         ├── fix-keyboard-input ── 🔲 unblocks launcher navigation
    │         └── fix-gamepad-input ─── 🔲 unblocks gamepad
    │
Phase B ── register-core-options ──── 🔲 unblocks launcher + config
    │         │
    │         └── software-framebuffer ── 🔲 OSD overlay
    │
Phase C ── wire-audio ─────────────── 🔲 independent
    │
Phase D ── dbps-mouse ─────────────── 🔲 independent
    │         │
    │         ├── dbps-platform ──────── 🔲 depends on mouse
    │         └── disk-control ───────── 🔲 independent
    │
Phase E ── video-enhancements ─────── 🔲 depends on options
    │         │
    │         └── rmlui-overlay ──────── 🔲 depends on almost everything
    │
Phase F ── save-states ────────────── 🔲 independent (but complex)
```

---

## OpenSpec Changes

| Change | Status | OpenSpec Path |
|--------|--------|---------------|
| `patch-fopen-vfs` | 🔲 created | `openspec/changes/patch-fopen-vfs/` |
| `fix-keyboard-input` | 🔲 created | `openspec/changes/fix-keyboard-input/` |
| `fix-gamepad-input` | 🔲 created | `openspec/changes/fix-gamepad-input/` |
| `register-core-options` | 🔲 created | `openspec/changes/register-core-options/` |
| `software-framebuffer` | 🔲 created | `openspec/changes/software-framebuffer/` |
| `wire-audio` | 🔲 created | `openspec/changes/wire-audio/` |
| `dbps-mouse` | 🔲 created | `openspec/changes/dbps-mouse/` |
| `dbps-platform` | 🔲 created | `openspec/changes/dbps-platform/` |
| `disk-control` | 🔲 created | `openspec/changes/disk-control/` |
| `video-enhancements` | 🔲 created | `openspec/changes/video-enhancements/` |
| `rmlui-overlay` | 🔲 created | `openspec/changes/rmlui-overlay/` |
| `save-states` | 🔲 created | `openspec/changes/save-states/` |
