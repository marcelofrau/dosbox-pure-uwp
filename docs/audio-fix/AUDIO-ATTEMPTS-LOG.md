# Audio Fix Attempts Log — Retrospective

> **Purpose:** Document every approach tried for the XAudio2 audio pipeline, what
> worked, what didn't, and where we ended up (back at the start). Written after
> reverting all changes to the `stutter-fix-v1` baseline.
>
> **Date:** Jul 2026
> **Baseline:** tag `stutter-fix-v1` (`738d7a0`) — the clean foundation.

---

## Timeline

### Phase 1: XAudio2 Takeover (`xaudio2-takeover`, `cf7974e`)

**Goal:** Replace SDL audio with native XAudio2 for lower latency and direct control.

**What was done:**
- Replaced SDL audio output with XAudio2 source/mastering voice
- Added `OnBufferEnd` callback for queue tracking
- Alloc-per-submit: `new int16_t[]` on every `Submit()`, `delete[]` in callback
- Queue-depth cap with `Stop/Flush/Start` to bound max latency
- `CreateWaitableTimerEx(HIGH_RESOLUTION)` for frame pacing

**Result:** Audio plays. Queue wanders. Flush band-aid produces pops.

**Learned:** XAudio2 works. But two independent clocks (QPC vs DAC) cause drift.

---

### Phase 2: Pace Flow Fix (`pace-flow-fix`, `13b498e`)

**Goal:** Fix input latency by moving sleep before ProcessEvents.

**What was done:**
- Moved `DoPacingSleep()` before `ProcessEvents` for input responsiveness
- Removed VSync to avoid double-wait

**Result:** Input latency improved. Audio drift unchanged.

**Learned:** Wall-clock pacing doesn't address the root cause.

---

### Phase 3: Audio Fix + Tight Spin-Wait (`audio-fix`, `18a6e8b`)

**Goal:** Hit exactly 70 FPS with busy spin to match core's target rate.

**What was done:**
- Kept SDL paused until ~93ms queued (16384 bytes)
- Tight busy spin-wait on QPC to hit 70.09 FPS before each `RunFrame()`
- No `SwitchToThread` — full CPU pinning

**Result:** CPU pinned at 100%. Bad for Xbox thermals. Still QPC-based → drift masked by pre-buffer, not fixed.

**Learned:** Busy spin is wrong tool. Coupling video loop to audio rate is wrong.

---

### Phase 4: Stutter Fix v1 (`stutter-fix-v1`, `738d7a0`) ⭐

**Goal:** Clean architecture — core self-regulates, XAudio2 is pure sink.

**What was done:**
- Removed `WaitForDrain` audio pacing
- Core self-regulates via its own thread (DOSBox PIC timer + `time_cb()`)
- `Submit()` is non-blocking pure sink
- No external pacing at all
- Queue settles 5000–15000 frames

**Result:** Cleanest architecture. Queue wanders but no pops/crackles in short sessions.

**Problem:** No feedback loop. Queue eventually (a) hits MAX_QUEUE → flush → pop, or (b) heavy frame drains it → crackle. Also visual loop free-runs burning CPU.

**Verdict:** Best baseline. Needs exactly one addition: DRC.

---

### Phase 5: Queue-Depth Cap (`current main` before revert)

**Goal:** Bound max latency with flush band-aid.

**What was done:**
- `MAX_QUEUE` = 22050 (500ms) — log-only safety net
- Queue accumulates slowly due to QPC-vs-DAC drift

**Result:** Same as stutter-fix-v1 but with larger buffer to hide drift longer. Latency ~150-250ms. Still no feedback loop.

---

### Phase 6: Accumulator Scaling + Emergency Catch-up

**Goal:** Queue-based feedback without DRC controller.

**What was done:**
- After retro_run loop: `scale = targetQ / (targetQ + (q - targetQ) * 0.25)`
- `m_audioTimeAccumulator *= scale` — nudges how many retro_runs next frame
- Emergency catch-up: if queue < 500, run up to 30 extra `RunFrame()` immediately

**Result:** Partial. Reactive, always one frame behind. Emergency catch-up helps with spikes but not steady drift.

**Learned:** Catch-up always fires AFTER the damage (heavy frame already drained queue). Need proactive control, not reactive.

---

### Phase 7: Multi-retro_run Per Visual Frame (Accumulator)

**Goal:** Force 70fps aggregate retro_run rate despite 60fps visual loop.

