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
