# Discoveries

## Keyboard→JOYPAD State Leak (Jul 2026)

### Symptom
Pressing certain keyboard keys triggers unintended extra keys in DOS:
- **Enter** → Enter + "4"
- **Tab** → Tab + Space
- Other keys with RETROK values 0-15 likely affected

### Root Cause
`retro_input_state()` for `RETRO_DEVICE_JOYPAD` was reading from `s_keyboardState[]`, causing keyboard state to leak into joypad queries.

RETROK values (0-323) numerically overlap with JOYPAD button IDs (0-15):

| Keyboard Key | RETROK  | JOYPAD ID | Joypad Button | Core Maps To |
|-------------|---------|-----------|---------------|-------------|
| Backspace   | 8       | 8         | A             | KBD_leftalt |
| Tab         | 9       | 9         | X             | KBD_space   |
| Clear       | 12      | 12        | L2            | KBD_3       |
| Enter       | 13      | 13        | R2            | KBD_4       |
| Escape      | 27      | —         | (27 > 15, no overlap) | — |

### Fix
1. Added `s_joypadState[16]` array — separate from `s_keyboardState`
2. `retro_input_state` JOYPAD branch now reads from `s_joypadState` instead
3. Cleared to all-false each frame (no physical gamepad connected yet)
4. Added `SetJoypadButton(id, held)` API for future gamepad wiring

### Files Changed
- `dosbox-uwp/Content/RetroCore.h` — added `s_joypadState[16]`, `SetJoypadButton()`
- `dosbox-uwp/Content/RetroCore.cpp` — fixed JOYPAD branch, added impl
- `dosbox-uwp/dosbox_uwpMain.cpp` — clears joypad state before each RunFrame

### Future: Real Gamepad
When connecting a physical gamepad, wire SdlInput button state to `RetroCore::SetJoypadButton()`. Note that SdlInput button IDs (`BUTTON_A=0`..`BUTTON_R3=11`) don't match libretro JOYPAD IDs directly — a translation table is needed (see TODO in `dosbox_uwpMain.cpp:Update()`).

## SDL Space→BUTTON_A Mapping
SdlInput maps keyboard Space to `BUTTON_A` (line 178-191 of SdlInput.cpp). This means pressing Space triggers both `RETROK_SPACE` (via CoreWindow key event) and `BUTTON_A` (via SDL event). Currently BUTTON_A is not forwarded to JOYPAD state, so this only affects the SdlInput button-held color feedback in the HUD. If gamepad wiring is added later, consider removing or guarding the SDL keyboard→button mapping to avoid double-triggering.

## XAudio2 DRC Removal & Queue Feedback via Accumulator Scaling (Jul 2026)

### Symptom
Permanent audio pitch wow/flutter from `SetFrequencyRatio` DRC. Queue had no feedback mechanism after DRC removed — stabilized at 9449-10079 frames instead of target 4410. Underruns during heavy frames (CDA loading, resolution change): queue 10079→0 in 1.9s gap.

### Root Cause: Catch-up always one frame behind
Heavy processing (CDA load) happens INSIDE `RunFrame()` which runs at START of Update. After RunFrame returns (200ms later), queue is already drained. But the retro_run loop already finished — catch-up waits until NEXT frame's accumulator delta to fire more runs. Always one frame behind.

### Fix 1: Remove SetFrequencyRatio DRC (no pitch artifacts)
`XAudio2Output.cpp`: removed `updateDrc()`, `m_currentRatio`, `SetFrequencyRatio()` calls. Ratio fixed at 1.0.

### Fix 2: Reduce TARGET_QUEUE 7560→4410→3307
Three stages:
- 7560 (171ms) → 4410 (100ms) — original reduction
- 4410 (100ms) → 3307 (75ms) — current, safe with catch-up protection

### Fix 3: Queue-based accumulator scaling (Replaces DRC feedback)
In `dosbox_uwpMain.cpp` after retro_run loop:
```
scale = targetQ / (targetQ + (q - targetQ) * 0.25f)
m_audioTimeAccumulator *= scale
```
Clamped [0.25, 2.0]. When queue > target → scale < 1 → accumulator shrinks → fewer retro_runs next frame. When queue < target → scale > 1 → more retro_runs. No pitch change (no SetFrequencyRatio).

### Fix 4: Emergency catch-up after heavy frame
In `dosbox_uwpMain.cpp` after retro_run loop + accumulator scaling:
- If queue < 500 frames (~11ms), run up to 30 extra `RunFrame()` immediately
- Loop checks `GetQueuedFrames()` each iteration — stops when queue reaches target
- Guards: XB_INSPECTOR_ENABLED s_paused check, shutdown check, 30-frame cap