**What was done:**
- QPC accumulator: accumulate real time, call `retro_run()` floor(deltaMs / audioPeriodMs) times
- At 60fps visual: 1.1667 retro_run calls per visual frame → 70fps aggregate
- Audio production = 70 × 630 = 44,100 Hz ≈ consumption

**Result:** Queue stable at ~5034 frames (~114ms). consumption=44100Hz. underruns=0.

**Log evidence:**
```
XA2 submit #200: queue=5034 (114ms) consumption=43954Hz underruns=0
XA2 submit #300: queue=5034 (114ms) consumption=44158Hz underruns=0
XA2 submit #400: queue=4404 (100ms) consumption=44261Hz underruns=0
```

**Problems discovered:**
1. Contradicts AUDIO-PIPELINE.md §2: "Stop trying to make the visual loop run at exactly 70 FPS"
2. If `retro_run()` ever becomes expensive (>16.7ms), loop falls behind → audio starves
3. Visual FPS stays at 60 — doesn't improve visual smoothness
4. We kept adding more logic on top, going in circles

---

### Phase 8: Ring Buffers + DRC Controller

**Goal:** Pre-allocate XAudio2 buffers (no heap on hot path) + proportional DRC.

**What was done:**
- 16 pre-allocated ring buffer slots (no `new`/`delete` on submit)
- `OnBufferEnd` uses return value from `InterlockedExchangeAdd` (race fix)
- DRC controller: proportional, exponential smoothing (alpha=0.3)
- Measurement via `SamplesPlayed` from `GetState()`
- DRC range: ±5% (gain 0.05)
- Update interval: every 10 submits (~300ms)

**Result:** Architecture correct. But `SamplesPlayed` measurement unreliable on UWP — shows ~34-39kHz instead of 44.1kHz.

**Problem:** Alternating consumption values (34k vs 39k) suggest measurement artifact, not actual clock drift. Likely caused by frequent underruns (SamplesPlayed stalls during silence).

---

### Phase 9: AudioResampler + DRC via retro_audio

**Goal:** RetroArch-style resampler between core output and XAudio2 Submit.

**What was done:**
- `AudioResampler.h`: stereo 16-bit linear interpolation resampler
- Ratio from DRC controller applied in `retro_audio()` before `Submit()`
- Resampler handles rate conversion without pitch change

**Result:** Added complexity. DRC ratio based on unreliable `SamplesPlayed` measurement. Queue still grows.

**Learned:** Resampler is correct tool, but without reliable clock measurement, DRC can't steer properly.

---

### Phase 10: Accumulator Frame Limiter (Final Attempt)

**Goal:** Replace threshold-based frame limiter with accumulator to hit exact 70fps.

**What was done:**
- `m_frameTimeAccum += elapsedMs; if (accum >= targetMs) { RunFrame(); accum -= targetMs; }`
- Alternates between 2 and 3 main-loop ticks to hit 70fps from 180fps base

**Result:** Would work for frame pacing. But we'd already proven this contradicts the docs ("don't force 70fps"). Stopped before testing.

---

## Where We Ended Up (after XAudio2 phases)

**Back at `stutter-fix-v1`.** Reverted all audio changes. Build compiles clean.

The tag represents the cleanest architecture:
- Core self-regulates on its own thread
- `Submit()` is non-blocking pure sink
- No external pacing, no coupling, no band-aids

**What's missing:** A slow, closed-loop DRC controller that:
1. Measures real DAC consumption (not QPC)
2. Steers queue toward constant target depth
3. Makes tiny corrections (≤±0.5%, inaudible)

**Why we couldn't implement it:** `SamplesPlayed` measurement is unreliable on UWP. Without a reliable clock, DRC steers blind.

---

## Phase 11: SDL Audio (WASAPI) + D3D11 Renderer

**Goal:** Replace XAudio2 with SDL2 audio (WASAPI) for simpler integration. Add D3D11 renderer.

**What was done:**
- `SdlAudio.cpp/h`: new SDL2 audio output using `SDL_QueueAudio` (queue mode)
- `retro_audio()` changed to **no-op** — audio pulled via `DBPS_AudioMix()` every ~7ms
- Frame accumulator added but **capped at 1.0** (max 1 retro_run per tick)
- `RetroD3D11Renderer.cpp/h`: new D3D11 textured-quad renderer replacing D2D bitmap
- FPS overlay added (rolling 60-frame window)
- VSync OFF by default
- XAudio2Output code **retained but disconnected** from main loop

