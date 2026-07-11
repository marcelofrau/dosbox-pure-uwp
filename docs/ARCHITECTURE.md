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
│   ├── dosbox_pure_sta.cpp             ← DBPS_* stubs (12 no-ops, 1 real)
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

### Audio
- `retro_audio_sample_batch(left, right, samples)` — stereo PCM16
  - Submitted to XAudio2 via `XAudio2Output::Submit()`
  - Ring buffer with `OnBufferEnd` callback
  - Queue-depth cap at 882 frames (~20ms) — flush/restart cycle bounds latency

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
  - `RETRO_ENVIRONMENT_GET_THROTTLE_STATE` → `RETRO_THROTTLE_NONE`

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
retro_run() → 630 stereo frames per call (~70fps @ 44100Hz)
    ↓
retro_audio_sample_batch_cb() → store in s_audioBuffer
    ↓
XAudio2Output::Submit(samples, frames)
    ↓
Alloc ring buffer → memcpy → IXAudio2SourceVoice::SubmitSourceBuffer()
    ↓
OnBufferEnd() callback → InterlockedDecrement(s_queuedFrames)
    ↓
Voice consumes at 44100Hz hardware rate
```

**Pacing:** QPC-based multi-retro_run per visual frame. Accumulator tracks time, runs `retro_run()` 1.17× per 60fps visual frame to produce 70fps aggregate audio rate. Queue-headroom-based `maxRetroRuns` prevents overshoot. Emergency catch-up when queue < 500 frames.

## FrontendMenu (BIOS-style overlay)

Renders as a D2D overlay on top of the spinning cube (no game loaded) or retro framebuffer (game running). Gamepad-navigable via DPad + A/B.

```
Main Page:
  Continue Game        → hide menu, resume core
  Load Game            → open FileBrowser
  Puremenu             → open core's PUREMENU (OSD settings)
  Settings             → future submenu
  Exit                 → close app

Input:
  DPad Up/Down         → navigate
  A                    → confirm
  B                    → back / close
  Start                → toggle menu
  Mouse click          → select + confirm
  PageUp/PageDown      → scroll items by MAX_VISIBLE
```

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
