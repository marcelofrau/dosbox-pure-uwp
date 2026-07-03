# Wire audio: connect GrabAudio to SDL_QueueAudio

## Problem
`retro_audio` fills `s_audioBuffer` but `Update()` never calls `GrabAudio()`. Buffer replaces each frame instead of appending. No sound output.

## Solution
Call `GrabAudio()` in `Update()` after `retro_run()`. Send to `SDL_QueueAudio()`. Replace flat buffer with ring buffer for accumulation.

## Spec
`openspec/specs/audio/spec.md`

## Files
- `dosbox_uwpMain.cpp`
- `RetroCore.cpp`
- `SdlInput.cpp` / `SdlInput.h`