**Result:** Partial. Some games (Xargon, Tyrian) near-perfect. Rally Championship: crackling persists.

**Root cause of persistent crackling:**
1. Accumulator cap at 1.0 prevents 70fps aggregate → core produces fewer samples than XAudio2/WASAPI consumes
2. `DBPS_AudioMix()` pull is time-based (every ~7ms), not queue-driven → pulls even when mixer has stale data
3. No feedback loop between SDL queue depth and production rate

**Problems discovered:**
1. `DBP_STANDALONE_AUDIO` enabled → core consumed its own mixer → `DBPS_AudioMix` starved (see discoveries.md)
2. Reverted to `#ifndef DBP_STANDALONE` (core audio pipeline skipped)
3. Heavy games not tested (Screamer, etc.)

**Verdict:** SDL path has cleaner architecture (pull model, WASAPI built-in) but the crackling problem is harder to solve without DRC. The accumulator cap is the primary blocker.

---

## Phase 12: Restoration Decision

**Decision:** Restore v0.8.2.0 audio architecture (XAudio2 push + accumulator + queue feedback).

**Rationale:**
- v0.8.2.0 was the best working state (Xargon, Tyrian clean; Screamer had minor ~50-100ms stutters)
- `stutter-fix-v1` failed on Xbox (core needs frequent retro_run calls)
- SDL pull model crackling is harder to fix than v0.8.2.0's flush stutter
- D3D11 renderer + FPS overlay + spdlog retained from HEAD

**What to restore:**
- `retro_audio()` → `Submit()` push model
- QPC accumulator + multi-retro_run (target 70fps aggregate)
- Queue feedback scaling (`scale = targetQ / (targetQ + (q-targetQ)*0.25)`)
- XAudio2 initialization in constructor
- Force-start XAudio2 after load + accumulator boost

**What to keep from HEAD:**
- `RetroD3D11Renderer` (D3D11 render path)
- FPS overlay
- spdlog everywhere
- Loading screen improvements
- xb-xray instrumentation

**What to remove:**
- `SdlAudio.cpp/h` (unused after restore)
- Frame accumulator cap at 1.0
- SDL pull logic in Update()

**Remaining problem:** MAX_QUEUE flush causes ~50-100ms audio gaps on heavy games. To fix:
1. Increase MAX_QUEUE or remove routine flush (safety net only)
2. Add emergency catch-up: if queue < 500 frames, run extra RunFrame()
3. Queue feedback scaling should prevent drift from reaching MAX_QUEUE

---

## Key Discoveries

### 1. Core self-regulates sample rate
DOSBox Pure does NOT produce a fixed number of samples per `retro_run()`. The emulator runs on its own thread, paced to real time. `retro_run()` emits however many samples were produced since last call. At 60fps vs 70fps, each call returns more samples (~735 vs ~630). Aggregate output stays ≈44100Hz either way.

**Implication:** Stop trying to force 70fps. The core already produces the right average rate.

### 2. Two independent clocks cause drift
QPC (production clock) and DAC crystal (consumption clock) are physically different crystals. A few Hz mismatch accumulates → queue slowly grows or drains.

**Implication:** Cannot fix drift with bigger buffer or better wall-clock timer. Must close the loop on real consumption clock.

