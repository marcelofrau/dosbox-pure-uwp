# Audio Pipeline — Architecture & Root Cause

> Read this before `IMPLEMENTATION-PLAN.md`. This explains *why* audio breaks.
> Source of truth = current code + git history, **not** the older `docs/` files.

---

## 1. The signal chain

```
┌──────────────────────── DOSBox Pure core (libretro) ────────────────────────┐
│                                                                              │
│   Emulation THREAD (separate)              Frontend THREAD (retro_run)       │
│   ─────────────────────────                ────────────────────────────      │
│   Runs DOS + mixer in real time            retro_run() = per-video-frame     │
│   Produces audio into dbp_audio[]          handshake via semaphores          │
│           │                                (DBP_ThreadControl)               │
│           └──────── semaphore sync ────────────────┘                         │
│                                             │                                │
│                                             ▼  audio_batch_cb(data, frames)  │
└─────────────────────────────────────────────┼───────────────────────────────┘
                                              │
                                              ▼
                    RetroCore::retro_audio()            [RetroCore.cpp:577]
                                              │
                                              ▼
                    XAudio2Output::Submit(data, frames) [XAudio2Output.cpp:201]
                                              │
                          new int16_t[] + SubmitSourceBuffer()
                                              │
                                              ▼
                    XAudio2 source voice → mastering voice → DAC (hardware)
                          OnBufferEnd() decrements the queue counter
```

Key files:

| File | Role |
|------|------|
| `dosbox-uwp/Content/XAudio2Output.cpp/.h` | XAudio2 sink: submit, queue accounting, callbacks |
| `dosbox-uwp/Content/RetroCore.cpp` | libretro bridge; `retro_audio()` forwards to the sink |
| `dosbox-uwp/dosbox_uwpMain.cpp` | Main loop; calls `RunFrame()` (→ `retro_run()`) |
| `dosbox-uwp/App.cpp` | `App::Run()` loop: ProcessEvents → Update → Render → Present |
| `extern/dosbox-pure/dosbox_pure_libretro.cpp` | Core; `DBP_ThreadControl` at line ~527 |

---

## 2. How the core actually produces audio (the detail that changes everything)

DOSBox Pure does **not** produce a fixed number of samples per `retro_run()`.

- The emulator runs on its **own thread**, paced to real (wall-clock) time via the
  DOSBox PIC timer and the core's `time_cb()` (which on Windows is
  `QueryPerformanceCounter`).
- `retro_run()` on the frontend thread performs a **frame handshake**
  (`DBP_ThreadControl`, `dosbox_pure_libretro.cpp:527`) and emits **however many
  samples were produced since the last call** (tracked via
  `DBP_MIXER_DoneSamplesCount()` / `dbp_audio_remain`).
- Therefore, if you call `retro_run()` at **60 Hz** instead of the core's target
  **70 Hz**, each call simply returns **more** samples (≈735 instead of ≈630). The
  aggregate output stays ≈ `av.timing.sample_rate` (44100 Hz) either way.

**Consequence:** the "60fps × 630 = 37,800 Hz deficit" analysis in the old
`discoveries.md` is **wrong for this core**. It assumed fixed 630 samples/frame.
The core self-regulates. This was empirically confirmed by tag `stutter-fix-v1`,
whose commit notes: *"Queue stabilizes around 5000–15000 frames without external
pacing."*

> **Takeaway #1:** Stop trying to make the visual loop run at exactly 70 FPS
> (spin-waits, accumulator multi-run). The core already produces the right *average*
> sample rate on its own.

---

## 3. The real root cause: two independent clocks

There are **two clocks** and they are physically different crystals:

| Clock | What it drives | Reference |
|-------|----------------|-----------|
| **Production clock** | How fast the core generates samples | `QueryPerformanceCounter` (QPC) via `time_cb()` |
| **Consumption clock** | How fast XAudio2's DAC plays samples | The audio hardware's own crystal |

QPC and the DAC crystal are never perfectly equal. The DAC might really run at
44098 Hz or 44103 Hz while the core produces a QPC-perfect 44100 Hz. The tiny
mismatch (a few Hz) **accumulates**:

- Production slightly faster than consumption → queue **slowly grows** → latency
  climbs → eventually hits a cap → the code does `Stop()+FlushSourceBuffers()` →
  audible **pop/click**.
- Production slightly slower than consumption → queue **slowly drains** → hits zero
  → XAudio2 has nothing to play → **underrun → crackle**.

**Both user-reported symptoms — "perfect but underruns" and "full of crackling" —
are the same drift**, differing only in direction and in which band-aid was active.

> **Takeaway #2:** You cannot fix drift with a bigger buffer or a better wall-clock
> timer. A buffer only delays the inevitable. You must **close the loop on the real
> consumption clock** (measure how many samples the DAC actually played, and steer
> production toward a target queue depth). That is DRC.

---

## 4. Symptom → cause map

| Symptom | Immediate cause | Underlying cause |
|---------|-----------------|------------------|
| Periodic single "pop"/click | `Submit()` hit `MAX_QUEUE` → `Stop/Flush/Start` | Drift grew the queue until the flush band-aid fired |
| Continuous crackle | XAudio2 underrun (`OnVoiceProcessingPassStart` reports `BytesRequired>0`) | Queue drained to zero (drift, or a heavy frame stalled production) |
| Latency (~150–250 ms) | `TARGET_QUEUE`/`TARGET_FRAMES` pre-buffer set high (6615 ≈ 150 ms) | Deep buffer used to *hide* drift instead of correcting it |
| Occasional stutter with VSync on | `Present(1,0)` blocks 16.7 ms at 60 Hz while core targets 70 Hz | Video pacing coupled to audio pacing |

