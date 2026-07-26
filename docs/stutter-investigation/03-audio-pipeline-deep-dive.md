# Audio Pipeline Deep Dive

## XAudio2 Architecture

Both RetroArch and our UWP app use XAudio2 for audio output. The difference is in how they manage buffers and thread synchronization.

### Our XAudio2Output (`XAudio2Output.cpp`)

#### Buffer Pool: 32 Pre-Allocated Slots

```cpp
static const int POOL_SIZE = 32;

struct BufferSlot {
    volatile long inUse;      // 0 = free, 1 = claimed, 2 = submitted to XAudio2
    XAUDIO2_BUFFER xa2buf;    // XAudio2 buffer descriptor
    int16_t data[MAX_FRAME_SIZE * 2]; // actual audio data (stereo interleaved)
    uint32_t frames;          // number of valid frames in this slot
    long flushGen;            // generation counter for flush detection
};
```

**Claim mechanism** (lock-free CAS):

```cpp
BufferSlot* ClaimSlot()
{
    for (int attempt = 0; attempt < POOL_SIZE; attempt++)
    {
        long idx = InterlockedIncrement(&s_nextSlot) % POOL_SIZE;
        if (InterlockedCompareExchange(&s_pool[idx].inUse, 1, 0) == 0)
            return &s_pool[idx];
    }
    return nullptr;  // all slots in use — should never happen with 32 slots
}
```

**Submit** (called from `RetroCore::retro_audio` callback inside `retro_run()`):

```cpp
void Submit(const int16_t* data, size_t frames)
{
    BufferSlot* slot = ClaimSlot();
    if (!slot) return;  // pool exhausted — drop audio

    memcpy(slot->data, data, frames * 2 * sizeof(int16_t));  // copy audio data

    slot->xa2buf.AudioBytes = (uint32_t)(frames * 2 * sizeof(int16_t));
    slot->xa2buf.pAudioData = (const BYTE*)slot->data;
    slot->xa2buf.pContext   = slot;

    voice->SubmitSourceBuffer(&slot->xa2buf);       // non-blocking XAudio2 call
    InterlockedExchange(&slot->inUse, 2);            // mark as submitted

    long qf = InterlockedExchangeAdd(&s_queuedFrames, (long)frames) + (long)frames;

    // Auto-start voice when enough queued
    if (qf >= TARGET_QUEUE && !s_started)
    {
        voice->Start(0);
        s_started = true;
    }

    // *** FLUSH CAP — THE AUDIO CLIP ***
    if (qf > MAX_QUEUE)
    {
        voice->Stop(0);
        voice->FlushSourceBuffers();
        // ... resubmit ...
        voice->Start(0);
    }
}
```

#### OnBufferEnd (release slot, track consumption)

```cpp
STDMETHODIMP_(void) OnBufferEnd(void* pBufferContext)
{
    auto* slot = static_cast<BufferSlot*>(pBufferContext);
    if (slot->flushGen == s_flushGen)
    {
        InterlockedExchangeAdd(&s_queuedFrames, -(long)slot->frames);
    }
    InterlockedExchange(&slot->inUse, 0);   // release slot back to pool
}
```

### RetroArch's XAudio2 Driver (`xaudio.c`)

#### Ring Buffer: 16 Fixed Buffers

```c
#define MAX_BUFFERS       16
#define MAX_BUFFERS_MASK  (MAX_BUFFERS - 1)

struct xa2_handle {
    XAUDIO2_BUFFER xa_buf[MAX_BUFFERS];
    int16_t buf[MAX_BUFFERS][bufsize];       // pre-allocated ring buffer
    int write_buffer, read_buffer, buffers;   // circular indices
    HANDLE hEvent;                            // signaled by OnBufferEnd
    int bufsize;                               // bytes per buffer
    int bufptr;                                 // write position in current buffer
};
```

#### Blocking Write

```c
static ssize_t xa_write(void *data, const void *s, size_t len)
{
    xa2_handle_t* handle = (xa2_handle_t*)data;
    size_t bytes = len;

    while (bytes)
    {
        size_t to_copy = (handle->bufsize - handle->bufptr);
        if (to_copy > bytes) to_copy = bytes;

        memcpy(handle->buf[handle->write_buffer] + handle->bufptr, src, to_copy);
        handle->bufptr += to_copy;
        src += to_copy;
        bytes -= to_copy;

        if (handle->bufptr == handle->bufsize)   // buffer slot full → submit
        {
            XAUDIO2_BUFFER xa2buffer = {0};
            xa2buffer.AudioBytes = handle->bufsize;
            xa2buffer.pAudioData = handle->buf[handle->write_buffer];

            // *** BLOCK UNTIL A SLOT IS FREE ***
            while (handle->buffers == MAX_BUFFERS - 1)
            {
                DWORD ret = WaitForSingleObject(handle->hEvent, XAUDIO_TIMEOUT);
                if (ret != WAIT_OBJECT_0) return -1;  // timeout
            }

            IXAudio2SourceVoice_SubmitSourceBuffer(handle->pSourceVoice, &xa2buffer, NULL);
            InterlockedIncrement(&handle->buffers);
            handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
            handle->bufptr = 0;
        }
    }
    return len;
}
```

#### OnBufferEnd Signals the Event

```c
STDMETHOD_(void, OnBufferEnd)(void*)
{
    InterlockedDecrement((LONG volatile*)&buffers);
    SetEvent(hEvent);           // <-- unblocks the waiting xa_write()
}
```

### Critical Difference Summary

| Aspect | RetroArch | UWP |
|--------|-----------|-----|
| Buffer management | Ring buffer, 16 slots | Pool, 32 slots |
| When full | Block on WaitForSingleObject | Never block — flush at 16000 frames |
| Pool exhaustion? | Handled by blocking | Can return nullptr → audio drop |
| Frame pacing | Audio consumption drives frame rate | QPC + Sleep drives frame rate |
| Audio clip cause | None (never flushes) | Flush at MAX_QUEUE |

## Why the Blocking Model Works Better

1. **Natural rate matching:** The XAudio2 hardware runs at 48kHz. When buffers are full, blocking means "we've produced exactly as much audio as has been consumed." The core can't get ahead of real-time.

2. **No drift:** XAudio2's internal clock is sample-accurate. There's no cumulative error between "time we think passed" and "time actually passed."

3. **No flush needed:** Because production is naturally throttled, the buffer never overflows. No force-flush = no audio clip.

4. **Simpler code:** No QPC, no Sleep, no timer objects, no accumulator math. Just "fill buffer → if full, wait for XAudio2 to consume one → repeat."
