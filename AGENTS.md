# AGENTS.md — DOSBox Pure Unleashed UWP

## Project
Integrate libretro dosbox-pure core into UWP scaffold. DOS emulator on Windows/Xbox.

## Language
All documentation, code, comments, and commit messages MUST be in English.
(User may converse in Portuguese or English; agent responds accordingly.)

## Critical Rules
- **NEVER commit or push** without explicit user request. Stage changes only. Wait for "commit", "push", "faz o commit", etc.
- **NEVER commit to submodule** `extern/dosbox-pure/`. Patches go in `dosbox-uwp/local/dosbox-pure/` mirroring same directory structure.
- **CAN commit to `extern/uwp-xray-depot/`** — same author/owner as parent repo. Push separately when asked.
- **Build via .sln** not .vcxproj. `$(SolutionDir)` needed for `uwp-dep.props` SDL paths.
- **x64 only.** ARM64/ARM/x86 NOT supported (Xbox Series is x64).
- **`CompileAsWinRT=false`** for legacy C (dosbox-pure source). `/ZW` for C++/CX files.
- **Dynarec enabled** via patched `dyn_cache.h` (`VirtualAllocFromApp` + W^X fix, see `docs/DYNAREC_UWP.md`).
- **InputTest workflow:** after editing `docs/tools/inputtest-better/inputtest.c` (or `hello.c`), regenerate `.exe` + `.dosz` in BOTH `docs/tools/inputtest-better/dist/` AND `E:\PC\DOSBoxPure\`. Open Watcom at `C:\Apps\OW`. Run `pwsh -NoProfile build.ps1` in that directory. Script does: wcc (compile) → wlink (link) → Compress-Archive (.dosz) → copy to `E:\PC\DOSBoxPure\`.

## Build
```powershell
MSBuild.exe "dosbox-pure-unleashed-uwp.sln" /p:Configuration=Release /p:Platform=x64 /nowarn:MSB4011
```
Current: 0 errors, ~1500 warnings C4244 (cosmetic).

## Key Files

| File | Purpose |
|------|---------|
| `dosbox-uwp/Content/RetroCore.cpp/.h` | libretro bridge: init, load, run, callbacks, retro_env, VFS |
| `dosbox-uwp/Content/RetroScreenRenderer.cpp/.h` | D2D bitmap render + letterbox |
| `dosbox-uwp/Content/XAudio2Output.cpp/.h` | XAudio2 audio output: ring buffer, OnBufferEnd, pre-buffer |
| `dosbox-uwp/dosbox_uwpMain.cpp/.h` | Main loop, Update/Render, input routing, audio init |
| `dosbox-uwp/App.cpp` | Entry point, Ctrl+Alt+F2 → FileOpenPicker → async file read → LoadRom |
| `dosbox-uwp/dosbox_pure_sta.cpp` | DBPS_* stubs (12 no-ops, 1 real: DBPS_SubmitOSDFrame, DBPS_GetMouse) |
| `dosbox-uwp/local/dosbox-pure/dosbox_pure_libretro.cpp` | Patched core (copied from submodule) |
| `extern/libretro-common/vfs/vfs_implementation_uwp.cpp` | UWP VFS via CreateFile2FromAppW |
| `dosbox-uwp/uwp-xray-depot.props` | xb-xray include/libs/defines (Debug only) |
| `extern/uwp-xray-depot` | xb-xray submodule: TCP diagnostics + Lua REPL |

## Known Bugs & Pitfalls

### 1. D2D: BeginDraw/EndDraw mandatory
`DrawBitmap()` crashes without `BeginDraw()`/`EndDraw()`. `BeginDraw()` returns `void`.

### 2. RETRO_HW_FRAME_BUFFER_VALID = pitch==0
Core sends `(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)` for HW frames. Guard `if (pitch == 0) return` or `memcpy` with size 0 crashes.

### 3. Pixel format: IGNORE not PREMULTIPLIED
Framebuffer is raw XRGB8888. `PREMULTIPLIED` distorts colors.

### 4. DPI must match D2D context
Always `GetDpi()` from render target. Hardcoded 96.0f breaks on high-DPI.

### 5. NEVER .get() on WinRT async from STA
`create_task(IAsyncOperation^).get()` from STA throws `Concurrency::invalid_operation`. Use `.then()` chaining.

### 6. StorageFile^ required, not path
`GetFileFromPathAsync(arbitraryPath)` throws `AccessDeniedException`. Read file in picker continuation via `FileIO::ReadBufferAsync(file)`.

### 7. SET_HW_RENDER return 0
Return 0 to force SW path. Returning 1 causes GL crash (no OpenGL context).

### 8. OSD overlay (PUREMENU) invisible with DBP_STANDALONE
`DBP_STANDALONE` defined in UWP project causes OSD to render to separate `dbp_osdbuf[]` instead of `buf.video`. `video_cb` sends `buf.video` which lacks OSD overlay. `DBPS_SubmitOSDFrame()` in `dosbox_pure_sta.cpp` is no-op stub.

**Fix:** In `dosbox-uwp/local/dosbox-pure/dosbox_pure_libretro.cpp`, comment out `#ifdef DBP_STANDALONE` block in `GFX_EndUpdate()` so OSD draws directly onto `buf.video` (non-standalone path).

### 9. Audio queue grows unbounded — QPC vs audio clock skew
`DoPacingSleep()` uses QPC + Sleep to target 70fps. Default Sleep granularity (~15.6ms) > framePeriod (14.3ms), causing cumulative drift → actual fps ~71-72 → audio queue grows ~630 frames/sec. Even after fixing timer resolution with `CreateWaitableTimerEx(HIGH_RESOLUTION)`, residual drift (~0.5fps) from variable processing time causes slow queue accumulation.

