# Architecture — DOSBox Pure Unleashed UWP

## Overview

UWP port of DOSBox Pure. Replaces the ZillaLib desktop platform layer with a DirectX 11 + D2D UWP scaffold. The emulation core (`dosbox-pure`) runs unmodified as a libretro core — our app acts as the libretro frontend.

**Target platforms:** Windows 11 (x64), Xbox Series (Dev Mode, x64).

## Project Structure

```
dosbox-pure-unleashed-uwp/
├── dosbox-uwp/                         ← UWP frontend (our code)
│   ├── App.cpp/h                       ← Entry point, Ctrl+L fallback, BackRequested
│   ├── dosbox_uwpMain.cpp/h            ← Main loop, input routing, audio pacing
│   ├── Content/
│   │   ├── RetroCore.cpp/h             ← libretro bridge: init/load/run/callbacks/VFS
│   │   ├── RetroScreenRenderer.cpp/h   ← D2D bitmap + letterbox rendering
│   │   ├── XAudio2Output.cpp/h         ← XAudio2 audio: ring buffer, queue cap
│   │   ├── FrontendMenu.cpp/h          ← DOS-style BIOS menu overlay
│   │   ├── FileBrowser.cpp/h           ← In-app file explorer (D2D, gamepad+mouse)
│   │   └── SdlInput.cpp/h              ← SDL gamepad + UWP Gamepad API fallback
│   ├── dosbox_pure_sta.cpp             ← DBPS_* stubs (GetMouse + ApplyConfigOverrides real)
│   ├── local/dosbox-pure/              ← Patched core files (UWP compat)
│   └── Package.appxmanifest            ← UWP manifest + capabilities
├── extern/
│   ├── dosbox-pure/                    ← Submodule: emulation core (unmodified)
│   ├── libretro-common/                ← VFS + UWP helpers (selective copy)
│   └── uwp-xray-depot/                ← TCP diagnostics + Lua REPL (Debug only)
├── scripts/                            ← Build, package, deploy
├── docs/                               ← Documentation
└── dosbox-pure-unleashed-uwp.sln       ← Solution file
```

## Dependency Graph

```
┌──────────────────────────────────────────────────────────────┐
│                     dosbox-uwp (UWP App)                     │
│        (C++/CX, Direct2D, DirectWrite, XAudio2)             │
├──────────────────────┬───────────────────────────────────────┤
│   libretro frontend  │        Platform layer                 │
│   ┌───────────────┐  │  ┌──────────────────────────────────┐ │
│   │ retro_run()   │  │  │ XAudio2 (audio output)           │ │
│   │ retro_video_  │  │  │ D2D + DWrite (rendering)         │ │
│   │   refresh_cb  │  │  │ UWP Gamepad API (Xbox fallback)  │ │
│   │ retro_input_  │  │  │ SDL_GameController (gamepad)     │ │
│   │   poll/state  │  │  │ CoreWindow (keyboard/mouse)      │ │
│   │ retro_audio_  │  │  │ VFS via CreateFile2FromAppW      │ │
│   │   sample_batch│  │  └──────────────────────────────────┘ │
│   └───────┬───────┘  │                                      │
├───────────┼──────────┴───────────────────────────────────────┤
│           │                                                  │
│  ┌────────▼────────┐     ┌──────────────────┐               │
│  │  dosbox-pure    │     │  xb-xray         │               │
│  │  (libretro)     │     │  (Debug only)    │               │
│  │  submodule      │     │  TCP diagnostics │               │
│  └─────────────────┘     └──────────────────┘               │
│  ┌─────────────────┐                                        │
│  │  libretro-common│  ← VFS UWP impl                       │
│  └─────────────────┘                                        │
└──────────────────────────────────────────────────────────────┘
```

## Libretro Frontend — What We Implement

The core calls these callbacks; our frontend provides them:

### Video
- `retro_video_refresh_cb(data, width, height, pitch)`
  - **SW path** (our path): `data` = pointer to XRGB8888 framebuffer in RAM
  - Copied to `ID2D1Bitmap1` and rendered with letterboxing via `DrawBitmap()`
  - `pitch == 0` = `RETRO_HW_FRAME_BUFFER_VALID` — skip (no HW context on UWP)
- **PUREMENU OSD**: `DBP_STANDALONE` is defined per-file only for the core TU
  (`dosbox_pure_libretro.cpp`, vcxproj override) → `dbp_osdbuf`/menu state exist; the
  patched core composites the OSD onto `buf.video` via `DBP_RenderOSD()` using a fixed
  **640x480** design buffer (`dbp_osdbuf`), nearest-neighbor scaled up/down. No
  `DBPS_SubmitOSDFrame` involved (no-op stub).