### 3. `SamplesPlayed` unreliable on UWP
`GetState().SamplesPlayed` shows ~34-39kHz instead of expected 44.1kHz. Alternating pattern suggests measurement artifact. Likely affected by underruns (SamplesPlayed doesn't advance during silence).

**Implication:** DRC based on SamplesPlayed doesn't work without fixing underruns first — but underruns are what DRC is supposed to fix. Chicken-and-egg.

### 4. Multi-retro_run works but contradicts architecture
Forcing 70fps aggregate rate via accumulator produces stable queue and 0 underruns. But it contradicts "core self-regulates" principle and creates coupling.

### 5. Ring buffers correct, DRC controller correct, measurement broken
The architecture (pre-allocated ring + proportional DRC) is sound. The missing piece is a reliable consumption measurement. Options:
- Fix `SamplesPlayed` measurement (requires fixing underruns first)
- Use QPC-vs-DAC drift measurement (what we had, but drift is tiny and hard to measure precisely)
- Use queue depth as proxy (what we tried, but reactive not proactive)

---

## What Would Actually Fix It

The correct path forward (not tried yet):

1. **Accept stutter-fix-v1 as-is** for now — queue wanders but no pops/crackles in short sessions
2. **Add a VERY slow DRC** using queue depth as proxy (not SamplesPlayed):
   - Target: 5000 frames (~114ms)
   - Correction: ±0.1% max (much smaller than ±5% we tried)
   - Update: once per second (not every 10 submits)
   - This is what RetroArch does — it doesn't use SamplesPlayed either, it uses FIFO fullness
3. **Let queue wander within bounds** — accept that perfect steady-state is impossible with two clocks
4. **Only flush as last resort** — when queue exceeds 2x target, not as routine

The mistake was trying to be too precise. RetroArch's DRC is slow and conservative. We tried to be fast and precise, which caused oscillation and unreliable measurements.

---

## Files at Reverted State

| File | State |
|------|-------|
| `dosbox-uwp/Content/XAudio2Output.h` | stutter-fix-v1 (alloc-per-submit, drain event) |
| `dosbox-uwp/Content/XAudio2Output.cpp` | stutter-fix-v1 (alloc, MAX_QUEUE flush, WaitForDrain) |
| `dosbox-uwp/Content/RetroCore.h` | No AudioResampler |
| `dosbox-uwp/Content/RetroCore.cpp` | Direct Submit(), no resampler |
| `dosbox-uwp/dosbox_uwpMain.cpp` | Simple RunFrame(), no accumulator |
| `dosbox-uwp/dosbox_uwpMain.h` | No m_frameTimeAccum/m_lastRunFrameTime |
| `dosbox-uwp/Content/AudioResampler.h` | Deleted |

---

## Lessons Learned

1. **Read your own docs first.** AUDIO-PIPELINE.md §2 already said "don't force 70fps." We did it anyway.
2. **Don't chase unreliable measurements.** SamplesPlayed showed wrong values from the start. We kept building on it.
3. **Simpler is better.** RetroArch's DRC is slow (updates once per frame, ±0.5% max). We tried fast (every 10 submits, ±5%).
4. **Chicken-and-egg problems need breaking.** DRC needs reliable clock → clock needs no underruns → underruns need DRC. Break the cycle by accepting imperfect measurement and being very conservative.
5. **Revert early.** We should have reverted after Phase 7 showed the accumulator approach works but contradicts architecture. Instead we kept adding layers.

---

### Phase 13: Full rewrite — RetroArch blocking model (current)

**Goal:** Delete all audio implementation, rewrite from scratch based on RetroArch's proven XAudio2 blocking model.

**What was done:**
- Deleted entire XAudio2Output (pool, CAS, drain event, flush cap, trend tracking, diagnostic counters)
- Rewrote with 16 pre-allocated ring buffers (FRAMES_PER_BUFFER=960 = 20ms@48kHz)
- `Submit()` blocks when ring is full via `WaitForSingleObject(hEvent)`
- `OnBufferEnd` decrements buffer count + `SetEvent(hEvent)` (auto-reset event)
- Voice auto-starts on first buffer submission
- Removed ALL accumulator/DRC/pacing logic from `dosbox_uwpMain.cpp`
- Simple model: `Update()` calls `RunFrame()` once per tick, audio backpressure paces the main thread
- Removed `m_audioTimeAccumulator`, `m_audioLastTick`, `s_diagRunsAccum`
- Removed `ConsumeVoiceStarted`, `GetAndResetUnderrunCount`, `GetTargetQueueFrames`, `WaitForDrain`, `QueuedFramesPtr`, `TotalProducedPtr`, `TotalConsumedPtr`

**Architecture:**
```
retro_run() → audio_batch_cb() → retro_audio() → Submit() [BLOCKS when ring full]
                                                         │
                                          16× RingSlot (960 frames each)
                                          OnBufferEnd → InterlockedDecrement + SetEvent
                                                         │
                                              XAudio2 source voice → DAC
```

**Key difference from all prior attempts:**
- Audio IS the frame pacer (like RetroArch), not QPC sleep
- `Submit()` blocks the main thread when ring is full → naturally paces `retro_run()` at DAC consumption rate
- No accumulator, no DRC controller, no queue-depth feedback — just blocking

**Status:** Build clean. Untested.

**Risks:**
- Blocking main thread for up to 20ms when ring is full (should be rare with 16 slots = 320ms headroom)
- `GetQueuedFrames()` is now an estimate (buffer count × FRAMES_PER_BUFFER + partial), not exact frame count
