# Main Loop Comparison: RetroArch vs. UWP

## RetroArch's Main Loop (`runloop.c`)

### Structure

```
runloop_iterate()                   // called once per frame
  ├── frame_time callback           // tells core how long last frame took
  ├── audio_buffer_status callback  // tells core buffer occupancy
  ├── core_run()                    // = retro_run()
  │     ├── retro_input_poll()
  │     ├── (CPU emulation)
  │     ├── video_cb()              // → video_driver_frame() → d3d11_gfx_frame()
  │     └── audio_cb()              // → audio_driver_sample_batch() → xa_write()
  ├── frame_limit_minimum_time      // QPC sleep (safety net only, not primary pacer)
  └── return
```

### Audio Backpressure (The Key Feature)

Inside `retro_run()`, the core produces audio. This flows through:

```
retro_audio() → audio_driver_sample_batch() → xa_write()
```

In `xa_write()` (`audio/drivers/xaudio.c` line 503-568):

```c
while (bytes)
{
    // ... fill ring buffer ...

    if (handle->bufptr == handle->bufsize)    // current buffer slot full
    {
        XAUDIO2_BUFFER xa2buffer = { ... };

        // *** THIS IS THE CRITICAL BLOCKING CALL ***
        while (handle->buffers == MAX_BUFFERS - 1)   // 16 buffers total
            if (!(WaitForSingleObject(handle->hEvent, XAUDIO_TIMEOUT) == WAIT_OBJECT_0))
                return -1;  // timeout after 256ms

        IXAudio2SourceVoice_SubmitSourceBuffer(handle->pSourceVoice, &xa2buffer, NULL);
        InterlockedIncrement((LONG volatile*)&handle->buffers);
        handle->bufptr = 0;
        handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
    }
}
```

`OnBufferEnd` signals the event:

```c
STDMETHOD_(void, OnBufferEnd)(void*)
{
    InterlockedDecrement((LONG volatile*)&buffers);
    SetEvent(hEvent);             // <-- wakes the blocked main thread
}
```

**Result:** If the core produces faster than audio consumes, `retro_run()` blocks in the audio callback until XAudio2 drains a buffer. This naturally paces the entire loop at the audio consumption rate.

### Frame Limit Sleep (Secondary Only)

From `runloop.c:7682-7688`:
> "When content is actively running behind the menu, core_run() -> audio_driver_write() already paces the iterate loop at the audio buffer's drain rate — i.e. the core's natural fps. Layering the refresh-rate retro_sleep() throttle below on top of that is redundant double-pacing, and retro_sleep() resolves to OS Sleep() whose granularity is ~15 ms on Windows by default — coarser than typical audio low-water marks, so the sleep overshoots and stutters audio."

The QPC sleep at line 7837-7879 only activates for fast-forward, VRR, or when paused:

```c
if ((frame_limit_min) && (vrr_runloop_enable || fastmotion || ...))
{
    // sleep to limit speed — NOT active during normal gameplay
}
```

### Dynamic Rate Control (DRC)

From `audio_driver.c:548-575`:

```c
static void audio_driver_control(void)
{
    size_t write_avail = audio_st->current_audio->write_avail(audio_st->context_audio_data);
    // target: 50% buffer occupancy
    int percent = (int)(100 - (write_avail * 100 / audio_st->buffer_size));
    // nudge resampler ratio ±rate_control_delta
    if (percent > 60) audio_st->rate_control = 1.0 + rate_control_delta;
    else if (percent < 40) audio_st->rate_control = 1.0 - rate_control_delta;
}
```

This fine-tunes the resampler every frame to keep buffer occupancy at ~50%. Prevents both underruns (buffer emptying) and overruns (buffer growing unbounded).

---

## Our UWP Main Loop (`dosbox_uwpMain.cpp`)

### Structure

```
Update()                              // called by DX framework at ~3000Hz
  ├── FPS tracking
  ├── SDL input poll
  ├── m_audioTimeAccumulator += dt * targetFps   // wall-clock accumulator
  ├── maxRetroRuns = f(audio_queue_depth)        // 1-5 based on queue fullness
  ├── while (accumulator >= 1.0 && maxRetroRuns > 0)
  │     ├── retro_run()                           // NEVER blocks during audio
  │     │     ├── CPU emulation
  │     │     ├── video_cb() → store frame data
  │     │     └── audio_cb() → XAudio2Output::Submit() (non-blocking)
  │     └── accumulator -= 1.0
  ├── DoPacingSleep()                // QPC Sleep targeting 70fps
  │     └── WaitForSingleObject(hTimer, ms)   // imprecise!
  └── Render()
```

### The Problem in Detail

