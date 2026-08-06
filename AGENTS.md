# AGENTS.md — DOSBox Pure Unleashed UWP

## Project
Integrate libretro dosbox-pure core into UWP scaffold. DOS emulator on Windows/Xbox.

## Language
All documentation, code, comments, and commit messages MUST be in English.
(User may converse in Portuguese or English; agent responds accordingly.)

## Critical Rules
- **NEVER commit or push** without explicit user request. Stage changes only. Wait for "commit", "push", "faz o commit", etc. This is ABSOLUTE — no exceptions, no "helpful" auto-commits, no "cleaning up". User must ALWAYS explicitly ask.
- **ALWAYS confirm before commit+push**, even if user already asked. Each request is one-time only. Show files to be committed and ask "Confirmo commit+push?" before proceeding.
- **NEVER commit to submodule** `extern/dosbox-pure/`. Patches go in `dosbox-uwp/local/dosbox-pure/` mirroring same directory structure.
- **CAN commit to `extern/uwp-xray-depot/`** — same author/owner as parent repo. Push separately when asked.
- **Build via .sln** not .vcxproj. `$(SolutionDir)` needed for `uwp-dep.props` SDL paths.
- **x64 only.** ARM64/ARM/x86 NOT supported (Xbox Series is x64).
- **`CompileAsWinRT=false`** for legacy C (dosbox-pure source). `/ZW` for C++/CX files.
- **Dynarec enabled** via patched `dyn_cache.h` (RWX cascade: VirtualAlloc → VirtualAllocFromApp → CreateFileMappingFromApp+MapViewOfFileFromApp, all `PAGE_EXECUTE_READWRITE`; codeGeneration capability in manifest; see `docs/DYNAREC_UWP.md`).
- **InputTest workflow:** after editing `docs/tools/inputtest-better/inputtest.c` (or `hello.c`), regenerate `.exe` + `.dosz` in BOTH `docs/tools/inputtest-better/dist/` AND `E:\PC\DOSBoxPure\`. Open Watcom at `C:\Apps\OW`. Run `pwsh -NoProfile build.ps1` in that directory. Script does: wcc (compile) → wlink (link) → Compress-Archive (.dosz) → copy to `E:\PC\DOSBoxPure\`.

## Build
**MUST use VS2022 (v17) MSBuild, NOT VS18.** Machine has VS18 Community installed too; `vswhere -latest` resolves to VS18. Building with VS18's compiler corrupts the IPDB (compiler mismatch) → VS2022 forces full recompile of everything every time ("Previous IPDB was built with incompatible compiler"). Use the full path:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "dosbox-pure-unleashed-uwp.sln" /p:Configuration=Release /p:Platform=x64 /nowarn:MSB4011
```
Current: 0 errors, ~1500 warnings C4244 (cosmetic).

Pitfalls:
- **LNK1201 PDB lock** happens if `dosbox-uwp.exe` (app under test) is still running. Kill it first.
- Building with VS2022 open is fine (verified: CLI build succeeded with devenv running). If a rare C1083/LNK1201 file lock does occur, close VS2022 and retry.
- First build after switching compilers does a full recompile (IPDB/IOBJ mismatch) — one-time, subsequent builds incremental.

## Key Files

| File | Purpose |
|------|---------|
| `dosbox-uwp/Content/RetroCore.cpp/.h` | libretro bridge: init, load, run, callbacks, retro_env, VFS |
| `dosbox-uwp/Content/RetroScreenRenderer.cpp/.h` | D2D bitmap render + letterbox |
| `dosbox-uwp/Content/XAudio2Output.cpp/.h` | XAudio2 audio output: 32-slot buffer pool (zero heap alloc), OnBufferEnd, pre-buffer, 48kHz |
| `dosbox-uwp/dosbox_uwpMain.cpp/.h` | Main loop, Update/Render, input routing, audio init |
| `dosbox-uwp/App.cpp` | Entry point, Ctrl+Alt+F2 → FileOpenPicker → async file read → LoadRom |
| `dosbox-uwp/dosbox_pure_sta.cpp` | DBPS_* stubs (real: DBPS_GetMouse, DBPS_ApplyConfigOverrides; ~9 no-ops) |
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

### 8. DBP_STANDALONE is defined — per-file, only for the core TU
`DBP_STANDALONE` **IS defined**, but **per-file**: only `local\dosbox-pure\dosbox_pure_libretro.cpp`
gets it (vcxproj `<ClCompile>` `PreprocessorDefinitions` override on that file, Debug+Release x64).
All other TUs — other `$(DBPDir)src\*.cpp`, `keyb2joypad.cpp`, `dosbox_pure_sta.cpp`, frontend —
compile **without** it.

Why per-file: the `#ifdef DBP_STANDALONE` tail of `dosbox_pure_osd.h` (included by that TU)
provides `dbp_osdbuf[3]`, `DBPS_ToggleOSD`/`DBPS_IsShowingOSD`/`DBPS_IsGameRunning` and the OSD
menu state — so those symbols come from the core and must NOT be stubbed (link collision).

But the local core is patched so libretro behavior survives DBP_STANDALONE:
- `dbp_audio` + `audio_batch_cb` stay active (L83); empty-sample flush, mix, submit kept (L3632, L3678, L3703)
- audiorate hardcoded `"48000"` (matches XAudio2; the option value only exists in the non-standalone option enum)
- Software render path always (L3128)
- OSD: `DBP_RenderOSD(buf)` in `GFX_EndUpdate` composites `dbp_osdbuf[0]` at fixed **640x480** onto
  `buf.video` → `video_cb`. `DBPS_SubmitOSDFrame()` in `dosbox_pure_sta.cpp` is a no-op stub; the
  composite in `GFX_EndUpdate` is the effective path.

