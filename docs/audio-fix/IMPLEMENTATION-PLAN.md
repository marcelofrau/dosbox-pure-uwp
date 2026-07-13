# Audio Pipeline — Implementation Plan

> Prereq: read `AUDIO-PIPELINE.md` first. This doc is the actionable plan.
> Baseline to build on: tag **`stutter-fix-v1`** (`738d7a0`).

---

## 0. Ground rules

- **Do not** re-litigate 60-vs-70 FPS. The core self-regulates its sample rate
  (see `AUDIO-PIPELINE.md` §2). Drive `retro_run()` normally.
- **Do not** block the core/emulation thread. `Submit()` must stay non-blocking.
- **Do not** `Stop/Flush` the voice during normal playback (audible pop).
- **Do not** use `SetFrequencyRatio` for rate matching (changes pitch).
- Keep corrections **≤ ±0.5 %** (inaudible).
- Audio work and the D3D11 render migration are **separate** — do them in
  separate branches (see `RENDER-PIPELINE-D3D11.md`).

---

## 1. Already visited — do NOT redo these (from `docs/discoveries.md`)

The old `discoveries.md` may be stale, but its audio section records **dead ends
already explored**. Do not waste time re-trying:

| Approach already tried | Result | Verdict |
|------------------------|--------|---------|
| `SetFrequencyRatio()` DRC on the source voice | Pitch wow/flutter, audible | ❌ Abandoned — never bring back |
| `m_audioTimeAccumulator` scaling (`scale = targetQ/(targetQ+(q-targetQ)*0.25)`) | Partial; reactive, one frame behind on heavy loads | ⚠️ Superseded by proper DRC |
| Emergency catch-up (run up to 30 extra `RunFrame()` when queue < 500) | Band-aid for load spikes | ⚠️ Keep only as a spike guard, not steady-state |
| `Sleep()`-based pacing at 70 FPS | 15.6 ms granularity → drift | ❌ Wrong tool |
| `CreateWaitableTimerEx(HIGH_RESOLUTION)` pacing | Better precision, still QPC-referenced → drift | ⚠️ Precise but doesn't fix clock mismatch |
| `timeBeginPeriod(1)` | **Unavailable** — `winmm` not linkable in UWP AppContainer | ❌ Not an option |
| Queue-depth cap → `Stop/Flush/Start` | Bounds latency but pops | ⚠️ Keep only as a last-resort safety cap, not routine |
| `WaitForDrain` backpressure (blocking) | `OnBufferEnd` didn't fire during the wait → queue pinned then collapsed with 20+ underruns (this is exactly what `stutter-fix-v1` removed) | ❌ Do not reintroduce blocking backpressure |

**Never implemented but proposed in `discoveries.md` (this is the plan below):**
- Audio-driven pacing / PLL using `GetState().SamplesPlayed`.
- RetroArch-style resampler + DRC via `libretro-common/audio/resampler/`.

---

## 2. Step 1 — Reset to the good baseline

1. Diff current `main` audio code against `stutter-fix-v1`:
   ```
   git diff stutter-fix-v1 -- dosbox-uwp/Content/XAudio2Output.cpp \
                                dosbox-uwp/Content/XAudio2Output.h \
                                dosbox-uwp/dosbox_uwpMain.cpp
   ```
2. Restore the `stutter-fix-v1` behavior: single `RunFrame()` per loop iteration,
   `Submit()` non-blocking, no `WaitForDrain`.
3. Lower the pre-buffer target from ~150 ms to **~50–60 ms** (see constants in §5).
4. Confirm baseline behavior on device: audio plays; queue wanders (expected —
   DRC comes next).

---

## 3. Step 2 — Fix the latent bugs (do this regardless of DRC option)

These are correctness issues in `XAudio2Output.cpp` independent of the controller.

### 3a. Race in the queue counter / watermark compare
`OnBufferEnd` (`XAudio2Output.cpp:67-81`) does:
```cpp
InterlockedExchangeAdd(&s_queuedFrames, -(long)sb->frames);
...
if (s_drainEvent && s_queuedFrames < XAudio2Output::LOW_WATERMARK)  // racy read
```
The separate read of `s_queuedFrames` can be stale (another thread mutated it).
**Fix:** use the value returned by the interlocked op:
```cpp
long newQ = InterlockedExchangeAdd(&s_queuedFrames, -(long)sb->frames) - (long)sb->frames;
if (s_drainEvent && newQ < XAudio2Output::LOW_WATERMARK) SetEvent(s_drainEvent);
```
Same pattern in `Submit()` for the produce side.

### 3b. Alloc-per-Submit heap churn
`Submit()` (`XAudio2Output.cpp:216-220`) does `new int16_t[frames*2]` every call and
`delete[]` in the audio callback. Heap ops on/around the audio thread risk jitter
and priority inversion.
**Fix:** pre-allocate a **ring of N fixed buffers** (RetroArch uses 16). On submit,
copy into the next free slot; `OnBufferEnd` marks the slot free. No new/delete on
the hot path. `XA2SubmittedBuffer` becomes an index into the ring, not an owner.

### 3c. Remove routine flush
The `MAX_QUEUE` flush in `Submit()` (`XAudio2Output.cpp:240-249`) should never fire
once DRC is in place. Keep a `MAX_QUEUE` **only** as a very high safety net
(e.g. ~500 ms) and log loudly if it ever triggers (means DRC is broken).

---

## 4. Step 3 — Add the DRC controller (the actual fix)

Pick **Option B** first (simpler). Escalate to **Option A** only if a heavy game
starves despite B.

