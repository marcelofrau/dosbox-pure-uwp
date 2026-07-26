#include "pch.h"
#include "XAudio2Output.h"
#include <xaudio2.h>
#include <cstring>
#include <spdlog/spdlog.h>

using namespace dosbox_uwp;

// Voice callback — fires on XAudio2's internal thread.
// Decrements buffer count and signals the write event.
class RingCallback : public IXAudio2VoiceCallback
{
public:
    volatile long* m_pBuffers;
    HANDLE m_hEvent;

    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}

    void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override
    {
        if (!pBufferContext) return;
        auto* slot = static_cast<XAudio2Output::RingSlot*>(pBufferContext);
        (void)slot;
        InterlockedDecrement(m_pBuffers);
        SetEvent(m_hEvent);
    }
};

XAudio2Output::XAudio2Output()
    : m_pXAudio2(nullptr)
    , m_pMasterVoice(nullptr)
    , m_pSourceVoice(nullptr)
    , m_initialized(false)
    , m_started(false)
    , m_buffers(0)
    , m_writeIdx(0)
    , m_writeOffset(0)
    , m_hEvent(nullptr)
{
    memset(m_ring, 0, sizeof(m_ring));
}

XAudio2Output::~XAudio2Output()
{
    if (m_pSourceVoice)
    {
        m_pSourceVoice->Stop(0);
        m_pSourceVoice->FlushSourceBuffers();
    }
    Sleep(50);

    if (m_pSourceVoice) { m_pSourceVoice->DestroyVoice(); m_pSourceVoice = nullptr; }
    if (m_pMasterVoice) { m_pMasterVoice->DestroyVoice(); m_pMasterVoice = nullptr; }
    if (m_pXAudio2) { m_pXAudio2->Release(); m_pXAudio2 = nullptr; }
    if (m_hEvent) { CloseHandle(m_hEvent); m_hEvent = nullptr; }
}

bool XAudio2Output::Initialize()
{
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 48000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = 192000;

    m_hEvent = CreateEventW(nullptr, FALSE /*auto-reset*/, FALSE /*initially non-signaled*/, nullptr);
    if (!m_hEvent)
    {
        spdlog::error("XAudio2Output: CreateEvent failed");
        return false;
    }

    HRESULT hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        spdlog::error("XAudio2Output: XAudio2Create FAILED 0x{:08lX}", (unsigned long)hr);
        return false;
    }

    hr = m_pXAudio2->CreateMasteringVoice(
        &m_pMasterVoice, 2, 48000, 0, nullptr, nullptr, AudioCategory_GameEffects);
    if (FAILED(hr))
    {
        spdlog::error("XAudio2Output: CreateMasteringVoice FAILED 0x{:08lX}", (unsigned long)hr);
        m_pXAudio2->Release(); m_pXAudio2 = nullptr;
        return false;
    }

    static RingCallback s_callback;
    s_callback.m_pBuffers = &m_buffers;
    s_callback.m_hEvent = m_hEvent;

    hr = m_pXAudio2->CreateSourceVoice(
        &m_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
        &s_callback, nullptr, nullptr);
    if (FAILED(hr))
    {
        spdlog::error("XAudio2Output: CreateSourceVoice FAILED 0x{:08lX}", (unsigned long)hr);
        m_pMasterVoice->DestroyVoice(); m_pMasterVoice = nullptr;
        m_pXAudio2->Release(); m_pXAudio2 = nullptr;
        return false;
    }

    m_initialized = true;
    m_started = false;
    m_buffers = 0;
    m_writeIdx = 0;
    m_writeOffset = 0;

    spdlog::info("XAudio2Output: initialized (ring={} slots, {}ms each)",
        MAX_BUFFERS, FRAMES_PER_BUFFER * 1000 / 48000);
    return true;
}

