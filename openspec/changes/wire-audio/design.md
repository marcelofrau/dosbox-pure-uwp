# Design: Wire audio pipeline

## Current state
- `retro_audio()` callback writes to `s_audioBuffer` via `push_back`.
- `GrabAudio()` returns `s_audioBuffer` then clears it (replace semantics).
- `Update()` never calls `GrabAudio()` — audio data accumulates silently or is discarded on buffer clear.
- SDL audio device opened at 44100Hz mono.
- Core outputs 48000Hz stereo `int16` frames.

## Proposed architecture

### Ring buffer (replaces `s_audioBuffer`)
- Fixed-size circular buffer: 65536 `int16` samples (~0.34s at 48kHz stereo).
- Properties: `data[]`, `size`, `writeCursor`, `readCursor`.
- Thread-safe: mutex lock on write (retro_audio callback from core thread) and read (GrabAudio from Update thread).

```
writeCursor → [W][ ][ ][ ][ ][ ][R] ← readCursor
              [Samples waiting to play]
```

### retro_audio callback
```cpp
static bool retro_audio(const int16_t* samples, size_t frames) {
    std::lock_guard<std::mutex> lock(s_audioMutex);
    size_t samplesToWrite = frames * 2; // stereo
    // Circular write: copy samples into ring buffer at writeCursor
    for (size_t i = 0; i < samplesToWrite; i++) {
        s_ringBuffer[s_writeCursor] = samples[i];
        s_writeCursor = (s_writeCursor + 1) % s_ringBufferSize;
    }
    return true;
}
```

### GrabAudio (reads from ring buffer)
```cpp
static void GrabAudio(int16_t* output, size_t maxFrames) {
    std::lock_guard<std::mutex> lock(s_audioMutex);
    size_t available = (s_writeCursor - s_readCursor + s_ringBufferSize) % s_ringBufferSize;
    size_t framesToRead = min(available / 2, maxFrames);
    for (size_t i = 0; i < framesToRead * 2; i++) {
        output[i] = s_ringBuffer[s_readCursor];
        s_readCursor = (s_readCursor + 1) % s_ringBufferSize;
    }
}
```

### Update() loop (dosbox_uwpMain.cpp)
```cpp
void Update() {
    retro_run();

    // Audio
    const size_t maxFrames = 4096; // ~85ms at 48kHz
    int16_t tempBuf[maxFrames * 2];
    GrabAudio(tempBuf, maxFrames);
    size_t framesAvailable = /* computed by GrabAudio */;
    if (framesAvailable > 0) {
        SDL_QueueAudio(m_audioDevice, tempBuf, framesAvailable * 2 * sizeof(int16_t));
    }
}
```

### SDL audio device re-init
- Change from 44100Hz mono to 48000Hz stereo to match core output.
- In `SdlInput::Init()`:

```cpp
SDL_AudioSpec want, have;
SDL_zero(want);
want.freq = 48000;
want.format = AUDIO_S16SYS;
want.channels = 2;
want.samples = 1024;
want.callback = nullptr; // push mode via QueueAudio
m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
SDL_PauseAudioDevice(m_audioDevice, 0);
```

- Verify `have.freq == 48000` and `have.channels == 2`; log warning if mismatch.
- If 48000 stereo unsupported, fall back to sample rate conversion in GrabAudio.

### Expose m_audioDevice
- Add `SDL_AudioDeviceID GetAudioDevice() const` to `SdlInput`.
- Or pass device ID to `dosbox_uwpMain` during init.

## Thread safety
- `s_audioMutex` protects ring buffer read/write cursors.
- `retro_audio` (core thread) writes, `GrabAudio` (Update thread) reads.
- No contention on `SDL_QueueAudio` (SDL is thread-safe for queue operations).

## Edge cases
- **Ring buffer full**: drop oldest frames (overwrite read cursor) or drop newest.
  - Strategy: drop oldest (let write cursor pass read cursor) —宁可丢旧不丢新.
- **Ring buffer empty**: return 0 frames, SDL underruns silently.
- **Game load/unload**: core resets audio state; ring buffer may contain stale data from old game.
  - Add `ClearAudioBuffer()` called on unload to reset cursors.
- **Audio device not opened**: guard `SDL_QueueAudio` with `m_audioDevice > 0`.