### Shared: measure the real consumption clock
Every `Submit()` (or every N submits), read the DAC position:
```cpp
XAUDIO2_VOICE_STATE vs;
m_pSourceVoice->GetState(&vs);      // vs.SamplesPlayed = DAC-clock truth
long queued = s_queuedFrames;        // frames still in flight
```
Define the target and a proportional controller around a **constant queue depth**:
```cpp
const long   TARGET   = 2205;        // ~50 ms @ 44100 (tune 1760..2650 = 40..60ms)
const double MAX_SKEW = 0.005;       // ±0.5 % — inaudible ceiling
double error  = (double)(queued - TARGET) / (double)TARGET;   // -1..+big
double adjust = 1.0 + MAX_SKEW * clamp(error, -1.0, 1.0);      // 0.995..1.005
```
- `queued > TARGET` → `adjust > 1.0` → drain/consume faster → queue shrinks.
- `queued < TARGET` → `adjust < 1.0` → produce/keep more → queue grows.

Smooth `error` with the existing trend average (`trend_avg()` already in
`XAudio2Output.cpp:191`) so the controller reacts to the *trend*, not per-buffer
noise. Update `adjust` slowly (e.g. once per 100 submits) to avoid oscillation.

### Option B — Production-rate nudge (no resampler) ✅ start here
Apply `adjust` to how the main loop drains the core. Concretely, in
`dosbox_uwpMain.cpp` where `RunFrame()` is called: maintain a fractional
accumulator and run `retro_run()` `floor(base * adjust)` times, carrying the
remainder. Because the core self-regulates samples per call, nudging the **call
cadence** by ≤0.5 % nudges aggregate production by ≤0.5 % — enough to null the
QPC-vs-DAC drift. No pitch change, no resampling.

- Pros: minimal code; no new dependency; reuses existing counters.
- Cons: correction granularity is one `retro_run()` worth of samples; fine for
  steady drift, coarse for violent spikes (handle spikes with the §1 catch-up guard).

### Option A — Resampler + DRC (RetroArch-style) — most robust
Insert a resampler between `retro_audio()` and `Submit()`:
1. Add `libretro-common/audio/resampler/` (sinc + nearest) to the build.
2. In `RetroCore::retro_audio()` (`RetroCore.cpp:577`), instead of forwarding raw,
   push through `resampler->process(ratio, in, out)` with
   `ratio = (out_rate / in_rate) * adjust` and forward the resampler output to
   `Submit()`.
3. Drive `adjust` from the same controller (§4 shared).
4. Bound `adjust` by `MAX_SKEW`. Optionally output at 48000 Hz (native Xbox mixer
   rate) to avoid a second OS-level resample.

- Pros: fully decouples audio from video; survives expensive `retro_run()`; exact.
- Cons: more code; resampler CPU cost; must manage resampler state across
  load/reset.

---

## 5. Constants — current vs proposed

| Constant | Location | Current | Proposed | Meaning |
|----------|----------|---------|----------|---------|
| `TARGET_FRAMES` | `XAudio2Output.h:31` | 6615 (150 ms) | ~2205 (50 ms) | pre-buffer before `Start()` |
| `TARGET_QUEUE` | `XAudio2Output.cpp:16` | 6615 | = target depth (2205) | DRC setpoint |
| `MAX_QUEUE` | `XAudio2Output.cpp:17` | 22050 (500 ms) | keep as safety net only | flush ceiling (should never hit) |
| `HIGH_WATERMARK` | `XAudio2Output.h:32` | 4410 | (unused w/ DRC) | legacy backpressure |
| `LOW_WATERMARK` | `XAudio2Output.h:33` | 3307 | (unused w/ DRC) | legacy backpressure |
| — | new | — | `MAX_SKEW = 0.005` | DRC correction ceiling |

Tune `TARGET` for the latency/robustness trade-off: lower = tighter latency but
less headroom for spikes.

---

## 6. Verification & success criteria

Instrument with the existing xb-xray binds (Debug build, `XB_INSPECTOR_ENABLED`):
`audio_queued`, `audio_produced`, `audio_consumed`, plus the periodic `XA2 submit
#N` log in `XAudio2Output.cpp:285`.

Connect: `nc <xbox-ip> 9000` (see xb-homebrew-vault). Watch for:

**PASS criteria (10-minute soak on a music-heavy DOS game, on real Xbox):**
- `underruns == 0` across the whole session (no `OnVoiceProcessingPassStart`
  `BytesRequired>0`).
- Queue oscillates tightly around `TARGET` (± a few ms), never trending to 0 or to
  `MAX_QUEUE`.
- **Zero** `XA2 FLUSH` log lines during normal play.
- `consumption` reported ≈ 44100 Hz (±0.5 %).
- No audible pitch change (the ≤±0.5 % skew must be inaudible — verify by ear on a
  sustained tone, e.g. a title screen with steady music).
- CPU: no core pinned at 100 % (confirms no busy spin-wait regression).

**Test matrix:**
- Light game (Doom, interpreter path) and heavy game (Duke3D/Quake, dynarec).
- CD-audio (CDA) load event — historically drained the queue; must survive with DRC
  + spike guard.
- Menu/OSD open/close, save/load state, resolution change (mode switch scraps
  audio — verify graceful recovery, no permanent desync).
- VSync on and off.

---

## 7. Suggested commit sequence

1. `revert/restore audio to stutter-fix-v1 baseline` (Step 1).
2. `fix(audio): interlocked queue race + preallocated ring buffers` (Step 2).
3. `feat(audio): DRC controller targeting constant queue depth (Option B)` (Step 3).
4. `test(audio): xb-xray DRC telemetry + soak results` (Step 4, docs update).
5. (only if needed) `feat(audio): sinc resampler + DRC (Option A)`.

Follow repo rules in `AGENTS.md`: **stage only, never commit/push without explicit
request**; core patches go in `dosbox-uwp/local/dosbox-pure/`, never the submodule.
