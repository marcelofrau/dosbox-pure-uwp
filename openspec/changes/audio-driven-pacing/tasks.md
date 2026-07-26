## 1. Add Blocking Event + Signal in OnBufferEnd

- [ ] 1.1 Add `HANDLE s_bufferAvailableEvent` (auto-reset event) to XAudio2Output
- [ ] 1.2 Create event in Initialize() with `CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS)`
- [ ] 1.3 In `OnBufferEnd()`, add `SetEvent(s_bufferAvailableEvent)` after releasing slot
- [ ] 1.4 Close event handle in destructor

## 2. Add Blocking Submit Method

- [ ] 2.1 Add `bool SubmitBlocking(const int16_t* data, size_t frames)` to XAudio2Output
- [ ] 2.2 In ClaimSlot(): if no slot free, `WaitForSingleObject(s_bufferAvailableEvent, XAUDIO_TIMEOUT)` and retry
- [ ] 2.3 If timeout, write diagnostic log and return false (audio dropped)
- [ ] 2.4 Same data flow: memcpy → SubmitSourceBuffer → track queue

## 3. Switch RetroCore to Blocking Submit

- [ ] 3.1 In `RetroCore::retro_audio()`, change from `Submit()` to `SubmitBlocking()`
- [ ] 3.2 Handle return value: if false (timeout/drop), core continues without audio this frame

## 4. Reduce Pool Size and Slot Size

- [ ] 4.1 Change `POOL_SIZE` from 32 to 16
- [ ] 4.2 Reduce `MAX_FRAME_SIZE` from 8192 to 4096 frames per slot
- [ ] 4.3 Adjust `TARGET_QUEUE` from 2736 to 960

## 5. Remove DoPacingSleep and QPC Timer

- [ ] 5.1 Remove `DoPacingSleep()` function from dosbox_uwpMain.cpp
- [ ] 5.2 Remove `CreateWaitableTimerEx` HIGH_RESOLUTION call from initialization
- [ ] 5.3 Remove all DoPacingSleep() call sites in Update()

## 6. Simplify Main Loop

- [ ] 6.1 Remove `m_audioTimeAccumulator` variable and all accumulator math
- [ ] 6.2 Remove `maxRetroRuns` calculation and the `while (accumulator >= 1 && maxRetroRuns > 0)` loop
- [ ] 6.3 Replace with single `m_retroCore->RunFrame()` call
- [ ] 6.4 Remove `m_audioLastTick` QPC tracking for audio accumulator

## 7. Implement DRC (Dynamic Rate Control)

- [ ] 7.1 Add `AudioPacingUpdate()` method to XAudio2Output
- [ ] 7.2 Sample `s_queuedFrames` and compute occupancy ratio
- [ ] 7.3 If >60%: increase rate control
- [ ] 7.4 If <40%: decrease rate control
- [ ] 7.5 Apply ratio to audio resampler if available

## 8. Raise Emergency Flush Threshold

- [ ] 8.1 Change `MAX_QUEUE` from 16000 to 96000 frames
- [ ] 8.2 Add diagnostic log when flush triggers (should never happen in normal operation)

## 9. Test and Validate

- [ ] 9.1 Build project with changes
- [ ] 9.2 Verify no audio clipping during 10+ minute gameplay session
- [ ] 9.3 Verify audio queue stays stable (no unbounded growth)
- [ ] 9.4 Verify no periodic flushes occur
- [ ] 9.5 Verify frame pacing feels smooth
- [ ] 9.6 Verify graceful behavior when XAudio2 is slow or stalls (timeout)
