# Proposed Fix: Audio Backpressure Pacing

## Overview

Replace the QPC + Sleep pacing model with RetroArch's proven approach: **use XAudio2 buffer exhaustion as the frame pacer**. The core naturally runs at the audio consumption rate because `retro_run()` blocks in the audio callback when XAudio2 buffers are full.

## Changes Required

### 1. XAudio2Output: Add Blocking Submit Mode

Introduce a blocking variant of `Submit()` that waits when all buffer slots are queued.

**New method: `BlockingSubmit()` or modify `Submit()` with a blocking flag.**

```cpp
void SubmitBlocking(const int16_t* data, size_t frames)
{
    BufferSlot* slot = ClaimSlot();
    while (!slot)
    {
        // All slots full — wait for OnBufferEnd to free one
        WaitForSingleObject(s_bufferAvailableEvent, INFINITE);
        slot = ClaimSlot();
    }

    // ... fill slot, submit to XAudio2 ...

    // If pool is now full, next call will block naturally
}
```

**New event: `s_bufferAvailableEvent`**

Signaled in `OnBufferEnd()` when a slot transitions from `inUse=2` (submitted) to `inUse=0` (free).

```cpp
STDMETHODIMP_(void) OnBufferEnd(void* pBufferContext)
{
    auto* slot = static_cast<BufferSlot*>(pBufferContext);
    if (slot->flushGen == s_flushGen)
    {
        InterlockedExchangeAdd(&s_queuedFrames, -(long)slot->frames);
    }
    InterlockedExchange(&slot->inUse, 0);
    SetEvent(s_bufferAvailableEvent);   // <-- wake blocked writer
}
```

### 2. Reduce Buffer Pool Size

Currently 32 slots with `MAX_FRAME_SIZE = 8192` frames each. At 48kHz stereo: 32 × 8192 = 262,144 frames = ~5.46 seconds of audio.

Reduce to match RetroArch's 16-buffer model. With 4096 frames/slot = 16 × 4096 = 65,536 frames = ~1.36 seconds. This ensures the blocking happens frequently enough to pace frames tightly.

### 3. Remove DoPacingSleep

Delete `DoPacingSleep()` from `dosbox_uwpMain.cpp`. Remove the `CreateWaitableTimerEx` (HIGH_RESOLUTION) from initialization since it's no longer needed.

The main loop becomes:

```cpp
// In Update():
m_retroCore->RunFrame();   // This now blocks internally on audio if needed
// No Sleep, no accumulator, no maxRetroRuns calculation
```

### 4. Simplify Main Loop

Remove:
- `m_audioTimeAccumulator` (and all its math)
- `maxRetroRuns` calculation
- `DoPacingSleep()` call
- `CreateWaitableTimerEx` call
- The `while (accumulator >= 1.0 && maxRetroRuns > 0)` loop — becomes a single `retro_run()` call

The loop becomes:

```cpp
void Update()
{
    PollInput();
    m_retroCore->RunFrame();    // blocks on audio if producing too fast
    Render();                    // draws latest frame
}
```

### 5. Implement DRC (Dynamic Rate Control)

Add periodic sampling of audio queue occupancy to adjust a resampler ratio, exactly as RetroArch's `audio_driver_control()` does.

```cpp
void AudioPacingUpdate()
{
    uint32_t qf = GetQueuedFrames();
    double occupancy = (double)qf / MAX_QUEUE_TARGET;   // 0.0 to 1.0

    if (occupancy > 0.60)
        m_rateControl += RATE_CONTROL_DELTA;    // speed up resampler slightly
    else if (occupancy < 0.40)
        m_rateControl -= RATE_CONTROL_DELTA;    // slow down resampler slightly

    // Apply to audio resampler
    m_resampler.SetRatio(m_rateControl);
}
```

This fine-tunes the resampler ratio to keep buffer occupancy at ~50%, preventing both underruns and overruns.

### 6. Optional: Frame Time Callback

If the core supports `RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK` (dosbox-pure does), register a callback that tells the core the actual frame delta. The core uses this for its internal DRC (CPU_CycleMax adjustment).

## Before vs After

### Before (Current)

```
Timer tick (~3000Hz on Windows)
  ├── Poll input
  ├── Accumulator += dt * targetFps   (QPC wall clock)
  ├── maxRetroRuns = f(queue depth)
  ├── while (accumulator >= 1 && runs < maxRetroRuns)
  │     └── retro_run()  (non-blocking audio)
  ├── DoPacingSleep()    (QPC Sleep — imprecise!)
  └── Render()
```

### After (Proposed)

```
Timer tick (~60Hz as fast as core produces)
  └── retro_run()
        ├── CPU emulation
        ├── video_cb() → store frame
        └── audio_cb() → SubmitBlocking()
              ├── fill buffer
              ├── if all buffers full → WaitForSingleObject(event)
              │     └── OnBufferEnd() → SetEvent(event) → unblock
              └── return
  └── Render()
```

## Migration Plan

1. **Add blocking event + signal in OnBufferEnd** — no behavioral change yet
2. **Add `SubmitBlocking()` method** alongside existing `Submit()`
3. **Switch RetroCore to use `SubmitBlocking()`** — this is the moment audio becomes the pacer
4. **Reduce pool size** from 32 to 16 (or appropriate)
5. **Remove DoPacingSleep** and related QPC code
6. **Simplify main loop** — remove accumulator, maxRetroRuns, while loop
7. **Add DRC** for fine-grained buffer management
8. **Test** — verify no stutter, no clipping, stable queue depth