### Audio
- `retro_audio_sample_batch(left, right, samples)` — stereo PCM16
  - Submitted to XAudio2 via `XAudio2Output::Write()`
  - Fixed ring of **4 × 12ms = 48ms** sub-buffers (`OnBufferEnd` callback decrements
    a lock-free counter + signals an event); `Write()` blocks as a safety valve at
    `MAX_BUFFERS-1` queued, bounded by `WRITE_TIMEOUT` (256ms)
  - Ring is a pure **follower** — it absorbs drift, not the frame clock

### Input
- `retro_input_poll()` — reads gamepad/keyboard/mouse state
- `retro_input_state(port, device, index, id)` — returns button state
  - `RETRO_DEVICE_JOYPAD` → `s_joypadState[16]` (separate from keyboard)
  - `RETRO_DEVICE_KEYBOARD` → `s_keyboardState[RETROK_LAST]`
  - `RETRO_DEVICE_MOUSE` / `RETRO_DEVICE_POINTER` → mouse/pointer events

### Environment
- `retro_environment(cmd, data)` — responds to core requests:
  - `RETRO_ENVIRONMENT_GET_VFS_INTERFACE` → UWP VFS (CreateFile2FromAppW)
  - `RETRO_ENVIRONMENT_GET_VARIABLE` → returns from `s_optionValues` map
  - `RETRO_ENVIRONMENT_SET_VARIABLE` → stores in map, sets dirty flag
  - `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` → returns dirty flag
  - `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` → `ApplicationData.Current.LocalFolder`
  - `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY` → `ApplicationData.Current.LocalFolder`
  - `RETRO_ENVIRONMENT_SET_HW_RENDER` → **returns 0** (forces SW path)
  - `RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK` → pushes held key states
  - `RETRO_ENVIRONMENT_SET_MESSAGE_EXT` → log via spdlog
  - `RETRO_ENVIRONMENT_GET_THROTTLE_STATE` → `RETRO_THROTTLE_VSYNC` with `rate=refresh`
    only when vsync is on and `refresh < core_fps`; otherwise `RETRO_THROTTLE_NONE` with
    `rate=core_fps`. Must stay bit-for-bit consistent with the rate `PaceFrame()` picks.

### VFS (Virtual File System)
Interface the core uses to read files (ROMs, saves, config). UWP implementation adapted from RetroArch:
- Uses `CreateFile2FromApp()` from Win10 SDK
- Functions: open, close, read, write, seek, tell, size, stat, mkdir, opendir, readdir
- Paths relative to UWP sandbox (`LocalFolder`, `InstalledLocation`)

## Rendering Pipeline

```
retro_run()
    ↓
Core renders frame to RAM framebuffer (XRGB8888)
    ↓
retro_video_refresh_cb(data, w, h, pitch)
    ↓
pitch == 0? → REJECT (RETRO_HW_FRAME_BUFFER_VALID, log and discard)
    ↓
Copy pixels → std::vector (with mutex)
    ↓
Render(): GrabVideoFrame() → Get frame from mutex buffer
    ↓
RetroScreenRenderer::UpdateVideoFrame(data, w, h, pitch)
    ↓
memcpy → ID2D1Bitmap1 (DXGI_FORMAT_B8G8R8A8_UNORM, ALPHA_MODE_IGNORE)
    ↓
RetroScreenRenderer::Render()
    ↓
BeginDraw() → ComputeLetterbox() → DrawBitmap() → EndDraw()
    ↓
SwapChain::Present()
```

## Audio Pipeline

```
EmulationThreadMain
    ↓
RunFrame() → retro_run() → 48000/rate stereo frames per call
    ↓
retro_audio_sample_batch_cb() → store in s_audioBuffer
    ↓
XAudio2Output::Write(samples, frames)
    ↓
Fill current 12ms sub-buffer → when full, submit via IXAudio2SourceVoice::SubmitSourceBuffer()
    (blocks at MAX_BUFFERS-1 queued = ~36-48ms steady-state, WRITE_TIMEOUT 256ms, then drops)
    ↓
OnBufferEnded() callback → InterlockedDecrement(m_buffers) + InterlockedIncrement64(m_totalConsumed) + SetEvent
    ↓
Voice consumes at 48000Hz hardware rate
```

**Pacing:** single `retro_run()` per tick, then `RetroCore::PaceFrame()` — a QPC
accumulator at the same self-consistent rate reported by `GET_THROTTLE_STATE`
(vsync ON → display refresh when `< core_fps`, else core fps; vsync OFF → core fps).
Waits with `CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` (~100µs;
`timeBeginPeriod` is not available in UWP). The ring is a follower; `Write()` blocking
is only a rare-overrun safety valve.

## FrontendMenu (fullscreen settings menu)