---

## 5. History of attempts (what was tried, why each fell short)

All tags exist in the repo. Inspect with `git show <tag>`.

### `xaudio2-takeover` (`cf7974e`)
Replaced SDL audio with native XAudio2. Alloc-per-submit ring, `OnBufferEnd`
callback, queue-depth cap with flush, `CreateWaitableTimerEx(HIGH_RESOLUTION)` for
frame pacing.
- **Good:** got off SDL; native low-level control.
- **Short:** relied on a wall-clock waitable timer for pacing → same QPC-vs-DAC
  drift. The flush cap is a band-aid that pops.

### `pace-flow-fix` (`13b498e`)
Moved the QPC accumulator sleep before `ProcessEvents` for input responsiveness;
removed VSync to avoid double-wait.
- **Good:** input latency win; correct instinct that VSync + sleep pacing
  double-wait.
- **Short:** still wall-clock pacing; didn't address drift.

### `audio-fix` (`18a6e8b`) — "pre-buffer + tight spin-wait"
Still **SDL-based**. Kept SDL paused until ~93 ms queued (16384 bytes), then a
**tight busy spin-wait** on QPC to hit exactly 70.09 FPS before each `RunFrame()`
(no `SwitchToThread`, to avoid scheduler jitter).
- **Good instinct:** pace production to the core's FPS; pre-buffer masks callback
  jitter.
- **Fatal flaws:**
  1. Busy spin **pins a full CPU core at 100%** — bad for Xbox power/thermals.
  2. Couples the **video** loop to 70 FPS.
  3. Paces off **QPC, not the DAC** → drift only *masked* by the 93 ms pre-buffer;
     any stall or accumulated drift beyond 93 ms → crackle.
- Verdict: "almost there" in intent, wrong mechanism.

### `stutter-fix-v1` (`738d7a0`) — **best baseline** ⭐
Removed `WaitForDrain` audio pacing. Core self-regulates via its own thread;
`Submit()` is a **non-blocking pure sink**; no external pacing. Queue settles
5000–15000 frames.
- **Good:** cleanest architecture. No spin, no coupling, matches how the core wants
  to be driven.
- **Short (the one missing piece):** **no feedback loop.** The queue wanders freely
  and eventually (a) hits `MAX_QUEUE` → flush → pop, or (b) a heavy frame drains it
  → crackle. Also the visual loop free-runs (no cap) burning CPU.
- Verdict: **This is the foundation to build on.** It needs exactly one addition —
  DRC.

### Current `main` (HEAD)
Single `RunFrame()` per loop iteration, VSync off by default, `TARGET_QUEUE` back up
to 6615 (≈150 ms), `MAX_QUEUE` 22050 (≈500 ms). Effectively `stutter-fix-v1` with a
larger buffer to hide drift longer. Still no feedback loop → same failure modes,
just slower to manifest, at the cost of latency.

---

## 6. What "correct" looks like

Target architecture = **`stutter-fix-v1` + a slow DRC controller**:

- Core keeps self-regulating on its own thread (don't touch it).
- `Submit()` stays a **non-blocking sink** (never blocks the core thread).
- XAudio2 stays a **pure output**; **never** `Stop/Flush` during normal play.
- A **slow closed-loop controller** measures the *real* consumption
  (`IXAudio2SourceVoice::GetState().SamplesPlayed`, i.e. the DAC clock) and steers
  the queue toward a **constant target depth** (~40–60 ms) with tiny corrections
  (**≤ ±0.5 %**, inaudible — this is RetroArch's `audio_max_timing_skew`).

Two ways to apply the ≤±0.5 % correction **without changing pitch**:

- **Option B (nudge):** micro-adjust the *production/drain rate* (how aggressively
  we drain the core toward the target depth). No resampler.
- **Option A (resampler + DRC):** run the core output through a sinc resampler at
  ratio `44100 × adjust`. RetroArch's `audio/drivers/xaudio.c` and `wasapi.c` both
  do this; **neither uses `SetFrequencyRatio`** and neither implements `write_raw`.

**Never do these:**
- `SetFrequencyRatio(0.857)` to "match" a perceived 37,800 Hz → changes **pitch by
  −14 %**. Wrong on every level (the 37,800 figure is also wrong, see §2).
- `Stop()+FlushSourceBuffers()` during normal playback → audible pop. It is a
  band-aid; DRC makes it unnecessary.
- Bigger and bigger buffers to hide drift → just adds latency.

See `IMPLEMENTATION-PLAN.md` for the concrete steps.

---

## 7. Reference: RetroArch's model (validated in its source)

- `audio_driver_flush()` → `audio_driver_compute_rate_adjust()`:
  ```c
  int avail       = write_avail(ctx);          // how much the FIFO can accept
  int half_size   = buffer_size / 2;           // target = 50% full
  double direction = (avail - half_size) / (double)half_size;   // -1..+1
  double rate_adjust = 1.0 + timing_skew_delta * direction;     // ~±0.005
  src_ratio_curr  = src_ratio_orig * rate_adjust;               // feed resampler
  ```
- Proportional controller around a **50 %-full FIFO** target.
- Correction bounded by `audio_max_timing_skew` (default ~0.05, effective steps far
  smaller) → **inaudible**.
- XAudio2 driver: **16 pre-allocated ring buffers**, submits when a slot fills,
  `OnBufferEnd` uses `InterlockedDecrement` + `SetEvent`. **No** `SetFrequencyRatio`.

The DOSBox Pure port should mirror this controller. The only structural difference:
our "FIFO fullness" is `s_queuedFrames` (or `SamplesPlayed`-derived), and our target
is a fixed queue depth rather than 50 % of a fixed FIFO.
