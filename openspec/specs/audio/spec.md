# Audio Capability

## Scope
Audio samples from dosbox-pure core routed to SDL audio output.

## Requirements

### Audio buffer
- `retro_audio_sample_batch(data, frames)` receives stereo int16_t PCM
- Current: replaces buffer each frame → data loss if not consumed
- Required: ring buffer or double buffer to accumulate frames safely
- Thread-safe with `s_audioMutex`

### Sample rate management
- Core output: 48000 Hz stereo (configurable 8000-49716)
- SDL device: 44100 Hz mono (current init)
- Need sample rate conversion (48000→44100) and stereo→mono downmix
- Or: re-init SDL device at core's sample rate for optimal quality

### Output
- `GrabAudio()` returns pointer + frame count from ring buffer
- Called in `Update()` after `retro_run()`
- Sent to `SDL_QueueAudio(m_audioDevice, data, byteCount)`
- Verify no underflow/overflow with ring buffer sizing

### Core audio options
Expose via GET_VARIABLE:
- `dosbox_pure_audiorate` (sample rate)
- `dosbox_pure_volume_sb`, `volume_midi`, `volume_adlib`, `volume_speaker`, `volume_cdrom`, `volume_other`
- `dosbox_pure_sblaster_type`, `dosbox_pure_sblaster_adlib_mode`, `dosbox_pure_sblaster_adlib_emu`
- `dosbox_pure_midi`, `dosbox_pure_gus`, `dosbox_pure_tandysound`, `dosbox_pure_swapstereo`

## Affected Changes
- `wire-audio`
