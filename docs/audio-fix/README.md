# Audio Pipeline — Spec & Handoff

> **Purpose.** This folder is a self-contained specification for fixing the audio
> engine of the DOSBox Pure UWP port. It is written so a **fresh agent** can pick
> up the work without prior context. Read the three docs in order.

> **⚠️ Source of truth.** The older docs in `docs/` (`discoveries.md`,
> `ARCHITECTURE.md`, `ROADMAP.md`) may be **stale**. When they conflict with the
> **actual code** or **git history**, trust the code and git. This spec was
> derived by reading the current source + the relevant tags, not the old docs.

## Read order

1. **[AUDIO-PIPELINE.md](AUDIO-PIPELINE.md)** — *the "why"*. How the core produces
   audio, the real root cause (two independent clocks), and why every past attempt
   fell short. Read this first or nothing else makes sense.
2. **[IMPLEMENTATION-PLAN.md](IMPLEMENTATION-PLAN.md)** — *the "what to do"*.
   Step-by-step plan to reach a stable, low-latency, crackle-free output. Concrete
   files, functions, and success criteria.
3. **[RENDER-PIPELINE-D3D11.md](RENDER-PIPELINE-D3D11.md)** — *side quest*. Notes on
   the D2D → D3D11 rendering migration and **why it must be kept separate from the
   audio work** (they were attempted together once and confounded each other).

## TL;DR

- DOSBox Pure runs emulation on a **separate thread** and **self-regulates** the
  number of audio samples per `retro_run()`. It already targets ~44100 Hz output
  regardless of visual FPS. **Do not** fight the 60-vs-70 FPS battle; the core
  already handles it.
- The real problem: the **production clock** (core, referenced to `QueryPerformanceCounter`)
  and the **consumption clock** (the XAudio2 DAC hardware crystal) are **not the
  same clock**. They drift. Over time the queue slowly grows → flush → *pop*, or
  slowly drains → underrun → *crackle*. Both reported symptoms are the same drift.
- **Best known baseline:** tag `stutter-fix-v1` (core self-regulates, `Submit()` is
  a non-blocking sink, no external pacing). It was the closest to correct.
- **The missing piece:** a slow closed-loop controller (**DRC — Dynamic Rate
  Control**) that steers the queue toward a constant depth by measuring the *real*
  DAC consumption. This eliminates drift without pitch change and without flushes.

## Decision (chosen direction)

> Fill this in once the maintainer decides. Two viable options are specified in
> `IMPLEMENTATION-PLAN.md`:
>
> - **Option B — Production-rate nudge (simpler).** No resampler. DRC nudges how
>   fast the core is drained toward a target queue depth. Recommended first step.
> - **Option A — Resampler + DRC (RetroArch-style, most robust).** Sinc resampler
>   converts core output to the DAC rate at a DRC-adjusted ratio. More code, but
>   survives heavy games and fully decouples audio from video.
>
> **Current recommendation:** start with Option B on top of `stutter-fix-v1`. Move
> to Option A only if a heavy game proves the nudge insufficient.