### Files Changed
- `dosbox-uwp/Content/XAudio2Output.h` — added `static const long TARGET_FRAMES = 3307` (public)
- `dosbox-uwp/Content/XAudio2Output.cpp` — `TARGET_QUEUE` 4410→3307, DRC/SetFrequencyRatio removed
- `dosbox-uwp/dosbox_uwpMain.cpp` — added accumulator scaling + emergency catch-up after retro_run loop

### Future Refinements
- **Present-based catch-up**: run catch-up at END of Render() instead of after retro_run loop, to use vsync wait time for audio production
- **Audio-driven pacing**: `GetState().SamplesPlayed` as primary clock instead of QPC (see section below)
- **Trend-based prediction**: use rolling queue trend (already computed in `XAudio2Output::Submit()`) to pre-adjust accumulator before queue deviates

## XAudio2 Audio Latency — Queue Depth & Frame Pacing (Jul 2026)

### Symptom
Permanent ~240ms audio lag after replacing SDL audio with native XAudio2. HUD showed `XA2frames=10709` stable from submit #500 onward — never drained.

### Root Cause 1: Sleep granularity
`DoPacingSleep()` uses QPC + `Sleep()` to target 70fps (14.3ms/frame). Default Windows timer resolution is ~15.6ms. `Sleep(13)` always sleeps for ~15.6ms, overshooting by 1.3ms/frame. Cumulative error causes `m_lastFrameTime` to persistently lag behind QPC. The sleep is eventually skipped, and the loop runs slightly faster than 70fps (~71-72fps). At 71fps × 630 frames/call = 44730 frames/sec vs 44100 consumption → queue grows ~630 frames/sec = 14ms/sec.

**Fix:** Replaced `Sleep(remainingMs-1)` with `CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)` + `SetWaitableTimer` + `WaitForSingleObject`. This gives ~100μs precision without requiring `timeBeginPeriod(1)` (which is blocked in UWP AppContainer — `winmm.lib` not available). FPS now locks at exactly target (no drift).

### Root Cause 2: No queue-depth cap
Even with accurate timing, residual drift (~0.5fps) from processing-time variation causes the audio queue to slowly accumulate. Without a cap, it grows to 240ms and stays there (production = consumption in steady state, so it never drains).

**Fix:** Added queue-depth cap in `XAudio2Output::Submit()`. When `s_queuedFrames > MAX_QUEUE`, the voice is `Stop()` + `FlushSourceBuffers()` + resubmit with fresh audio + `Start(0)`. This bounds maximum latency to `MAX_QUEUE / 44100 * 1000` ms.

**Current values:**
| MAX_QUEUE | Max Latency | Flush Frequency (with ~0.5fps drift) |
|-----------|-------------|------|
| 4410 | 100ms | ~every 11s |
| 1260 | 29ms | ~every 1.8s |
| 882 | 20ms | ~every 0.7s |

Lower values = tighter latency but more frequent flushes. Flush is a Stop/Flush/Start cycle — inaudible at 882 (user reports no pops/crackling).

### Residual Issue: QPC vs audio clock mismatch
QPC-based `DoPacingSleep()` has inherent drift because processing time varies per frame (~0.03-2.5ms in observed logs). The expected time `m_lastFrameTime` advances by a fixed `framePeriod` each call, but the real QPC advance between exit/entry includes variable processing overhead. This produces short bursts above/below target fps, averaging to target over long windows, but the nonlinear queue dynamics cause the steady-state queue to settle above zero.

### Future Solution: Audio-Driven Pacing (Phase-Locked Loop)
Instead of using QPC as the timing reference, use XAudio2's actual audio consumption position via `IXAudio2SourceVoice::GetState(&state).SamplesPlayed`. The frame pacing should target a constant queue depth (e.g., 630 frames = 14ms) and adjust the sleep time based on whether the voice is ahead or behind:

```
expectedConsumed = frameCount * 44100 / targetFps
actualConsumed = GetState().SamplesPlayed
skew = actualConsumed - expectedConsumed
if skew < 0: producing too fast → wait longer
if skew > 0: producing too slow → reduce wait
```

This couples the emulation clock to the audio clock, eliminating drift entirely. Resets on flush need to track `SamplesPlayed` offset.