Fullscreen D2D overlay. Gamepad-navigable (DPad + A/B, LB/RB page). Mouse/left-stick
hover selection. Shows at startup (behavior from `dosbox_pure_menu_time`), stays
available as the app shell when no game is loaded.

```
Main Page:
  Load File      → open FileBrowser
  Settings       → General / Input / Performance / Video / System / Audio / State
  History        → recent games
  About          → app info
  Exit           → close app

Settings pages:
  General        → VSync, Frame Limiter (Off/60/70), Scaler (Nearest/Bilinear),
                   Startup Folder, Force Output FPS, Save States Support, Start Menu,
                   Debug Overlay, Reload Settings, Restart App
  Input          → L3 to Show Menu (OSK), Mouse Input Mode, Mouse Wheel bind, Mouse
                   Sensitivity (global + X), Auto Mappings, Keyboard Layout (26),
                   Analog Deadzone, Timed Intervals, Action Wheel Inputs
  Performance    → Emulated Performance (AUTO/MAX/8086..Athlon), Maximum Performance,
                   Performance Scale (20-200%), Limit CPU Usage, Performance Stats
  Video          → Graphics Chip (SVGA/VGA/EGA/CGA/Tandy/Hercules/PCjr), CGA/Hercules/
                   SVGA modes, SVGA Memory, 3dfx Voodoo (8MB/12MB/4MB/off), Aspect
                   Ratio Correction, Overscan Border
  System         → memory, machine details, etc.
  Audio          → SoundBlaster Type/Settings/Adlib, MIDI, GUS, Tandy, Swap Stereo,
                   per-channel volumes
  State          → save/load slot shell (wired later)
```

Controls: DPad navigate, A/Start confirm, B back, LB/RB page, left-stick/mouse hover.
In-game: **R3** toggles PUREMENU, **LB+RB+Select** toggles gamepad mouse mode.
When PUREMENU/OSK is active, A↔B are swapped so A=confirm / B=back.

## FileBrowser (In-app file explorer)

Custom D2D overlay styled like a DOS dialog. Navigable by gamepad and mouse. Replaces UWP `FileOpenPicker`.

```
Panel: 70% width (max 900px), 65% height (max 600px)
Position: left-aligned (covers FrontendMenu)
Background: semi-transparent black (alpha 0.55)

Navigation:
  DPad Up/Down         → move selection
  A                    → enter dir / select file
  B                    → parent dir / close
  LB/RB                → page up/down
  Mouse wheel          → scroll (3 items/notch)
  Mouse click          → select + confirm

File loading:
  FileBrowser::onFileSelected(path)
    → dosbox_uwpMain::QueueLoadRom(path, {})
    → async copy to LocalFolder/temp/ via StorageFile
    → ProcessPendingLoad() → LoadRom(localPath)
    → core initializes → retro_run() starts
```

Supported extensions: `.zip .dosz .exe .com .bat .iso .chd .cue .img .ima .vhd .conf`

## Key Technical Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Render API | Direct2D | No OpenGL/ANGLE needed; `ID2D1Bitmap1` accepts XRGB8888 via `CopyFromMemory` |
| D2D pixel format | `DXGI_FORMAT_B8G8R8A8_UNORM` + `ALPHA_MODE_IGNORE` | Framebuffer is raw XRGB8888; `PREMULTIPLIED` distorts colors |
| D2D DPI | `GetDpi()` from render target | Hardcoded 96.0f causes `D2DERR_BITMAP_BOUND_AS_TARGET` on high-DPI |
| Audio API | XAudio2 (native) | Low latency, no SDL dependency, ring buffer with `OnBufferEnd` callback |
| VFS | RetroArch copy (`CreateFile2FromAppW`) | Already proven on UWP sandbox |
| File picker | Custom D2D file browser | Gamepad-navigable, replaces UWP FileOpenPicker; uses `broadFileSystemAccess` |
| Language | C++/CX | Scaffold already uses it; C++/WinRT would be rewrite |
| Core | Submodule schellingb/dosbox-pure | Updates via `git pull`; patches in `local/dosbox-pure/` |
| libretro-common | Selective copy in `extern/` | Only ~10 files needed, not full repo |
| Platform | **x64 only** | ARM64/ARM/x86 not supported; Xbox Series is x64 |
| HW render | Rejected (`SET_HW_RENDER` return 0) | No OpenGL context on UWP; core auto-fallbacks to SW |
| Dynarec | Enabled via `VirtualAllocFromApp` | UWP-compatible JIT; patched `dyn_cache.h` in local copy |
| xb-xray | Debug-only TCP diagnostics | Real-time logs + Lua REPL for Xbox dev; `#ifdef XB_INSPECTOR_ENABLED` |
