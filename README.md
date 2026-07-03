<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/status-work--in--progress-yellow?style=for-the-badge">
  <img alt="Status" src="https://img.shields.io/badge/status-work--in--progress-yellow?style=for-the-badge">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/platform-Windows%20%7C%20Xbox-blue?style=for-the-badge">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20Xbox-blue?style=for-the-badge">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/build-0%20errors-brightgreen?style=for-the-badge">
  <img alt="Build" src="https://img.shields.io/badge/build-0%20errors-brightgreen?style=for-the-badge">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/architecture-x64%20only-lightgrey?style=for-the-badge">
  <img alt="Arch" src="https://img.shields.io/badge/architecture-x64%20only-lightgrey?style=for-the-badge">
</picture>

# dosbox-pure-uwp

**DOSBox-Pure libretro core ported to UWP — runs on Windows 11 and Xbox Series (Dev Mode).**

This is a ground-up port of the [dosbox-pure](https://github.com/schellingb/dosbox-pure) emulator as a native UWP app. No RetroArch wrapper — direct libretro frontend built with C++/CX, Direct2D, and SDL2 for audio/input.

Inspired by [dosbox-pure-unleashed](https://github.com/marcelofrau/dosbox-pure-unleashed-uwp) (original experiment), rewritten for standalone deployment.

---

## Features

| Area | Status |
|------|--------|
| libretro core integration | ✅ Done |
| D2D video pipeline (bitmap + letterbox) | ✅ Done |
| UWP file picker (F1 / R3+Select) | ✅ Done |
| Xbox controller input (A/B/X/Y/Select/R3) | ✅ Done |
| Signing & packaging (self-signed cert) | ✅ Done |
| Xbox deployment via WDP REST API | ✅ Done |
| Keyboard input mapping | ⏳ Blocked |
| Audio output (SDL_QueueAudio) | 📋 Planned |
| GET_VARIABLE handler (launcher UI) | 📋 Planned |
| Xbox Series S|X testing | 🔬 Partial |

---

## Build

### Prerequisites

- Visual Studio 2022 (v17, not v18 preview)
- Windows SDK 10.0.26100.0
- x64 only (ARM/ARM64/x86 not supported)

### Quick start

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

0 errors, ~1500 warnings C4244 (cosmetic, from dosbox-pure source).

### Package

```powershell
.\scripts\package.ps1 -Configuration Release -Platform x64
```

Generates MSIX + auto-creates signing certificate if missing.

### Run (Windows)

```powershell
.\scripts\run.ps1 -Configuration Release -Platform x64
```

Builds, extracts MSIX, registers, and launches via `IApplicationActivationManager`.

### Deploy (Xbox)

```powershell
.\scripts\deploy-xbox.ps1 -Configuration Release -Platform x64 -XboxIp 10.0.0.98
```

Uploads MSIX + dependencies via WDP REST API, installs, and launches.

---

## Project Structure

```
dosbox-uwp/
├── App.cpp/h            — Entry point, F1 picker, lifecycle
├── dosbox_uwpMain.cpp/h — Main loop, Update/Render, input routing
├── Content/
│   ├── RetroCore.cpp/h  — libretro bridge (init/load/run/callbacks)
│   ├── RetroScreenRenderer.cpp/h — D2D bitmap + letterbox
│   └── SdlInput.cpp/h   — SDL + UWP gamepad polling
├── Package.appxmanifest — UWP manifest (Windows.Universal + Windows.Xbox)
├── dosbox-uwp.vcxproj   — Signing config, .NET Native off
├── local/dosbox-pure/   — Patched core (mirrors submodule structure)
└── uwp-dep.props        — SDL2/libuwp paths
scripts/
├── build.ps1            — MSBuild wrapper (VS2022 path)
├── package.ps1          — Build + MSIX + cert auto-creation
├── deploy-xbox.ps1      — WDP REST API deploy to Xbox
├── run.ps1              — Build + register + launch
└── install.ps1          — MSIX extract + register
extern/
├── dosbox-pure/         — Submodule (source, unmodified)
├── libretro-common/     — Submodule (VFS, mem, etc.)
└── uwp-dep/             — SDL2 + libuwp prebuilt binaries
```

---

## Known Pitfalls

| Issue | Detail |
|-------|--------|
| STA `.get()` crash | Never call `.get()` on `IAsyncOperation` from STA — use `.then()` |
| D2D `BeginDraw/EndDraw` | `DrawBitmap()` crashes without explicit begin/end |
| HW frame valid check | `pitch==0` = `RETRO_HW_FRAME_BUFFER_VALID`, skip memcpy |
| Pixel format | Framebuffer is raw XRGB8888 — use `D2D1_ALPHA_MODE_IGNORE` |
| DPI matching | Always `GetDpi()` from render target, never hardcode 96 |
| File access | UWP sandbox: use `StorageFile^` from picker, not raw paths |
| SET_HW_RENDER | Return 0 to force SW path (no OpenGL context on UWP) |
| Xbox B button | System `BackRequested` event — suppressed via handler |
| DEP6701 | VS2022 deploy to Xbox broken; use `deploy-xbox.ps1` instead |

---

## License

This project is based on [dosbox-pure](https://github.com/schellingb/dosbox-pure) by [@schellingb](https://github.com/schellingb), licensed under GPL-2.0.

Additional code in this repository is also licensed under **GPL-2.0**.