### Technical Debt
- **Audio-driven pacing using `GetState().SamplesPlayed`** is the correct long-term solution to eliminate residual drift. Current QPC-based pacing works but settles at ~20ms queue depth with periodic flushes. Implement when latency is critical (music/rhythm games).
- **`CreateWaitableTimerEx` with `HIGH_RESOLUTION` flag** requires Windows 10 1803+. Acceptable for Xbox Series and modern Windows, but legacy Windows support would need fallback.
- **Queue flush in `Submit()`** is a band-aid. The Stop/Flush/Start cycle produces a sub-millisecond audio gap on each flush. Not audible at current thresholds, but accumulated over long sessions could cause perceptible timing shifts in audio-reactive games.

## Audio Rate Mismatch: 60fps Visual × 70fps Core — Crackling Root Cause (Jul 2026)

### Symptom
Constant audio crackling on Xbox. XAudio2 underruns (~66-67 per 100-submit window). Queue stuck at 1-2 buffers (629-1259 frames). Log showed `consumption=~37700Hz` instead of 44100Hz, with net drift ~-240ms.

### Root Cause
DosBox Pure core targets 70fps internally (`av.timing.fps = 70.09`). Each `retro_run()` produces ~630 stereo frames of audio (one frame's worth at 44100Hz). On Xbox, `Present(0,0)` on the swap chain blocks for ~16.7ms (60Hz display rate), limiting the main loop to 60fps.

At 60fps × 630 frames/call = **37,800 Hz** production. XAudio2 voice consumes at **44,100 Hz**. Net deficit of ~6,300 Hz/sec. Queue always drains regardless of pre-buffer depth — headroom only delays inevitable starvation.

`DoPacingSleep()` in `dosbox_uwpMain.cpp:275` was designed to target 70fps via QPC + high-res waitable timer, but it never sleeps because the rest of the loop (ProcessEvents + Update + Render + Present) already consumes 16.7ms — longer than the 14.3ms frame period. The variable `m_lastFrameTime` keeps advancing by 14.3ms per iteration while real QPC advances by 16.7ms, so the check `_now < m_lastFrameTime` is always false. See `dosbox_uwpMain.cpp:296`.

### Fix: Multi-retro_run Per Visual Frame (Option A)

Instead of fighting the 60fvs visual cap, accumulate real QPC time and call `retro_run()` multiple times per visual frame to produce the correct aggregate audio rate.

```cpp
// In Update(), inside m_timer.Tick() lambda:
LARGE_INTEGER _now;
QueryPerformanceCounter(&_now);
double deltaMs = (_now - m_audioLastTick) * 1000.0 / qpcFreq;
m_audioTimeAccumulator += deltaMs;
while (m_audioTimeAccumulator >= audioPeriodMs)  // 14.3ms @ 70fps
{
    m_retroCore->RunFrame();
    m_audioTimeAccumulator -= audioPeriodMs;
}
m_audioLastTick = _now;
```

Average: 1.1667 retro_run calls per visual frame → 7 calls per 6 frames → 70fps aggregate. Audio production = 70 × 630 = 44,100 Hz. Deficit zero.

**Files changed:**
- `dosbox-uwp/dosbox_uwpMain.h` — added `m_audioLastTick` (LARGE_INTEGER), `m_audioTimeAccumulator` (double)
- `dosbox-uwp/dosbox_uwpMain.cpp` — replaced single `m_retroCore->RunFrame()` with the multi-call loop (line ~378)
- Clamp: accumulator capped at `audioPeriodMs × 15` to prevent spiral-of-death after long stalls (file picker, loading)

**Log evidence:**
```
XA2 submit #200: queue=5034 (114ms) consumption=43954Hz underruns=0
XA2 submit #300: queue=5034 (114ms) consumption=44158Hz underruns=0
XA2 submit #400: queue=4404 (100ms) consumption=44261Hz underruns=0
XA2 submit #500: queue=5033 (114ms) consumption=44160Hz underruns=0
...
XA2 submit #3300: queue=5034 (114ms) consumption=43955Hz underruns=0
```

- **Queue stable at ~5034 frames (~114ms)** — never starved
- **consumption = ~44,100 Hz** — matches expected hardware rate
- **underruns = 0** — no crackling throughout session
- `fps=60` in TICK log but queue never drains — confirms fix decouples audio from visual rate

### Future: Option B — RetroArch-Style Resampler + DRC

While Option A works, it has limitations:
1. **Not usable if retro_run is expensive** (complex games). Currently `frame=0.0ms` (near-instant), but a heavier game might not sustain 1.17× retro_run per visual frame.
2. **Cannot decouple audio/video timing** independently. If retro_run ever needs >16.7ms, the loop falls behind and audio starves.
3. **Visual FPS stays at 60** — the audio pacing is hidden but doesn't improve visual smoothness.

RetroArch solves this with a **software resampler + Dynamic Rate Control (DRC)** loop. Full investigation at `F:\workspace\RetroArch\audio\audio_driver.c`.

#### RetroArch Architecture

```
Core calls audio_batch_cb(samples, count)
         │
         ▼
audio_driver_flush()
         │
         ├── write_raw available? ──YES──▶ Driver gets rate_adjust directly
         │                                (CoreAudio only — adjusts hardware clock)
         │
         ├── int16 sinc fastpath? ─YES──▶ src_ratio_curr = src_ratio_orig × adjust
         │                                → resampler_int16_process(ratio)
         │                                → audio->write()
         │
         └── float path ─────────────▶ src_ratio_curr = src_ratio_orig × adjust
                                       → resampler->process(ratio)
                                       → audio->write()
```

#### DRC Algorithm (`audio_driver_compute_rate_adjust()`)

```c
int avail = audio->write_avail(context);         // How much FIFO can accept
int half_size = buffer_size / 2;                  // 50% midpoint target
double direction = (avail - half_size) / half_size;  // -1.0..+1.0
double rate_adjust = 1.0 + effective_delta * direction;  // ~±0.001 of 1.0
```

Proportional controller:
- Buffer > 50% full → `rate_adjust > 1.0` → resampler speeds up → drains buffer faster
- Buffer < 50% full → `rate_adjust < 1.0` → resampler slows down → fills buffer
- `effective_delta` scaled inversely with `src_ratio_orig` to prevent oscillation at high sample rates (96kHz+)

#### XAudio2 Driver in RetroArch (`audio/drivers/xaudio.c`)

- **16 ring buffers** of size `bufsize = latency × rate / 1000` (e.g. 384 frames for 8ms @ 48kHz)
- `xa_write()`: copies audio into current buffer slot; when slot fills, submits via `IXAudio2SourceVoice_SubmitSourceBuffer()`
- **`SetFrequencyRatio()` NEVER called** — driver relies entirely on software resampler to adjust rate
- `OnBufferEnd()` callback uses `InterlockedDecrement` + `SetEvent` to unblock `xa_write()`
- **Key point:** Neither XAudio2 nor WASAPI drivers in RetroArch implement `write_raw`. Both route through the software resampler.

#### WASAPI Driver (`audio/drivers/wasapi.c`)

- Exclusive mode: `buffer_duration = latency × 10000.0`, FIFO + `GetBuffer/ReleaseBuffer` cycle
- Shared mode: `GetCurrentPadding()` to query consumed frames
- `write_raw = NULL` — same resampler-dependent path as XAudio2

#### Why SetFrequencyRatio Alone Won't Work

A naïve approach — set `SetFrequencyRatio(37800/44100 = 0.857)` to match the 37.7kHz production rate — would work numerically but **changes pitch by -14%**. The audio would play in slow-motion with deep voice (chipmunk effect in reverse). This is because `SetFrequencyRatio` changes both **playback speed AND pitch**. For small corrections (±0.1%) the pitch shift is inaudible, but for the 14% mismatch here it's extremely noticeable.

RetroArch's resampler changes **only speed** — pitch is preserved via sample-rate conversion (sinc-windowed, nearest, or CC resamplers). The resampler produces the correct number of output samples regardless of input rate, using interpolation.

#### Implementation Plan for Option B

If Option A becomes insufficient (e.g., a complex DOS game makes retro_run too slow for 1.17× calls per frame):

1. **Integrate libretro's resampler** — `libretro-common/audio/resampler/` has `sinc` and `nearest` drivers. The core's `audio_batch_cb` output goes through the resampler before reaching `XAudio2Output::Submit()`.
2. **Insert Resampler in RetroCore.cpp** — intercept `audio_sample_batch_cb` → resample from core rate (44100) to output rate (44100 or 48000) at a ratio adjusted by DRC.
3. **Implement DRC loop** — every N submits, query XAudio2 buffer occupancy (`GetState().BuffersQueued` or `s_queuedFrames`) and compute `rate_adjust`. Modify resampler ratio by ±0.05% steps.
4. **Cap adjustments** — RetroArch uses `rate_control_delta` (default ~0.005) scaled by buffer deviation. The total adjustment range must be within ±5% to avoid audible artifacts (`audio_max_timing_skew`).
5. **Remove multi-retro_run** — Switch back to single `retro_run()` per visual frame. The resampler handles the rate conversion instead of producing extra frames.

See also: `C:\Users\marcelo\workspace\RetroArch\audio\audio_driver.c` lines 548-575 (DRC compute), `audio\drivers\xaudio.c` (buffer management, 667 lines), `audio\drivers\wasapi.c` (1553 lines). All `write_raw` paths are NULL for XAudio2/WASAPI — resampler path is mandatory.

## Logging: OutputDebugStringA → spdlog Tech Debt (Jul 2026)

### Current State
`LogHelper.h` defines `#define OutputDebugStringA(msg) LogPrint(msg)` when `XB_INSPECTOR_ENABLED`, which routes all `OutputDebugStringA()` calls through `spdlog::info()`. This works but has issues:

1. **Double-tag** — many callsites include `[dosbox-uwp]` prefix in the string, which duplicates spdlog's logger-name tag
2. **Temp buffer** — `sprintf_s(buf, ...) + OutputDebugStringA(buf)` pattern creates unnecessary stack buffers
3. **Inconsistent** — some new code uses `spdlog::info` directly (preferred), others use macro

### Future
Convert all `OutputDebugStringA` callsites to `spdlog::info("fmt", args...)` directly:
- `dosbox-uwp/App.cpp` — ~17 calls
- `dosbox-uwp/dosbox_uwpMain.cpp` — ~22 calls
- `dosbox-uwp/Content/RetroCore.cpp` — ~40 calls
- `dosbox-uwp/Content/SdlInput.cpp` — ~6 calls
- `dosbox-uwp/dosbox_pure_sta.cpp` — ~2 calls

When done, remove macro from `LogHelper.h` and include `<spdlog/spdlog.h>` directly.
New code: always use `spdlog::info()` directly, never `OutputDebugStringA`.

## Frame Pacing: VSync & Settings Menu (Jul 2026)

### Problem: Games running too fast on Windows
Main loop was a **busy-spin** with no frame limiter:
```
App::Run → Update → Render → Present(0, 0) → repeat
                              ↑ syncInterval=0 = no vsync wait
```
Loop ran as fast as CPU allowed. Only throttle was audio queue DRC feedback (reactive, not proactive). Xbox worked because UWP compositor enforces vsync.

### Fix 1: VSync via Present(1, 0)
`App.cpp:145` — Changed `Present(0, 0)` → `Present(m_deviceResources->GetSyncInterval(), 0)`.

- `DeviceResources::SetVSync(bool)` toggles `m_syncInterval` between 0 (no vsync) and 1 (vblank sync)
- Default is `m_syncInterval = 1` (vsync on)
- `Present(1, 0)` blocks until next display vblank → loop naturally runs at display refresh rate (60Hz/120Hz/144Hz)
- Adds ~1 frame of input latency (acceptable for DOS emulator)
- Xbox already gets this from compositor, but explicit is better

### Fix 2: GET_THROTTLE_STATE reports VSYNC
`RetroCore.cpp:415-426` — Changed `RETRO_THROTTLE_NONE` → `RETRO_THROTTLE_VSYNC`. This tells the core that external frame pacing exists, so it doesn't try to self-throttle.

### Settings Menu Architecture
**FrontendMenu** extended with real options:
- `MenuItem` struct gained `optionKey` field — maps toggle to core/frontend option key
- `onOptionChanged(key, value)` callback — dosbox_uwpMain handles VSync/Scaler changes
- Options populate from `SettingsManager::GetOption()` with current values at menu build time

**Video section:**
| Option | Key | Values | Type |
|--------|-----|--------|------|
| VSync | `frontend_vsync` | Off, On | Frontend-only |
| Scaler | `frontend_scaler` | Nearest, Bilinear | Frontend-only |
| Aspect Ratio | `dosbox_pure_aspect_correction` | Off, On, Doublescan, Padded, Padded+Doublescan | Core |
| Graphics Chip | `dosbox_pure_machine` | SVGA, VGA, EGA, CGA, Tandy, Hercules, PCJR | Core |
| SVGA Memory | `dosbox_pure_svgamem` | 0-4, 8 (512KB-8MB) | Core |
| Overscan | `dosbox_pure_overscan` | 0-3 | Core |

**Audio section:**
| Option | Key | Values | Type |
|--------|-----|--------|------|
| Sample Rate | `dosbox_pure_audiorate` | 48000, 44100, 32000, 22050, 16000, 11025 | Core |
| SoundBlaster Type | `dosbox_pure_sblaster_type` | SB16, SBPro2, SBPro1, SB2, SB1, GB, None | Core |
| Volume: SB/MIDI/Adlib/Speaker | `dosbox_pure_volume_*` | 50%, 100%, 150%, 200%, 300%, 500% | Core |

**Scaler** — `RetroScreenRenderer::SetInterpolationMode()` toggles D2D1 between `NEAREST_NEIGHBOR` (pixel-perfect) and `LINEAR` (smooth). Applied immediately on toggle.

### Key Insight: C++17 for static inline
`dosbox_pure_osd.h` forked with `static inline Bit32u` members inside `struct DBP_BufferDrawing` (replacing `enum EColors`). This requires `<LanguageStandard>stdcpp17</LanguageStandard>` in vcxproj. Without it, MSVC errors `C7525: inline variables require at least '/std:c++17'`.

### Future Improvements
- **Audio-driven pacing** using `GetState().SamplesPlayed` — eliminates QPC drift entirely
- **Slider items** for volume/sensitivity — currently discrete values only
- **Per-game settings** — `DBPS_ApplyConfigOverrides` (FRONTEND.DBP JSON override) still stub
- **Settings persistence on shutdown** — currently auto-saves on SetOption, but could batch save

## DBP_STANDALONE_AUDIO Pitfall (Jul 2026)

### Symptom
Xargon worked perfectly, but heavier games (Rally Championship) had audio crackling/artifacts. Logs showed `have=31` (only 31 samples available when 630 needed).

### Root Cause
Added `#define DBP_STANDALONE_AUDIO 1` to the local copy of `dosbox_pure_libretro.cpp`, plus changed 4x `#ifndef DBP_STANDALONE` guards to `#if !defined(DBP_STANDALONE) || defined(DBP_STANDALONE_AUDIO)`.

This **activated the core's internal audio pipeline** even with `DBP_STANDALONE` defined:
1. Core's `MIXER_CallBack()` consumed mixer samples during `retro_run()`
2. Sent them to `audio_batch_cb()` → our `retro_audio()` (no-op) → data discarded
3. After `retro_run()`, our `PullAndQueue()` → `DBPS_AudioMix()` found mixer empty

**Two consumers competing for the same mixer buffer.** Core consumes and discards; our pull gets nothing.

### Fix
Reverted all audio-related changes to the local copy. With `#ifndef DBP_STANDALONE` (false since DBP_STANDALONE is defined), the core's audio pipeline is skipped. `PullAndQueue()` → `DBPS_AudioMix()` is the sole consumer.

### Lesson
**Never patch the dosbox-pure core's audio pipeline.** Our role is the frontend shell (like ZillaLib/libretro/RetroArch UWP). The core works perfectly; we consume audio via `DBPS_AudioMix()` from the mixer, bypassing `audio_batch_cb` entirely.

## Audio Architecture Investigation — v0.8.2.0 vs HEAD (Jul 2026)

### Context
v0.8.2.0 (tag `a52f8dc`) was the best working audio state. HEAD switched to SDL Audio
(WASAPI) + D3D11 renderer. Audio crackling persists. Investigation to decide: restore
v0.8.2.0 or fix SDL path.

### Three Audio States Compared

#### `stutter-fix-v1` (tag `738d7a0`) — "clean architecture"
- **Push model**: `retro_audio()` → `XAudio2Output::Submit()` — core pushes audio directly
- **Single RunFrame()** per loop tick, no accumulator, no pacing
- **VSync OFF**, DoPacingSleep removed
- **Queue wanders** 5000–15000 frames (no feedback loop)
- **Windows**: worked
- **Xbox + light games**: **FAILED** — audio stopped working
- **Why it failed on Xbox**: core needs `retro_run()` called frequently enough to maintain
  `DBP_ThreadControl` semaphore handshake. At 60fps (Xbox compositor), single RunFrame()
  per tick was insufficient for core's internal 70fps target. Queue drifted to underrun/flush.

#### v0.8.2.0 (tag `a52f8dc`) — best working state
- **Same push model** as stutter-fix-v1
- **PLUS**: QPC accumulator + multi-retro_run (~70fps aggregate)
- **Queue feedback scaling**: `scale = targetQ / (targetQ + (q - targetQ) * 0.25)`
- **VSync ON** (default), TARGET_QUEUE=6615 (~150ms), MAX_QUEUE=22050
- **Windows/Xbox, light games** (Xargon, Tyrian): clean audio, queue ~5034, underruns=0
- **Heavy games** (Screamer): **~50-100ms audio stutter every ~5 seconds**
  - Cause: QPC-vs-DAC drift → queue grows → hits MAX_QUEUE → Stop/Flush/Start cycle
  - The flush is audible (50-100ms gap), then recovers

#### HEAD (current) — SDL Audio + D3D11
- **Pull model**: `retro_audio()` is **no-op**, audio pulled via `DBPS_AudioMix()` every ~7ms
- **Frame accumulator capped at 1.0** (max 1 retro_run per tick)
- **SDL QueueAudio** via WASAPI (SdlAudio.cpp)
- **XAudio2Output exists but disconnected** from main loop
- **D3D11 renderer** + FPS overlay
- **Some games** near-perfect, but **crackling persists**
- **Why crackling**: accumulator cap prevents 70fps aggregate. Pull time-based doesn't
  react to queue state. `DBPS_AudioMix()` reads from mixer that may have stale/insufficient data.

### Key Discovery: "Core Self-Regulates" Is Incomplete

The `AUDIO-PIPELINE.md` states: "the core self-regulates its sample rate... Stop trying
to force 70fps." This is **true but incomplete**:

- The core self-regulates **IF** `retro_run()` is called frequently enough
- At 60fps visual loop, single RunFrame() per tick → core's 70fps target desynchronizes
- The accumulator in v0.8.2.0 solved this by forcing ~70 retro_runs/sec regardless of visual fps
- **Without the accumulator** (stutter-fix-v1, HEAD): core falls behind on audio production

### Decision: Restore v0.8.2.0 Audio + Keep D3D11

**Restore**: XAudio2 push model + accumulator + queue feedback scaling
**Keep**: D3D11 renderer, FPS overlay, spdlog, loading screen, xb-xray
**Fix**: MAX_QUEUE flush stutter (the50-100ms gap every ~5s on heavy games)
**Remove**: SDL Audio (SdlAudio.cpp/h) — unused after restore

### Files Changed (v0.8.2.0 vs HEAD)

| File | v0.8.2.0 | HEAD | Action |
|------|----------|------|--------|
| `RetroCore.cpp:retro_audio` | `Submit(data, frames)` | no-op | **Restore push** |
| `dosbox_uwpMain.h` | `m_audioLastTick`, `m_audioTimeAccumulator` | `m_frameAccum`, `m_lastAccumTime` | **Restore accumulator** |
| `dosbox_uwpMain.cpp` Update() | Multi-retro_run + queue scaling | Single run, cap 1.0, SDL pull | **Restore accumulator loop** |
| `dosbox_uwpMain.cpp` constructor | `m_xaudio2` init + `SetAudioOutput` | `m_sdlAudio` init | **Restore XA2 init** |
| `dosbox_uwpMain.cpp` ProcessPendingLoad | XA2 force-start + acc boost | Removed | **Restore** |
| `XAudio2Output.cpp` | TARGET_QUEUE=6615, MAX_QUEUE=22050 | Same + drain event + WaitForDrain | Keep v0.8.2.0 version |
| `XAudio2Output.h` | TARGET_FRAMES=6615 | Same + watermarks | Keep v0.8.2.0 version |

### Remaining Problem to Solve After Restore

The MAX_QUEUE flush causes ~50-100ms audio gaps on heavy games. After restoring v0.8.2.0,
address this by:
1. Increasing MAX_QUEUE or removing routine flush (keep only as extreme safety net)
2. Adding emergency catch-up: if queue < 500 frames, run extra RunFrame() immediately
3. The queue feedback scaling should prevent drift from reaching MAX_QUEUE in most cases

## v0.8.3.0 Audio Fix — Complete Resolution (Jul 2026)

### Summary
Restored v0.8.2.0 audio architecture (XAudio2 push + accumulator + queue feedback) on top of HEAD (D3D11, spdlog, FPS overlay). Fixed 7 bugs including the critical `DBP_STANDALONE` compile-out and XAudio2 44100→48000 sample rate mismatch.

### Key Fixes

1. **`DBP_STANDALONE` removed from vcxproj** — compiled out `audio_batch_cb()` calls in core. Without it, `retro_audio()` never received data.
2. **Accumulator: `dt * targetFps`** — was missing multiplier, grew at 1.0/sec instead of 70/sec.
3. **Stall debt cap: `3.0 * targetFps`** — correct units (frames, not seconds).
4. **XAudio2 sample rate: 44100 → 48000** — core outputs 48kHz (`DBP_MIXER_GetFrequency()`). Music was 8% too slow.
5. **FPS computation** — `m_fpsLastFrame` now only updated when `m_lastRetroRuns > 0`.
6. **DIAG `runs_ival`** — accumulated retro_runs per interval (useful metric, `runs_last` is always 0).
7. **DBPS stubs** — `ToggleOSD`, `IsShowingOSD`, `IsGameRunning` added to `dosbox_pure_sta.cpp`.

### Files Changed
- `dosbox-uwp/dosbox-uwp.vcxproj` — removed `DBP_STANDALONE`
- `dosbox-uwp/Content/XAudio2Output.cpp` — 44100→48000, TARGET_QUEUE/MAX_QUEUE updated
- `dosbox-uwp/dosbox_pure_sta.cpp` — DBPS stubs
- `dosbox-uwp/dosbox_uwpMain.cpp` — accumulator, FPS, DIAG fixes
- `dosbox-uwp/Content/SdlAudio.cpp/h` — deleted (unused)
- `dosbox-uwp/Package.appxmanifest` — version 0.8.3.0

### Detailed Log
See `docs/audio-fix/V0.8.3.0-FIX-LOG.md`.

### Remaining Issue
QPC-vs-DAC drift still causes queue to grow to MAX_QUEUE (~24000 frames / 500ms) and flush. For Rally Championship (fast-paced), the pre-buffer covers the gap. For music-heavy games, may cause brief stutter. Full fix requires DRC (see `AUDIO-PIPELINE.md`).

## UWP File Access — Eliminating Temp Copy (Jul 2026)

### Problem
When loading a game, the file is copied to `LocalFolder\temp\` before passing to the core:
- **FileBrowser path:** `CopyFileFromAppW` — kernel-level copy to temp. Unnecessary for local files.
- **FileOpenPicker path (Ctrl+L):** `ReadBufferAsync` → `WriteBufferAsync` — loads entire file into managed memory, writes to temp. Catastrophic for large files (4GB ISO = 4GB RAM + 4GB disk).

### Why Other Emulators Don't Copy
Xenia Canary UWP, XBSX2, Dolphin UWP all use the same pattern:
1. `FileOpenPicker` → `StorageFile`
2. Get native path via `StorageFile::Path`
3. Open directly via `CreateFile2FromAppW` (the `*FromAppW` API family)
4. No copy needed — `broadFileSystemAccess` capability grants access to picker-selected paths

### How It Works in UWP
- `StorageFile::Path` returns native NTFS path (e.g., `E:\Games\doom.dosz`) for local filesystem providers
- `CreateFile2FromAppW` checks if the path is within an authorized scope (ApplicationData, install dir, or picker-granted access)
- If authorized, opens handle directly — no streaming, no copy
- The VFS layer (`vfs_implementation_uwp.cpp`) already uses `CreateFile2FromAppW`

### Our Capability
`Package.appxmanifest` already declares `broadFileSystemAccess`. The FileBrowser already uses `FindFirstFileExFromAppW` to browse directories — proving native path access works.

### Confirmed Finding: broadFileSystemAccess ≠ read access to arbitrary paths
**Test result (Jul 2026):** Passing native path `E:\PC\DOSBoxPure\Screamer.dosz` directly
to `QueueLoadRom` (bypassing `CopyFileToTemp`) caused `CreateFile2FromAppW` to fail
with "Unable to open ZIP file". The core still reported `retro_load_game SUCCESS` but
PUREMENU showed empty file list (no game content extracted).

**Root cause:** `broadFileSystemAccess` grants `FindFirstFileExFromAppW` directory
enumeration, but NOT `CreateFile2FromAppW` read access to arbitrary paths. The
capability only grants read access to:
1. Paths selected through `FileOpenPicker` (stored via `FutureAccessList`)
2. Paths explicitly added to `FutureAccessList`
3. The app's own data folders

The FileBrowser uses `FindFirstFileExFromAppW` (works for listing), but the VFS uses
`CreateFile2FromAppW` (fails for reading).

**Conclusion:** `CopyFileToTemp` is REQUIRED for FileBrowser and History paths. The temp
copy cannot be eliminated without a fundamentally different approach (e.g., `FutureAccessList`
for every file, which requires picker interaction).

**FileOpenPicker (Ctrl+L):** Direct `StorageFile::Path` → `QueueLoadRom` MAY work since
the picker auto-grants access. Tested: fallback to buffer copy if `Path` is empty.