void XAudio2Output::Submit(const int16_t* data, uint32_t frames)
{
    if (!m_initialized || !data || frames == 0 || !m_pSourceVoice)
        return;

    const int16_t* src = data;
    uint32_t remaining = frames;
    bool ringBlocked = false;

    // ── Phase 1: Copy data into slots, submit full slots to XAudio2 ──
    while (remaining > 0)
    {
        // If current slot is full, submit it and move to next
        if (m_writeOffset >= FRAMES_PER_BUFFER)
        {
            RingSlot& slot = m_ring[m_writeIdx];

            XAUDIO2_BUFFER buf = {};
            buf.AudioBytes = (UINT32)(FRAMES_PER_BUFFER * 2 * sizeof(int16_t));
            buf.pAudioData = (const BYTE*)slot.data;
            buf.pContext = &slot;

            m_pSourceVoice->SubmitSourceBuffer(&buf);
            InterlockedIncrement(&m_buffers);

            m_writeIdx = (m_writeIdx + 1) % MAX_BUFFERS;
            m_writeOffset = 0;

            // Auto-start after first buffer
            if (!m_started)
            {
                m_pSourceVoice->Start(0);
                m_started = true;
            }

            // Hard guard: if ring is completely full, block until slot frees
            while (m_buffers >= MAX_BUFFERS)
            {
                ringBlocked = true;
                WaitForSingleObject(m_hEvent, 256);
            }
        }

        // Copy into current slot
        RingSlot& slot = m_ring[m_writeIdx];
        uint32_t space = FRAMES_PER_BUFFER - m_writeOffset;
        uint32_t chunk = (remaining < space) ? remaining : space;
        memcpy(slot.data + m_writeOffset * 2, src, chunk * 2 * sizeof(int16_t));
        m_writeOffset += chunk;
        src += chunk * 2;
        remaining -= chunk;
    }

    // If a partial slot has data, submit it now (don't wait for next call)
    if (m_writeOffset > 0 && m_writeOffset < FRAMES_PER_BUFFER)
    {
        RingSlot& slot = m_ring[m_writeIdx];

        XAUDIO2_BUFFER buf = {};
        buf.AudioBytes = (UINT32)(m_writeOffset * 2 * sizeof(int16_t));
        buf.pAudioData = (const BYTE*)slot.data;
        buf.pContext = &slot;

        m_pSourceVoice->SubmitSourceBuffer(&buf);
        InterlockedIncrement(&m_buffers);

        m_writeIdx = (m_writeIdx + 1) % MAX_BUFFERS;
        m_writeOffset = 0;

        if (!m_started)
        {
            m_pSourceVoice->Start(0);
            m_started = true;
        }

        while (m_buffers >= MAX_BUFFERS)
        {
            ringBlocked = true;
            WaitForSingleObject(m_hEvent, 256);
        }
    }

    // ── Phase 2: pacing — block when queue exceeds TARGET ──
    // Prevents queue overflow when core produces faster than audio consumes.
    // At 60fps, deficit is unavoidable; pacing only prevents overfill.
    // Adaptive extra RunFrame in Update() compensates the deficit.
    bool paceBlocked = false;
    while (m_buffers > TARGET_BUFFERS)
    {
        paceBlocked = true;
        WaitForSingleObject(m_hEvent, 256);
    }

    // Diagnostic: log every 100th call
    {
        static int submitCount = 0;
        if ((++submitCount % 100) == 0)
        {
            spdlog::info("[XAudio2] Submit #{}: {} frames, q={}, ring={}, pace={}",
                submitCount, frames, (long)m_buffers, ringBlocked, paceBlocked);
        }
    }
}

void XAudio2Output::Flush()
{
    if (!m_initialized || !m_pSourceVoice)
        return;

    m_pSourceVoice->Stop(0);
    m_pSourceVoice->FlushSourceBuffers();
    m_buffers = 0;
    m_writeIdx = 0;
    m_writeOffset = 0;
    m_started = false;
    ResetEvent(m_hEvent);

    spdlog::info("XAudio2Output: flushed, voice stopped");
}

void XAudio2Output::Start()
{
    if (!m_initialized || !m_pSourceVoice || m_started)
        return;
    m_pSourceVoice->Start(0);
    m_started = true;
}

void XAudio2Output::Stop()
{
    if (!m_initialized || !m_pSourceVoice || !m_started)
        return;
    m_pSourceVoice->Stop(0);
    m_started = false;
}

uint32_t XAudio2Output::GetQueuedFrames() const
{
    // Estimate: queued buffers * FRAMES_PER_BUFFER + partial frames in current slot
    long bufs = m_buffers;
    int partial = (m_writeOffset > 0 && m_started) ? m_writeOffset : 0;
    uint32_t estimate = (uint32_t)(bufs * FRAMES_PER_BUFFER + partial);
    if (estimate > 300000) estimate = 300000; // sanity cap
    return estimate;
}