Do NOT extend `DBP_STANDALONE` to other TUs — that would compile out `audio_batch_cb` (breaks
audio) and change OSD behavior in the `$(DBPDir)src\*` files (only the core TU is patched).

### 9. Frame pacing — RetroArch-style QPC pacer + follower ring (replaces audio-master)
The blocking audio `Write()` used to be the frame clock and caused a **pacing bang-bang** oscillation on heavy games (Screamer: RETRO 45↔78, `audio_q=3840` = ring full). Ring `8×10ms=80ms`: full → Write blocked at real-time (~70fps); drained → core free-ran >78fps → refilled → oscillated. Auto-cycle was only a symptom (fixed cycles 77000/200000 still oscillated). See `docs/discoveries.md` "Frame Pacing Bang-Bang".

**Fix (build 155):** emulation thread is now paced by `RetroCore::PaceFrame()` — a QPC accumulator at the same self-consistent rate reported by `GET_THROTTLE_STATE`: vsync ON → display refresh (only when `refresh < core_fps`), else core fps; vsync OFF → core fps. Waits with `CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` (~100µs). `timeBeginPeriod()` is **NOT available in UWP** (winmm.lib restricted) — do not use it.

`XAudio2Output` ring = **4×12ms = 48ms** sub-buffers (RetroArch xaudio.c follower model): `MAX_BUFFERS=4`, `LATENCY_MS=12`, `SUBBUF_FRAMES=576`. (Earlier 16×64ms ≈ 1.024s was a misreading of xaudio.c — it queued ~960ms steady-state; see `docs/discoveries.md` "XAudio2 Ring".) 48ms is the Xbox stability floor: 4×8ms=24ms cracked (under=7-8/s), 4×12ms clean (under=0/s). The ring is a pure **follower**; blocking `Write()` is only a safety valve for rare overrun (waits ≤256ms on `OnBufferEnd`, then drops remainder). `OnBufferEnded` = `InterlockedDecrement(m_buffers)` + `InterlockedIncrement64(m_totalConsumed)` + `SetEvent`. `Flush()` = FlushSourceBuffers + wait for callbacks.

Loop: one `retro_run` per tick, `PaceFrame()` after each frame. Self-consistency: pacer rate == `GET_THROTTLE_STATE` rate → core generates `48000/rate` samples/frame → device consumes 48000/s → ring balanced, no bang-bang. CPU-bound games: pacer falls behind → runs at DOSBox speed → ring drains → audio underrun (same as RetroArch).

**Gotcha:** `s_displayRefreshRate` comes from DXGI swap chain (`DeviceResources::GetDisplayRefreshRate()`, set in `dosbox_uwpMain.cpp`). `PaceFrame()` and `GET_THROTTLE_STATE` MUST use identical rate logic or the ring drifts. `frameLimitFps>0` forces vsync off.

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
`__LIBRETRO__`, `XB_INSPECTOR_ENABLED` (Debug only), `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`

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
- `F:\workspace\RetroArch` — RetroArch source. Frame pacing reference: single-threaded runloop paced by Present(vsync)/frame-limit; GET_THROTTLE_STATE rate; audio ring (16×64ms) is a follower, `xa_write` blocks only when the queue is full. runloop.c (3401-3418 vsync rate, 8037-8045 audio_sync), `audio/drivers/xaudio.c`.

## Keyboard & Gamepad Bindings

## Status
- Phase 0-3 done (scaffold, core compiles, libretro frontend, video pipeline)
- Phase 4 complete (keyboard callback, GET_LOG_INTERFACE)
- Phase 5 complete (XAudio2 output replaces SDL audio)
- QPC pacer pacing (build 155, see bug #9): `RetroCore::PaceFrame()` QPC timer on emulation thread at GET_THROTTLE_STATE rate (vsync → refresh if < core fps, else core fps), high-res waitable timer (timeBeginPeriod NOT in UWP). Ring 4×12ms follower. 1 retro_run/tick. QPC accumulator + multi-run + queue-depth-cap hacks removed.
- OSD fix: `DBP_STANDALONE` defined per-file (core TU only); local core patched to composite the OSD via `DBP_RenderOSD` (640x480) onto `buf.video` → PUREMENU renders on the framebuffer. `DBPS_ToggleOSD`/`IsShowingOSD`/`IsGameRunning` come from the core tail (not stubbed).
- Dynarec enabled (patched `dyn_cache.h` with `VirtualAllocFromApp`, see `docs/DYNAREC_UWP.md`)
- Tested on Windows 11 via VS2022. Xbox Series deploy not tested.
- Mouse input implemented: CoreWindow pointer events → SetMouseMove/SetPointer/SetMouseButton/SetMouseWheel → retro_input_state + DBPS_GetMouse. Cursor hidden on first click. Puremenu cursor works via DBPS_GetMouse.
- Keyboard→joypad state leak fixed: JOYPAD reads `s_joypadState[16]` instead of `s_keyboardState[]`.
- register-core-options: SET_VARIABLE/GET_VARIABLE/GET_VARIABLE_UPDATE all implemented: PUREMENU changes now propagate to core and persist. Tested: option displays updated value, core applies via check_variables() on next frame. DBPS_ApplyConfigOverrides (FRONTEND.DBP) still stub.
- xb-xray integrated: submodule + props + start/stop/update + binds (audio_queued, fps, target_fps, frame timing). Debug-only. Connect via `nc <ip> 9000`.
- Audio (current): XAudio2 48kHz, sample rate hardcoded `"48000"` under `DBP_STANDALONE` (matches device). Ring model: 4×12ms sub-buffers (blocking Write safety valve ≤256ms, OnBufferEnd dec+SetEvent, Flush waits callbacks). No heap alloc on hot path.
