# Audio Pipeline — Spec & Handoff

> **Purpose.** This folder documents the audio engine of the DOSBox Pure UWP port.
> It is written so a **fresh agent** can pick up the work without prior context.

> **⚠️ Source of truth.** The older docs in `docs/` (`discoveries.md`,
> `ARCHITECTURE.md`, `ROADMAP.md`) may be **stale**. When they conflict with the
> actual code or git history, trust the code and git.

## Read order

1. **[AUDIO-PIPELINE.md](AUDIO-PIPELINE.md)** — How the core produces audio,
   the root cause analysis (two independent clocks), and why every past attempt
   fell short.
2. **[AUDIO-ATTEMPTS-LOG.md](AUDIO-ATTEMPTS-LOG.md)** — Complete log of all 12
   failed attempts and the current Phase 13 rewrite. Lessons learned.
3. **[RENDER-PIPELINE-D3D11.md](RENDER-PIPELINE-D3D11.md)** — Side quest: D2D → D3D11
   rendering migration notes.

## Architecture (current)

```
retro_run() → audio_batch_cb() → retro_audio() → Submit() [BLOCKS when ring full]
                                                       │
                                        16× RingSlot (960 frames each)
                                        OnBufferEnd → InterlockedDecrement + SetEvent
                                                       │
                                            XAudio2 source voice → DAC
```

- **Audio IS the frame pacer.** `Submit()` blocks the main thread when the ring
  buffer is full, naturally pacing `retro_run()` at the DAC consumption rate.
- **No accumulator, no DRC, no QPC pacing.** Just blocking.
- 16 ring buffers × 960 frames = 320ms headroom at 48kHz.
- Voice auto-starts on first buffer submission.

## Key insight

The core self-regulates audio sample production (targets ~44100 Hz regardless of
visual FPS). The real problem was that two independent clocks (QPC vs DAC crystal)
drift. The solution: let the DAC be the master clock by blocking the main thread
when the ring is full.

## Status

- Phase 0-5: scaffold, core, libretro bridge, video, XAudio2 (original)
- Phase 6-12: various audio fixes (accumulator, DRC, flush cap, etc.) — all failed
- Phase 13: **full rewrite** — RetroArch blocking model, build clean, untested
