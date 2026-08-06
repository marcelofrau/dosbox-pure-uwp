<p align="center">
  <img src="docs/images/social-preview.jpg" alt="DOSBox Pure - Xbox UWP Port" width="800"/>
</p>

<p align="center">
  <strong>DOS games on Windows and Xbox — no RetroArch, no fuss.</strong>
</p>

<p align="center">
  <img alt="Status" src="https://img.shields.io/badge/status-playable-yellow?style=for-the-badge">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20Xbox-blue?style=for-the-badge">
  <img alt="Build" src="https://img.shields.io/badge/build-0%20errors-brightgreen?style=for-the-badge">
  <img alt="Arch" src="https://img.shields.io/badge/architecture-x64%20only-lightgrey?style=for-the-badge">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--2.0-red?style=for-the-badge">
</p>

---

## What is this?

A standalone UWP port of [dosbox-pure](https://github.com/schellingb/dosbox-pure) — the best DOS emulator for retro gaming. Runs natively on **Windows 11** and **Xbox Series (Dev Mode)** without needing RetroArch or any other frontend.

The emulation core is identical to the one used in RetroArch. What's different is everything around it: a custom menu system, an in-app file browser, native audio, and gamepad/keyboard/mouse input — all built specifically for the UWP platform.

---

## Quick Start

### 1. Install

**Windows:**
Download the latest release `.msix` from [Releases](https://github.com/marcelofrau/dosbox-pure-unleashed-uwp/releases). Double-click to install (sideloading must be enabled).

**Xbox:**
Enable Dev Mode on your Xbox. Deploy the `.msix` via [Xbox Device Portal](https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/features/live/testing-on-xbox-devkits) or use the deploy script (see [Building from Source](#building-from-source)).

### 2. Add Games

The app includes an in-app file browser (press **A** on "Load File" or navigate to your game folders). Supported formats:

| Format | Description | Example |
|--------|-------------|---------|
| `.zip` | ZIP archive containing game files | `DOOM.ZIP` |
| `.dosz` | DOSBox Pure compressed archive | `QUAKE.DOSZ` |
| `.exe` | DOS executable | `DUKE3D.EXE` |
| `.com` | DOS COM file | `TETRIS.COM` |
| `.bat` | DOS batch file | `INSTALL.BAT` |
| `.iso` | CD-ROM image | `WING COMMANDER.ISO` |
| `.chd` | Compressed Hunks of Data (CD) | `COMMAND&CONQUER.CHD` |
| `.cue` + `.bin` | CUE sheet + BIN image | `STUNT_ISLAND.CUE` |
| `.img` / `.ima` | Floppy disk image | `WOLF3D.IMG` |
| `.vhd` | Virtual Hard Disk | `WINDOWS31.VHD` |
| `.conf` | DOSBox configuration file | `custom.conf` |

### 3. Play

Connect a gamepad or use keyboard/mouse. See [Controls](#controls) below.

---

## Creating Compatible ZIP Files

The easiest way to package a DOS game is as a `.zip` file. DOSBox Pure mounts the ZIP as a virtual C: drive.

### Basic ZIP (single-directory game)

Many DOS games have all files in one folder. Just ZIP the whole folder:

```
DOOM/
├── DOOM.EXE
├── DOOM.WAD
├── SETUP.EXE
└── README.TXT
```

**Windows:** Right-click the folder → Send to → Compressed (zipped) folder → rename to `DOOM.ZIP`

**Command line:**
```bash
# From inside the DOOM folder:
cd DOOM
powershell -Command "Compress-Archive -Path * -DestinationPath ../DOOM.ZIP"
```

### ZIP with subdirectories

Games with subdirectories work too. DOSBox Pure preserves the folder structure:

```
DUKE3D.ZIP
├── DUKE3D/
│   ├── DUKE3D.EXE
│   ├── DUKE3D.GRP
│   └── SETUP.EXE
└── README.TXT
```

When you load `DUKE3D.ZIP`, the app shows `DUKE3D.EXE` at the root. Select it and the game runs.

### Multiple disks in one ZIP

Games that span multiple floppy disks (like many Sierra adventures) can be packaged as:

```
KQ5.ZIP
├── DISK1/
│   ├── INSTALL.BAT
│   └── ...
├── DISK2/
│   └── ...
└── DISK3/
    └── ...
```

DOSBox Pure's built-in menu (PUREMENU) lets you swap disks during gameplay.

### Best practices

- **Use UPPERCASE** for filenames and extensions — most DOS games expect this
- **Include the launcher** — if the game has `INSTALL.BAT` or `SETUP.EXE`, include it
- **Don't ZIP the ZIP** — avoid nesting ZIPs inside ZIPs
- **Max 4GB** total ZIP size recommended for Xbox (storage limits)
- **`.dosz` format** — if you want smaller files, use the [dosz tool](https://github.com/schellingb/dosbox-pure#dosz-format) for better compression

---

## Controls

### Gamepad

| Button | Action |
|--------|--------|
| **D-Pad** | Navigate menus / arrow keys in-game |
| **A** | Confirm / primary action |
| **B** | Back / cancel |
| **X** | Varies by game |
| **Y** | Varies by game |
| **LB / RB** | Page up/down in menus / disk swap |
| **LT / RT** | Triggers (mapped per game) |
| **Left Stick** | Menu/OSD cursor + in-game mouse (mouse mode ON) |
| **Right Stick** | Scroll / secondary input |
| **Start** | Gamepad Start (mapped via Generic Keyboard preset) |
| **Select** | Gamepad Select (mapped via Generic Keyboard preset) |
| **R3** | PUREMENU (in-game settings) |
| **LB+RB+Select** (hold) | Toggle gamepad mouse mode (left stick → DOS mouse) |

### Keyboard

Standard DOS keyboard mapping. All keys work as expected: arrows, Enter, Escape, F1-F12, Ctrl, Shift, Alt, Tab, etc.

### Mouse

Connected via USB or emulated through left stick. Works in all DOS games that use mouse input (Doom, Duke Nukem 3D, etc.). In-game mouse emulation is a **toggle**: hold **LB+RB+Select** to switch the left stick from game analog to DOS mouse (A=Enter, B=Escape). The left stick always drives the menu and PUREMENU cursor.

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **F10** | Toggle PUREMENU in-game / visual-only hint on the start screen |
| **F12** | Reserved (swallowed) |
| **Ctrl+L** | Open the file browser (load a game) |
| **Alt** | Forwarded to the game (menu bar / Alt key) |

---

## Features

| Feature | Status |
|---------|--------|
| DOS emulation (CPU, memory, sound) | ✅ Done |
| Dynamic recompiler (JIT, 5-10x speed) | ✅ Done |
| D2D video pipeline with letterbox | ✅ Done |
| XAudio2 audio output (low latency, 48ms ring) | ✅ Done |
| Fullscreen settings menu (8 pages) | ✅ Done |
| In-app file browser (gamepad+mouse) | ✅ Done |
| Recent games history + startup folder | ✅ Done |
| PUREMENU (in-game OSD, 640x480) | ✅ Done |
| Gamepad input (Xbox controller) | ✅ Done |
| Gamepad mouse emulation (LB+RB+Select) | ✅ Done |
| Keyboard input (full DOS mapping) | ✅ Done |
| Mouse input (USB + stick emulation) | ✅ Done |
| Settings persistence (dosbox-pure-settings.json) | ✅ Done |
| Per-game config overrides (FRONTEND.DBP) | ✅ Done |
| Multi-disc support (CD swap) | ✅ Done |
| ZIP/ISO/CHD mounting | ✅ Done |
| Self-signed packaging (MSIX) | ✅ Done |
| Xbox deployment (WDP REST API) | ✅ Done |
| Save states (menu shell) | ⏳ In progress |
| Network play (IPX) | ⏳ Planned |

---

## Building from Source

### Prerequisites

- **Visual Studio 2022** (v17.x, not v18 preview)
- **Windows SDK 10.0.26100.0**
- **x64 only** — ARM/ARM64/x86 not supported (Xbox Series is x64)

### Build

```powershell
MSBuild.exe "dosbox-pure-unleashed-uwp.sln" /p:Configuration=Release /p:Platform=x64 /nowarn:MSB4011
```

Or use the build script:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

### Package (MSIX)

```powershell
.\scripts\package.ps1 -Configuration Release -Platform x64
```

Auto-creates a self-signed certificate if none exists.

### Run (Windows)

```powershell
.\scripts\run.ps1 -Configuration Release -Platform x64
```

Builds, registers, and launches the app.

### Deploy (Xbox)

```powershell
.\scripts\deploy-xbox.ps1 -Configuration Release -Platform x64 -XboxIp 10.0.0.98
```

Uploads MSIX + dependencies via Xbox Device Portal REST API.

---

## Project Structure

```
dosbox-pure-unleashed-uwp/
├── dosbox-uwp/                       ← UWP frontend (our code)
│   ├── App.cpp/h                     ← Entry point, hotkeys (F10/F12/Ctrl+L/Alt)
│   ├── dosbox_uwpMain.cpp/h          ← Main loop, input routing, mouse emulation
│   ├── Content/
│   │   ├── RetroCore.cpp/h           ← libretro bridge: init/load/run/callbacks/VFS
│   │   ├── RetroScreenRenderer.cpp/h ← D2D bitmap + letterbox rendering
│   │   ├── XAudio2Output.cpp/h       ← XAudio2 audio: 4×12ms follower ring
│   │   ├── FrontendMenu.cpp/h        ← Fullscreen DOS-style settings menu (8 pages)
│   │   ├── FileBrowser.cpp/h         ← In-app file explorer (D2D)
│   │   └── SdlInput.cpp/h            ← SDL gamepad + UWP fallback
│   ├── dosbox_pure_sta.cpp           ← DBPS_* stubs (GetMouse + ApplyConfigOverrides real)
│   ├── local/dosbox-pure/            ← Patched core files (UWP compat)
│   └── Package.appxmanifest          ← UWP manifest + capabilities
├── extern/
│   ├── dosbox-pure/                  ← Submodule: emulation core (unmodified)
│   ├── libretro-common/              ← VFS + UWP helpers (selective copy)
│   └── uwp-xray-depot/              ← TCP diagnostics + Lua REPL (Debug)
├── scripts/                          ← Build, package, deploy scripts
├── docs/                             ← Documentation
│   ├── ARCHITECTURE.md               ← Technical deep-dive
│   ├── ROADMAP.md                    ← Development phases
│   ├── DYNAREC_UWP.md               ← JIT compiler on UWP
│   ├── discoveries.md               ← Bug investigations & fixes
│   ├── filebrowser/                 ← File browser docs
│   └── frontend/                    ← FrontendMenu docs
└── dosbox-pure-unleashed-uwp.sln     ← Solution file
```

---

## For Developers

This section covers the architecture for contributors.

### How It Works

The app is a **libretro frontend**. The dosbox-pure core (in `extern/dosbox-pure/`) handles all emulation. Our code provides:

1. **Video** — Core calls `retro_video_refresh_cb()` with an XRGB8888 framebuffer. We copy it to a D2D bitmap and render with letterboxing.

2. **Audio** — Core calls `retro_audio_sample_batch()` with stereo PCM16. We submit to XAudio2 with a fixed 4×12ms = 48ms follower ring (`OnBufferEnd` callback). Frame pacing is a QPC timer (`PaceFrame`) at the same rate reported by `GET_THROTTLE_STATE`, so the ring stays balanced and only absorbs drift.

3. **Input** — `retro_input_poll()` reads gamepad/keyboard/mouse state. `retro_input_state()` returns button states. A generic-keyboard preset maps gamepad buttons to RetroPad IDs; R3 toggles PUREMENU, LB+RB+Select toggles gamepad mouse mode, and the left stick drives the menu/OSD pointer.

4. **Environment** — `retro_environment()` handles VFS, configuration (`GET_VARIABLE`/`SET_VARIABLE`/`GET_VARIABLE_UPDATE`), throttle state (`RETRO_THROTTLE_VSYNC` when vsync caps below core fps), hardware render rejection (SW path forced), and keyboard callbacks.

### Key Technical Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Render API | Direct2D | No OpenGL/ANGLE needed, `ID2D1Bitmap1` accepts XRGB8888 directly |
| Audio API | XAudio2 (native) | Low latency, no SDL dependency |
| VFS | RetroArch copy (`CreateFile2FromAppW`) | Already proven on UWP sandbox |
| File picker | Custom D2D file browser | Gamepad-navigable, replaces UWP FileOpenPicker |
| Language | C++/CX | Scaffold already uses it; C++/WinRT would be rewrite |
| HW render | Rejected (`SET_HW_RENDER` returns 0) | No OpenGL context on UWP; core auto-fallbacks to SW |
| Dynarec | Enabled via `VirtualAllocFromApp` | UWP-compatible JIT without AppContainer restrictions |

### Build System

- **Solution file** required (not `.vcxproj` directly) — `$(SolutionDir)` needed for `uwp-dep.props` SDL paths
- Core files: `CompileAsWinRT=false` (legacy C, no `/ZW`)
- C++/CX files: `/ZW` enabled
- ~1500 warnings C4244 (cosmetic, from dosbox-pure source)

### Submodule Policy

- **Never commit to `extern/dosbox-pure/`** — patches go in `dosbox-uwp/local/dosbox-pure/`
- Patches mirror the same directory structure as the submodule
- **OK to commit to `extern/uwp-xray-depot/`** — same author/owner

### Known Gotchas

| Issue | Detail |
|-------|--------|
| STA `.get()` crash | Never call `.get()` on `IAsyncOperation` from STA — use `.then()` chaining |
| D2D `BeginDraw/EndDraw` | `DrawBitmap()` crashes without explicit begin/end |
| HW frame valid check | `pitch==0` = `RETRO_HW_FRAME_BUFFER_VALID`, skip `memcpy` |
| Pixel format | Framebuffer is raw XRGB8888 — use `D2D1_ALPHA_MODE_IGNORE` |
| DPI matching | Always `GetDpi()` from render target, never hardcode 96 |
| File access | UWP sandbox: use `StorageFile^` from picker or `broadFileSystemAccess` |
| SET_HW_RENDER | Return 0 to force SW path (no OpenGL context) |
| Xbox B button | System `BackRequested` event — routed to file browser or menu |

---

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](docs/ARCHITECTURE.md) | Technical deep-dive: dependency graph, rendering pipeline, libretro interfaces |
| [Roadmap](docs/ROADMAP.md) | Development phases with detailed status |
| [Dynarec on UWP](docs/DYNAREC_UWP.md) | JIT compiler setup and performance |
| [Discoveries](docs/discoveries.md) | Bug investigations, audio pacing analysis, technical debt |
| [Release Notes](release_notes.md) | Version history and changelog |
| [PUREMENU Theming](docs/PUREMENU-THEMING.md) | Theme system for the in-game OSD menu |
| [File Browser](docs/filebrowser/README.md) | In-app file explorer design and implementation |
| [FrontendMenu](docs/frontend/FrontendMenu.md) | DOS-style menu system |
| [References](docs/REFERENCES.md) | Research findings, libretro interfaces, VFS implementation |

---

## Dependencies & Credits

This project builds on the work of several open-source projects:

### Core Emulation

| Project | Role | License |
|---------|------|---------|
| [dosbox-pure](https://github.com/schellingb/dosbox-pure) | DOS emulation core (libretro). Handles CPU, memory, sound, input, disk mounting. Included as git submodule at `extern/dosbox-pure/`. | GPL-2.0 |
| [libretro-common](https://github.com/libretro/libretro-common) | Shared libretro utilities. We use the VFS (Virtual File System) implementation for UWP file access via `CreateFile2FromAppW`. Selective copy in `extern/libretro-common/`. | MIT |

### UWP Platform

| Project | Role | License |
|---------|------|---------|
| [RetroArch UWP](https://github.com/XboxEmulationHub/RetroArch) | Reference for UWP VFS implementation, file picker patterns, and Xbox deployment. The VFS file (`vfs_implementation_uwp.cpp`) is adapted from this project. | GPL-3.0 |

### Development Tools

| Project | Role | License |
|---------|------|---------|
| [xb-xray](https://github.com/marcelofrau/uwp-xray-depot) | Remote diagnostics for UWP homebrews on Xbox Dev Mode. Provides real-time log streaming, a Lua REPL, and live C++ variable inspection over TCP. Debug-only (`#ifdef XB_INSPECTOR_ENABLED`), zero overhead in Release builds. Included as submodule at `extern/uwp-xray-depot/`. | MIT |
| [spdlog](https://github.com/gabime/spdlog) | Fast C++ logging library. Used via xb-xray for log streaming to file + TCP + OutputDebugString simultaneously. Header-only, included as xb-xray dependency. | MIT |
| [Lua 5.4](https://www.lua.org/) | Embedded scripting language. Powers the xb-xray REPL — lets developers inspect and modify C++ variables in real time from a terminal. Prebuilt static lib via xb-xray. | MIT |

### About xb-xray

Xbox UWP development is painful — the Visual Studio Remote Debugger disconnects constantly, and the Xbox Device Portal web UI is slow. **xb-xray solves this** by streaming diagnostics over TCP.

When enabled (Debug builds only), the app opens a TCP socket on port 9000-9009. Connect from your dev PC:

```bash
# Quick connect with netcat:
nc <xbox-ip> 9000

# Or Python CLI (recommended):
pip install xb-connector
python -m xb_connector.cli <xbox-ip>
```

What you can do:
- **Live logs** — see `spdlog::info()` output in real time, no Visual Studio needed
- **Inspect variables** — bind C++ variables (`fps`, `audio_queued`, `frame_ms`) and read them live
- **Modify at runtime** — change variable values from the Lua REPL to tweak behavior without redeploying
- **Lua scripting** — full Lua 5.4 (math, strings, control flow) with sandboxed access to game state

This project binds: `audio_queued` (queue depth), `fps`, `target_fps`, `frame_ms`, and timing breakdowns (`poll_ms`, `hud_ms`, `render_ms`, `total_ms`).

### Historical Reference

| Project | Role | Note |
|---------|------|------|
| [dosbox-pure-unleashed](https://github.com/marcelofrau/dosbox-pure-unleashed) | Original desktop frontend using ZillaLib. Reference for core integration patterns and build configuration. Not used at runtime. | Archived |
| [ZillaLib](https://github.com/Jereq/ZillaLib) | Cross-platform game framework used by dosbox-pure-unleashed. Replaced by our UWP scaffold (DirectX 11 + D2D). | Reference only |

---

## License

This project is based on [dosbox-pure](https://github.com/schellingb/dosbox-pure) by [@schellingb](https://github.com/schellingb).

**Licensed under the GNU General Public License v2.0 (GPL-2.0)** — same as dosbox-pure.

This license applies to all source code in this repository, including the UWP frontend, patched core files, and build scripts. See [extern/dosbox-pure/LICENSE](extern/dosbox-pure/LICENSE) for the full license text.

Third-party components retain their original licenses:
- xb-xray, spdlog, Lua 5.4: MIT
- libretro-common: MIT
- RetroArch UWP (reference only, not linked): GPL-3.0
