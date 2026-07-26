## Context

Full investigation documented in `docs/stutter-investigation/`. Key finding: RetroArch uses audio backpressure as the primary frame pacer — `retro_run()` blocks in `xa_write()` when all 16 XAudio2 buffers are full, waiting on `WaitForSingleObject(hEvent)` signaled by `OnBufferEnd`. Our UWP loop uses QPC + Sleep (imprecise) + accumulator (drift-prone) + flush (audio clip). The fix is to adopt RetroArch's proven blocking model.

## Goals / Non-Goals

**Goals:**
- Eliminate audio clipping caused by forced queue flush
- Eliminate stutter from imprecise QPC Sleep pacing
- Match RetroArch's audio backpressure model: retro_run() blocks when audio buffers full
- Implement DRC for fine-grained buffer management
- Remove QPC Sleep, HIGH_RESOLUTION timer, accumulator math

**Non-Goals:**
- Change audio format (48kHz stereo 16-bit stays)
- Change buffer slot data structure (pre-allocated pool stays)
- Change XAudio2 voice creation / destroy lifecycle
- RetroArch parity on all timing features (frame time callback not required for fix)

## Decisions

1. **Blocking submit over QPC Sleep** — RetroArch proves this works. Audio consumption is the one true clock that matches real-time. QPC math always drifts.

2. **Pool reduction 32→16** — RetroArch uses 16 buffers. Smaller pool means blocking happens more frequently, giving tighter pacing. Each slot at current MAX_FRAME_SIZE (8192) × 16 = 131,072 frames = ~2.7s. With reduced slot size (4096) = 65,536 frames = ~1.36s.

3. **Event-driven blocking vs polling** — `WaitForSingleObject(event, INFINITE)` is zero-CPU wait vs polling `GetQueuedFrames()`. OnBufferEnd sets the event on the XAudio2 callback thread.

4. **Single retro_run() per tick** — Without QPC accumulator, there's no "catch-up" logic. The audio blocking naturally prevents over-production. If emulation is slow, audio queue drops and retro_run() returns faster (no blocking). Self-regulating.

5. **DRC via occupancy sampling** — Periodically check buffer fullness and nudge a resampler ratio. Target 40-60% occupancy. Prevents long-term drift from any residual clock skew between audio hardware and emulation.

6. **Emergency flush at much higher threshold** — Change from 16000 to 96000 frames (~2s). Should never trigger during normal operation. Acts as safety net only.

## Risks / Trade-offs

- [Deadlock] Blocking inside retro_audio callback while XAudio2 calls OnBufferEnd from same thread → Not possible. XAudio2 callbacks run on a separate audio thread. Main thread blocks on event, audio thread signals event. Safe.

- [First-frame stall] No audio produced yet → retro_audio not called → no blocking → retro_run returns immediately. First frame runs free, then audio begins, then blocking engages. Same as RetroArch.

- [Blocking too long] If audio device stalls, WaitForSingleObject could hang → Use XAUDIO_TIMEOUT (256ms) like RetroArch. If timeout, return from Submit (drop audio) and let the core continue.

- [Xbox vs Windows behavior] XAudio2 on Xbox Series may have different buffer/threading behavior → Test on target hardware. Event signaling fundamentals are identical.

- [DRC complexity] Adding resampler ratio adjustment requires resampler object → Need to verify if current pipeline has a resampler or if one needs to be created.
