## ADDED Requirements

### Requirement: XAudio2 blocks core when buffer pool exhausted
When all 16 buffer slots are queued with XAudio2, the `Submit()` call SHALL block the calling thread until at least one slot is freed by `OnBufferEnd`.

#### Scenario: Pool full blocks submit
- **WHEN** `Submit()` is called and all 16 slots are in-use
- **THEN** `WaitForSingleObject(s_bufferAvailableEvent, XAUDIO_TIMEOUT)` blocks the caller
- **THEN** `OnBufferEnd` sets `s_bufferAvailableEvent` when a slot is released
- **THEN** `Submit()` unblocks, claims the freed slot, fills and submits it

#### Scenario: Blocking timeout
- **WHEN** `WaitForSingleObject` times out after `XAUDIO_TIMEOUT` (256ms)
- **THEN** `Submit()` returns 0 (frames written = 0) — audio is dropped
- **THEN** Core continues without blocking forever

### Requirement: OnBufferEnd signals event
`OnBufferEnd()` SHALL set a manual-reset event that unblocks any waiting `Submit()` caller.

#### Scenario: Event signaled
- **WHEN** `OnBufferEnd()` fires after XAudio2 finishes consuming a buffer
- **THEN** `SetEvent(s_bufferAvailableEvent)` is called
- **THEN** waiting `Submit()` thread wakes and claims freed slot

### Requirement: DoPacingSleep removed
`DoPacingSleep()` SHALL be removed. No QPC Sleep call SHALL exist in the main loop.

#### Scenario: No Sleep in main loop
- **WHEN** main loop iterates
- **THEN** no `Sleep()`, `WaitForSingleObject(hTimer)`, or `CreateWaitableTimerEx` is called

### Requirement: Main loop simplified to single retro_run
The audio accumulator and maxRetroRuns loop SHALL be removed. Each main loop tick SHALL call `retro_run()` once.

#### Scenario: Single retro_run per tick
- **WHEN** `Update()` runs
- **THEN** `m_retroCore->RunFrame()` is called exactly once
- **THEN** audio backpressure inside RunFrame naturally paces the core

### Requirement: Buffer pool reduced to 16 slots
`POOL_SIZE` SHALL be changed from 32 to 16.

#### Scenario: 16 slots
- **WHEN** XAudio2Output initializes
- **THEN** pool has 16 pre-allocated slots (down from 32)
- **THEN** blocking occurs sooner, providing tighter pacing

### Requirement: DRC adjusts resampler ratio
The system SHALL periodically sample audio buffer occupancy and adjust a resampler ratio to maintain ~50% occupancy.

#### Scenario: Buffer occupancy high
- **WHEN** queued frames > 60% of target maximum
- **THEN** resampler ratio is increased slightly (speeds up output)
- **THEN** buffer drains faster toward target

#### Scenario: Buffer occupancy low
- **WHEN** queued frames < 40% of target maximum
- **THEN** resampler ratio is decreased slightly (slows down output)
- **THEN** buffer fills toward target

### Requirement: Emergency flush threshold raised
`MAX_QUEUE` SHALL be raised from 16000 to 96000 frames (~2 seconds).

#### Scenario: Emergency flush
- **WHEN** queued frames exceed 96000 (~2s of audio)
- **THEN** flush occurs as safety net (should never trigger with blocking model)
- **THEN** diagnostic message is logged

### Requirement: TARGET_QUEUE reduced
`TARGET_QUEUE` SHALL be reduced from 2736 (~57ms) to 960 (~20ms).

#### Scenario: Quick start
- **WHEN** first audio is submitted after voice creation
- **THEN** `Start(0)` is called once queue depth >= 960 frames
- **THEN** audio playback begins after ~20ms of pre-buffer
