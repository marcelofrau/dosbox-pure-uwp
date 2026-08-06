# Memory — DOSBox Pure Unleashed UWP

## Project Goal
Integrate libretro dosbox-pure core into UWP scaffold so DOS ROMs load and render via Direct2D.

## Key Constraints
- Submodule `extern/dosbox-pure/` is pristine — never commit to it.
- Patches go in `dosbox-uwp/local/dosbox-pure/` with same directory structure.
- `DBP_STANDALONE` **IS defined, per-file, only** for `local\dosbox-pure\dosbox_pure_libretro.cpp`
  (vcxproj `<ClCompile>` override, Debug+Release x64). All other TUs compile without it. The core is
  patched so libretro behavior survives: `audio_batch_cb` active, OSD composited onto `buf.video`
  via `DBP_RenderOSD` (640x480). The `dosbox_pure_osd.h` DBP_STANDALONE tail (`dbp_osdbuf`,
  `DBPS_ToggleOSD`/`IsShowingOSD`/`IsGameRunning`) comes from the core — never stub those.
- Build only **x64** UWP. ARM64, ARM, x86 NOT supported — Xbox Series is x64.
- Build via `.sln` not `.vcxproj` (needs `$(SolutionDir)` for uwp-dep).
- `CompileAsWinRT=false` for legacy C code; `/ZW` for C++/CX files.
- Tested on Windows 11 via VS2022. Xbox Series deploy not tested yet.

## Files

| File | Purpose |
|------|---------|
| `dosbox-uwp/Content/RetroCore.cpp/.h` | libretro bridge: init, load, run, callbacks, VFS |
| `dosbox-uwp/Content/RetroScreenRenderer.cpp/.h` | D2D bitmap render with letterboxing |
| `dosbox-uwp/dosbox_uwpMain.cpp/.h` | Main loop (Update/Render), input routing, gamepad mouse mode, pacing |
| `dosbox-uwp/App.cpp` | Entry point, hotkeys (F10 PUREMENU / F12 swallow / Ctrl+L FileBrowser / Alt) |
| `dosbox-uwp/dosbox_pure_sta.cpp` | DBPS_* stubs (GetMouse + ApplyConfigOverrides real; ~9 no-ops) |
| `dosbox-uwp/local/dosbox-pure/dosbox_pure_libretro.cpp` | Patched core (vs submodule) |
| `extern/libretro-common/vfs/vfs_implementation_uwp.cpp` | UWP VFS via `CreateFile2FromAppW` |

## Lessons Learned

### 1. D2D: BeginDraw/EndDraw mandatory
`ID2D1RenderTarget::DrawBitmap()` crashes unless wrapped in `BeginDraw()`/`EndDraw()`. D2D debug layer error: `"An attempt to render a primitive outside of BeginDraw/EndDraw"`. `BeginDraw()` returns `void` (not `HRESULT`), `EndDraw()` returns `HRESULT`.

### 2. RETRO_HW_FRAME_BUFFER_VALID = pitch==0
libretro HW core sends `retro_video_cb(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)` when using HW path. Without `pitch == 0` guard, `memcpy` with size 0 crashes. Log rejected frames.

### 3. Pixel format: IGNORE not PREMULTIPLIED
Framebuffer is raw XRGB8888. `D2D1_ALPHA_MODE_PREMULTIPLIED` distorts colors. Use `D2D1_ALPHA_MODE_IGNORE` for correct rendering.

### 4. DPI must match render target
Create bitmap with same DPI as D2D context (`GetDpi()`). Hardcoded 96.0f causes `D2DERR_BITMAP_BOUND_AS_TARGET` or rendering artifacts on high-DPI displays.

### 5. UWP STA: never .get() on WinRT async
`concurrency::create_task(WinRT IAsyncOperation^).get()` from STA thread throws `Concurrency::invalid_operation`. Use continuation chaining (`.then()`) or fire-and-forget tasks.

### 6. UWP file access: use StorageFile^, not path
`StorageFile::GetFileFromPathAsync(arbitraryPath)` fails with `AccessDeniedException`. The `StorageFile^` from `FileOpenPicker` carries access rights. Read file in picker continuation via `FileIO::ReadBufferAsync(file)` + `DataReader::FromBuffer`.

### 7. SET_HW_RENDER return 0
Return 0 in `retro_env` for `RETRO_ENVIRONMENT_SET_HW_RENDER` to force SW path. Returning 1 causes the core to use GL paths that crash without OpenGL context.

### 8. Logging prefix [dosbox-uwp]
All `OutputDebugStringA` logs prepend `[dosbox-uwp]` for DebugView filtering.

## Build: Release|x64
- `MSBuild.exe "solution.sln" /p:Configuration=Release /p:Platform=x64 /nowarn:MSB4011`
- 0 errors. Warnings: ~1500 C4244 cosmetic.
- **Version bump**: edit `version.props` (major.minor.patch) — PreBuildEvent auto-increments
  `build_counter.txt` and rewrites `Package.appxmanifest` + `version.txt`. Current: 1.0.0.x.

## Known Issues
- Save states: menu shell only (State page disabled; DBPS_RequestSaveLoad/HaveSaveSlot still no-op)
- Per-game FRONTEND.DBP overrides: `DBPS_ApplyConfigOverrides` parses flat `{"key":"value"}` only
- `DBPS_OnContentLoad` / `DBPS_SubmitOSDFrame` no-ops (OSD composite via `DBP_RenderOSD` is the real path)
- MIDI Win32 off on UWP (winmm.lib restricted)
- Xbox Series deploy not tested yet

## Audio (current, build 155+)
- Ring: **4 × 12ms = 48ms** (`XAudio2Output.h`), RetroArch xaudio.c follower model
- Frame clock: `RetroCore::PaceFrame()` QPC high-res waitable timer, rate self-consistent
  with `GET_THROTTLE_STATE` (VSYNC only when refresh < core fps)
- 48ms is the Xbox stability floor (4×8ms=24ms cracked, under=7-8/s)

## Input (current)
- Gamepad → RetroPad Generic Keyboard preset (16 buttons)
- **R3** → PUREMENU toggle; **LB+RB+Select** → gamepad mouse mode (default OFF)
- Left stick drives menu/OSD cursor always (`pointerContext` gate in dosbox_uwpMain.cpp)
- `DBPS_GetMouse` real → `RetroCore::GetPointer`
