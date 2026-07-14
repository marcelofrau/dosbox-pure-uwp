# SDL Audio Attempts Log

> **Purpose:** Track attempts to get clean audio via the SDL pull model (DBPS_AudioMix).
> Separate from XAudio2 attempts in `AUDIO-ATTEMPTS-LOG.md`.

---

## Architecture

```
retro_run() → emulation thread produces audio into mixer
                ↓
PullAndQueue() → DBPS_AudioMix() → reads from mixer.done → SDL_QueueAudio() → WASAPI
```

- Frontend is the **sole consumer** of the mixer (core's audio pipeline skipped via `#ifndef DBP_STANDALONE`)
- `DBPS_AudioMix` handles rate matching internally (stretches/compresses)
- `retro_audio()` is a no-op — audio_batch_cb is never called

---

## Attempt 1: DBP_STANDALONE_AUDIO enabled (BROKEN)

**What:** Added `#define DBP_STANDALONE_AUDIO 1` + changed 4 guards in local copy.

**Result:** Core's internal audio pipeline activated → `MIXER_CallBack` consumes mixer → `audio_batch_cb` → no-op → data discarded. `DBPS_AudioMix` finds empty mixer. `have=31` instead of `have=630`.

**Fix:** Reverted all audio-related changes. `#ifndef DBP_STANDALONE` skips core's audio pipeline.

**Lesson:** Never patch core's audio pipeline. Frontend is sole consumer via DBPS_AudioMix.

---

## Current State

**Xargon:** Clean audio, no crackling detected.
**Rally Championship:** Some audio artifacts reported. Diagnostic logging added:
- `[SdlAudio] pull: samples=N speed=X.XXXX queue=XX.Xms` every ~1 second
- `speed` = DBPS_AudioMix stretch factor (1.0 = perfect, >1 = stretched, <1 = compressed)
- `queue` = SDL_QueueAudio buffer depth in ms

**Next:** User to reproduce Rally artifacts and share logs for analysis.