#### Step 1: Audio Time Accumulator (lines 720-737)

```cpp
double dt = (double)(now.QuadPart) / (double)m_qpcFreq.QuadPart - m_audioLastTick;
m_audioTimeAccumulator += dt * targetFps;  // wall-clock → "frames owed"
if (m_audioTimeAccumulator > 2.0 * targetFps)  // clamp at 140 frames
    m_audioTimeAccumulator = 2.0 * targetFps;
```

This uses QPC wall-clock time, NOT audio consumption. If QPC says 14.3ms passed, accumulator += 1.0 regardless of whether audio actually consumed anything.

#### Step 2: maxRetroRuns Based on Queue Depth (lines 739-751)

```cpp
int qf = m_xaudio2->GetQueuedFrames();
if (qf > 40)       maxRetroRuns = 1;  // queue healthy, slow down
else if (qf > 20)  maxRetroRuns = 2;
else if (qf > 10)  maxRetroRuns = 3;
else               maxRetroRuns = 5;  // empty queue, produce aggressively
```

This is backpressure, but it's **advisory** — the loop still runs retro_run() up to 5 times. It never blocks.

#### Step 3: retro_run() is Non-Blocking Audio (XAudio2Output::Submit)

```cpp
void XAudio2Output::Submit(const int16_t* data, size_t frames)
{
    // lock-free CAS claim a slot from 32-slot pool
    // memcpy data into slot
    // SubmitSourceBuffer (non-blocking — queues to XAudio2)
    // InterlockedExchangeAdd(&s_queuedFrames, frames)
    // If queuedFrames > MAX_QUEUE (16000):
    //     Stop() + FlushSourceBuffers() + resubmit + Start()
    //     *** THIS IS THE AUDIO CLIP ***
}
```

**No blocking. No WaitForSingleObject.** The core runs freely.

#### Step 4: DoPacingSleep (lines 335-398)

```cpp
void DoPacingSleep()
{
    double framePeriod = 1000.0 / m_retroCore->GetTargetFps();  // 14.3ms at 70fps
    double elapsed = QPC since start of tick;
    double toSleep = framePeriod - elapsed;
    if (toSleep > 0)
    {
        // WaitForSingleObject(hTimer, (DWORD)toSleep)
        // hTimer created with CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    }
}
```

Sleep granularity (~0.5ms even with HIGH_RESOLUTION) is far coarser than the micro-adjustments needed for smooth 70fps pacing. The 15.6ms default Sleep quantum means Sleep(14ms) actually sleeps ~15.6ms — overshooting by 1.3ms every frame.

### Cumulative Effect Over Time

| Time | retro_run time | Sleep target | Actual sleep | Error | Accumulated drift |
|------|----------------|-------------|-------------|-------|-------------------|
| t=0 | 8ms | 6.3ms | ~6.5ms | +0.2ms | +0.2ms |
| t=1 | 9ms | 5.3ms | ~5.5ms | +0.2ms | +0.4ms |
| t=2 | 7ms | 7.3ms | ~7.5ms | +0.2ms | +0.6ms |
| t=10 | ... | ... | ... | ~+0.2ms | ~+2ms |
| t=70 | ... | ... | ... | ... | ~+14ms = 1 extra frame |

At 70fps target, after ~70 frames (1 second), the drift accumulates to roughly one extra frame period. This means the loop produces ~71-72 frames/second instead of 70. Audio queue grows by ~685 frames/sec (at 48kHz, 70fps = 685 samples/frame).

### The Flush (The Audio Clip)

When `s_queuedFrames > MAX_QUEUE (16000)`:

```cpp
voice->Stop(0);
voice->FlushSourceBuffers();
// re-submit current valid buffers
voice->Start(0);
```

This clears all pending audio buffers and resubmits. The listener hears this as a **pop, click, or gap** — the audio clip.

---

## Key Differences Table

| Aspect | RetroArch | UWP | Impact |
|--------|-----------|-----|--------|
| Audio blocks core | Yes — `WaitForSingleObject` | No — always non-blocking | UWP has imprecise pacing, drift |
| Sleep used for pacing | Only fast-forward/VRR | Every frame | UWP adds 15.6ms granularity jitter |
| DRC (rate control) | Yes — resampler ratio adjusted per frame | No | UWP has no fine-grained buffer management |
| Buffer count | 16 fixed | 32 pool | Enough in both cases |
| Buffer flush | Never | When queue > 16000 (~333ms) | UWP causes audible clip |
| Frame time callback | Yes — core gets accurate frame delta | No | Core can't adjust emulation speed precisely |
| Thread safety | Single-threaded | Single-threaded | Same |
