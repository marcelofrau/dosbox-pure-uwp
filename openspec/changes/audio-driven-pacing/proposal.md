## Why

Current pacing uses QPC + Sleep which has granularity (~15.6ms default, ~0.5ms with HIGH_RESOLUTION) coarser than frame period (14.3ms at 70fps). Residual drift causes audio queue to grow ~630 frames/sec, requiring periodic flush that produces audible audio clips. Stutter and audio artifacts are the primary user-facing issues.

RetroArch's proven approach: **audio backpressure** — `retro_run()` blocks inside the XAudio2 callback when all buffers are full (`WaitForSingleObject` on `hEvent` signaled by `OnBufferEnd`). The audio consumption rate becomes the frame pacer naturally, eliminating both drift and flush.

## What Changes

- Add blocking submit mode to XAudio2Output: `WaitForSingleObject` when pool exhausted, signaled by `OnBufferEnd`
- Remove QPC-based `DoPacingSleep()` entirely
- Remove `CreateWaitableTimerEx` HIGH_RESOLUTION timer
- Replace `m_audioTimeAccumulator` + `maxRetroRuns` throttle with single `retro_run()` call that blocks on audio naturally
- Reduce pool size from 32 to 16 (matching RetroArch's proven architecture)
- Implement DRC (Dynamic Rate Control) to adjust resampler ratio based on audio buffer occupancy
- Remove queue-depth flush workaround (known bug #9) — flush becomes emergency-only at much higher threshold
- Reduce pre-buffer target

## Capabilities

### New Capabilities
- `audio-backpressure-pacing`: Block retro_run() on XAudio2 buffer exhaustion to use audio consumption as frame pacer

### Modified Capabilities
- _(none — no existing specs are changing)_

## Impact

- **XAudio2Output.cpp**: Add `s_bufferAvailableEvent`, signal in `OnBufferEnd`, add `SubmitBlocking()` method, reduce pool to 16, add `AudioPacingUpdate()` for DRC
- **dosbox_uwpMain.cpp**: Remove `DoPacingSleep()`, remove accumulator, remove `maxRetroRuns` loop, simplify to single `retro_run()` per tick
- **Known bug #9**: Resolved — queue drift eliminated, flush becomes emergency-only
- **Performance**: Eliminates Sleep jitter, eliminates audio clip on flush, smooth frame pacing via audio clock