**Fix (current):** Queue-depth cap in `XAudio2Output::Submit()`. When `s_queuedFrames > 882` (~20ms), `Stop()` + `FlushSourceBuffers()` + resubmit fresh + `Start(0)`.

**Fix (future):** Audio-driven pacing using `GetState().SamplesPlayed` instead of QPC — see `docs/discoveries.md`.

### 10. PUREMENU option changes don't persist (register-core-options)
PUREMENU calls `RETRO_ENVIRONMENT_SET_VARIABLE` with new value, but our `GET_VARIABLE` returned 0 for all options except `menu_time`. Core always falls back to `def.default_value` → display shows default, check_variables() never re-reads changed values.

**Fix:** `RetroCore.cpp` now handles:
- `SET_VARIABLE`: stores key→value in `s_optionValues` map, sets dirty flag
- `GET_VARIABLE`: returns from map (menu_time override stays)
- `GET_VARIABLE_UPDATE`: returns dirty flag, clears it → triggers `check_variables()` next frame

PUREMENU changes now propagate: user changes option → stored in map → display re-reads correct value → next frame core applies via `check_variables()`.

**Remaining:** `DBPS_ApplyConfigOverrides` (FRONTEND.DBP per-game JSON override) still stub. Need JSON parser for full config-override persistence.

### 11. xb-xray (TCP diagnostics + Lua REPL)
Integrated via `extern/uwp-xray-depot` submodule + `uwp-xray-depot.props` (Debug only). `#define XB_INSPECTOR_ENABLED` guards all inspector code. Binds:
- `audio_queued` (long) — XAudio2 queue depth
- `fps` (float) — measured FPS
- `target_fps` (double) — core target FPS
- `frame_ms` (double) — frame processing time
- `poll_ms/hud_ms/render_ms/total_ms` (double) — detailed timing breakdown

Connect: `nc <ip> 9000` or use [XB Homebrew Vault](https://github.com/marcelofrau/xb-homebrew-vault).

## Defines
`__LIBRETRO__`, `DBP_STANDALONE`, `XB_INSPECTOR_ENABLED` (Debug only), `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`

## Logging
- **New code:** always use `spdlog::info("fmt", args...)` directly. NEVER `OutputDebugStringA`.
- **Existing code:** `LogHelper.h` macro redirects `OutputDebugStringA` → `LogPrint` → `spdlog::info`. Legacy temp-buffer pattern (`sprintf_s(buf)` + `OutputDebugStringA(buf)`) tolerated but not for new code.
- xb-xray streams logs over TCP (port 9000-9009) when connected.
- Tech debt: migrate all callsites to direct `spdlog::info` (see `docs/discoveries.md`).

## LSP / clangd
codedev MCP (clangd-based LSP) does NOT understand C++/CX (`^`, `ref new`, `Platform::`, `Windows::`).
All LSP errors reported in tool results for `.cpp`/`.h` with C++/CX are **false positives**.
Ignore them — actual build uses MSVC with `/ZW` and compiles fine.

## References
- dosbox-pure official docs: https://docs.libretro.com/library/dosbox_pure/
- Upstream repo: https://github.com/schellingb/dosbox-pure
- RetroArch UWP fork (VFS reference): https://github.com/XboxEmulationHub/RetroArch

## Reference Projects (local only)
- `F:\workspace\vs2022\dosbox-pure-unleashed` — ZillaLib-based dosbox-pure frontend (no UWP). Uses `SetFpsLimit(av.timing.fps)` for frame cap (default 70fps), `ZL_ApplicationUpdateTimingFps` for Sleep-based pacing + vsync matching. Main file: `main.cpp` (2147 lines).
- `F:\workspace\vs2022\ZillaLib` — ZillaLib cross-platform game framework. WP8 UWP path uses D3D11 pure (no D2D), `Present(1,0)` with `BufferCount=1`, `DXGI_SWAP_EFFECT_DISCARD`, `SetMaximumFrameLatency(1)`. Source: `Source/ZL_PlatformWP.cpp`.

## Keyboard & Gamepad Bindings

## Status
- Phase 0-3 done (scaffold, core compiles, libretro frontend, video pipeline)
- Phase 4 complete (keyboard callback, GET_LOG_INTERFACE)
- Phase 5 complete (XAudio2 output replaces SDL audio: alloc-per-submit ring buffer, OnBufferEnd callback, voice starts in Initialize, queue-depth cap at 882 frames/20ms)
- OSD fix: commented out DBP_STANDALONE separate-buffer path in GFX_EndUpdate. PUREMENU now renders directly onto framebuffer.
- Dynarec enabled (patched `dyn_cache.h` with `VirtualAllocFromApp`, see `docs/DYNAREC_UWP.md`)
- Tested on Windows 11 via VS2022. Xbox Series deploy not tested.
- Mouse input implemented: CoreWindow pointer events → SetMouseMove/SetPointer/SetMouseButton/SetMouseWheel → retro_input_state + DBPS_GetMouse. Cursor hidden on first click. Puremenu cursor works via DBPS_GetMouse.
- Keyboard→joypad state leak fixed: JOYPAD reads `s_joypadState[16]` instead of `s_keyboardState[]`.
- register-core-options: SET_VARIABLE/GET_VARIABLE/GET_VARIABLE_UPDATE all implemented: PUREMENU changes now propagate to core and persist. Tested: option displays updated value, core applies via check_variables() on next frame. DBPS_ApplyConfigOverrides (FRONTEND.DBP) still stub.
- xb-xray integrated: submodule + props + start/stop/update + binds (audio_queued, fps, target_fps, frame timing). Debug-only. Connect via `nc <ip> 9000`.
