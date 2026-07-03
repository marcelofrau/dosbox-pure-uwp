# Tasks: Wire audio

## Task 1: Replace s_audioBuffer vector with ring buffer
- Remove or keep `s_audioBuffer` (backward compat for stubs).
- Add `s_ringBuffer` as fixed `int16_t[65536]` in RetroCore.cpp.
- Add `s_writeCursor`, `s_readCursor`, `s_ringBufferSize` (65536) static vars.
- Initialize cursors to 0 on core load.

## Task 2: Update retro_audio to write into ring buffer
- In `retro_audio()` callback, lock `s_audioMutex`.
- Write `frames * 2` samples (stereo) into ring buffer at `s_writeCursor`.
- Advance `s_writeCursor` circularly.
- Handle wrap-around: copy in two segments if near buffer end.

## Task 3: Update GrabAudio to read from ring buffer
- Remove `s_audioBuffer` clear logic.
- In `GrabAudio()`, lock `s_audioMutex`.
- Compute available frames from `(writeCursor - readCursor) % size`.
- Copy up to `maxFrames` to output buffer.
- Advance `s_readCursor` circularly.
- Return number of frames read.

## Task 4: Call GrabAudio in Update() after retro_run()
- In `dosbox_uwpMain.cpp::Update()`, after `retro_run()`:
  - Declare `int16_t audioBuf[8192]` (4096 stereo frames).
  - Call `GrabAudio(audioBuf, 4096)`.
  - If frames > 0: `SDL_QueueAudio(m_audioDevice, audioBuf, frames * 2 * sizeof(int16_t))`.

## Task 5: Re-init SDL audio at 48000Hz stereo
- In `SdlInput::Init()`, change `want.freq` from 44100 to 48000.
- Change `want.channels` from 1 to 2.
- Verify `have.freq` and `have.channels` match; log warning if not.
- Ensure `SDL_PauseAudioDevice(0)` called after init.

## Task 6: Expose m_audioDevice from SdlInput
- Add `public: SDL_AudioDeviceID GetAudioDevice() const { return m_audioDevice; }` to `SdlInput.h`.
- Or pass device ID as parameter to `Update()`.
- Ensure `m_audioDevice` is initialized (0) before SDL init.

## Task 7: Thread safety verification
- Confirm `s_audioMutex` locked in both `retro_audio` (write) and `GrabAudio` (read).
- Check no deadlock: `SDL_QueueAudio` called outside mutex scope.
- Verify no other thread touches ring buffer without lock.

## Task 8: Build and test with PC speaker / AdLib
- Build with MSBuild.
- Test with game that uses PC speaker beep (e.g. Alley Cat, Commander Keen).
- Test with AdLib music (e.g. Monkey Island, Duke Nukem II).
- Verify sound is audible and not distorted.

## Task 9: Test crackling/underflow detection
- Add debug log: `[dosbox-uwp] Audio: %u frames queued, %u in SDL buffer\n`.
- Monitor for underflow (SDL_GetQueuedAudioSize near 0 during playback).
- If crackling: increase ring buffer size or adjust tempBuf size.

## Task 10: Verify no audio leak on game load/unload
- Add `ClearAudioBuffer()` that resets cursors to 0.
- Call on game unload (`retro_unload_game`).
- Verify SDL device not leaking (SDL_CloseAudioDevice on shutdown).
