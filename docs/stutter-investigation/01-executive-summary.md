# Stutter & Audio Clip Investigation — Executive Summary

## The Problem

Users report occasional stutter and audio clipping during gameplay on the DOSBox Pure Unleashed UWP frontend. The emulation speed is adequate (stable ~70fps), but the frame pacing is uneven, causing perceptible hitches and audible artifacts in the audio output.

## Root Cause (TL;DR)

**The main loop paces frames using QPC + Sleep, which has coarser granularity (~15.6ms default, ~0.5ms with HIGH_RESOLUTION) than the frame period (14.3ms at 70fps).** This causes:

1. **Imprecise timing:** Sleep overshoots or undershoots the target, creating micro-stutter
2. **Cumulative drift:** Residual error accumulates, causing the audio queue to grow unbounded
3. **Forced flush:** When queue exceeds 882 frames (~20ms), all buffers are flushed and resubmitted — this causes the audible **clip**
4. **No audio backpressure:** The core runs as fast as it can, decoupled from audio consumption

## The Fix

Replace QPC + Sleep pacing with **audio backpressure**, exactly as RetroArch does:

1. Make `XAudio2Output::Submit()` **block** the calling thread when all buffer slots are queued (via `WaitForSingleObject` on an event set by `OnBufferEnd`)
2. The core naturally paces itself because `retro_run()` can't complete until XAudio2 consumes audio
3. Remove `DoPacingSleep()` entirely
4. Implement DRC (Dynamic Rate Control) to adjust resampler ratio based on audio buffer occupancy

## RetroArch Comparison

| Aspect | RetroArch | Our UWP |
|--------|-----------|---------|
| Primary pacer | Audio backpressure (blocking write) | QPC + Sleep |
| Audio blocking | `WaitForSingleObject(hEvent, 256ms)` when 16/16 buffers full | Never blocks — flush at 882 frames |
| XAudio2 buffers | 16 × `(latency * rate / 1000 * 2 * sizeof(int16_t))` | 32 × `MAX_FRAME_SIZE` |
| DRC | Resampler ratio ±rate_control_delta per frame | None |
| Sleep | Only for fast-forward / VRR mode | Every frame |
| Audio buffer occupancy callback | Core can query and adjust | Not implemented |
| Frame time callback | Core gets actual frame delta | Not implemented |

## Key Insight

The audio pipeline in RetroArch serves dual purpose:
1. **Audio output** (obviously)
2. **Frame pacer** (the blocking write throttles the core naturally)

Our audio pipeline only does #1. We added QPC Sleep to do #2, but it's fundamentally less precise than using the audio hardware clock itself.
