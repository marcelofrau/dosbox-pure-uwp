#include "pch.h"
#include "XAudio2Output.h"
#include <xaudio2.h>
#include <cstring>
#include <cstdio>

using namespace dosbox_uwp;

static volatile long s_queuedFrames = 0;
static volatile long s_flushGen = 1;

class XA2VoiceCallback : public IXAudio2VoiceCallback
{
public:
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override
    {
        if (!pBufferContext) return;
        auto* sb = static_cast<XA2SubmittedBuffer*>(pBufferContext);
        if (sb->data) delete[] sb->data;
        // Ignora callbacks de buffers submetidos antes do último flush
        if (sb->flushGen == s_flushGen)
            InterlockedExchangeAdd(&s_queuedFrames, -(long)sb->frames);
        delete sb;
    }
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT Error) override
    {
        char buf[128];
        sprintf_s(buf, "[dosbox-uwp] XAudio2 voice error: 0x%08X\n", Error);
        OutputDebugStringA(buf);
    }
};

static XA2VoiceCallback s_callback;

XAudio2Output::XAudio2Output()
    : m_pXAudio2(nullptr)
    , m_pMasterVoice(nullptr)
    , m_pSourceVoice(nullptr)
    , m_initialized(false)
    , m_started(false)
{
}

XAudio2Output::~XAudio2Output()
{
    if (m_pSourceVoice)
    {
        m_pSourceVoice->Stop();
        m_pSourceVoice->FlushSourceBuffers();
    }
    Sleep(80);

    if (m_pSourceVoice) { m_pSourceVoice->DestroyVoice(); m_pSourceVoice = nullptr; }
    if (m_pMasterVoice) { m_pMasterVoice->DestroyVoice(); m_pMasterVoice = nullptr; }
    if (m_pXAudio2)     { m_pXAudio2->Release(); m_pXAudio2 = nullptr; }
}

bool XAudio2Output::Initialize()
{
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = 176400;

    HRESULT hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        char buf[128];
        sprintf_s(buf, "[dosbox-uwp] XAudio2Create FAILED: 0x%08X\n", hr);
        OutputDebugStringA(buf);
        return false;
    }

    hr = m_pXAudio2->CreateMasteringVoice(
        &m_pMasterVoice, 2, 44100, 0, nullptr, nullptr, AudioCategory_GameEffects);
    if (FAILED(hr))
    {
        char buf[128];
        sprintf_s(buf, "[dosbox-uwp] CreateMasteringVoice FAILED: 0x%08X\n", hr);
        OutputDebugStringA(buf);
        m_pXAudio2->Release();
        m_pXAudio2 = nullptr;
        return false;
    }

    hr = m_pXAudio2->CreateSourceVoice(
        &m_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
        &s_callback, nullptr, nullptr);
    if (FAILED(hr))
    {
        char buf[128];
        sprintf_s(buf, "[dosbox-uwp] CreateSourceVoice FAILED: 0x%08X\n", hr);
        OutputDebugStringA(buf);
        m_pMasterVoice->DestroyVoice();
        m_pMasterVoice = nullptr;
        m_pXAudio2->Release();
        m_pXAudio2 = nullptr;
        return false;
    }

    m_initialized = true;
    s_queuedFrames = 0;

    HRESULT hrStart = m_pSourceVoice->Start(0);
    if (SUCCEEDED(hrStart))
    {
        m_started = true;
        OutputDebugStringA("[dosbox-uwp] XAudio2Output: initialized OK, voice started\n");
    }
    else
    {
        char dbg[128];
        sprintf_s(dbg, "[dosbox-uwp] XAudio2 Start in init FAILED: 0x%08X\n", hrStart);
        OutputDebugStringA(dbg);
    }
    return true;
}

void XAudio2Output::Submit(const int16_t* data, uint32_t frames)
{
    if (!m_initialized || !data || frames == 0 || !m_pSourceVoice)
        return;

    static const long MAX_QUEUE = 2520;

    long q = s_queuedFrames;
    if (q > MAX_QUEUE)
    {
        char dbg[128];
        sprintf_s(dbg, "[dosbox-uwp] XA2 queue=%ld > %ld, flushing\n", q, (long)MAX_QUEUE);
        OutputDebugStringA(dbg);

        m_pSourceVoice->Stop();
        m_pSourceVoice->FlushSourceBuffers();
        s_queuedFrames = 0;
        InterlockedIncrement(&s_flushGen);
    }

    auto* sb = new XA2SubmittedBuffer();
    sb->frames = frames;
    sb->flushGen = s_flushGen;
    sb->data = new int16_t[(size_t)frames * 2];
    memcpy(sb->data, data, (size_t)frames * 2 * sizeof(int16_t));

    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = (UINT32)(frames * 2 * sizeof(int16_t));
    buf.pAudioData = (const BYTE*)sb->data;
    buf.pContext = sb;

    m_pSourceVoice->SubmitSourceBuffer(&buf);
    InterlockedExchangeAdd(&s_queuedFrames, (long)frames);

    if (q > MAX_QUEUE)
    {
        m_pSourceVoice->Start(0);
        m_started = true;
    }

    static uint32_t submitCounter = 0;
    if ((++submitCounter % 500) == 0)
    {
        long q2 = s_queuedFrames;
        char dbg[128];
        sprintf_s(dbg, "[dosbox-uwp] XA2 submit #%u: frames=%u queue=%ld (%.0fms)\n",
            submitCounter, frames, q2, (double)q2 * 1000.0 / 44100.0);
        OutputDebugStringA(dbg);
    }
}

void XAudio2Output::Flush()
{
    if (!m_initialized || !m_pSourceVoice)
        return;

    m_pSourceVoice->Stop();
    m_pSourceVoice->FlushSourceBuffers();
    s_queuedFrames = 0;
    InterlockedIncrement(&s_flushGen);
    m_started = false;
}

uint32_t XAudio2Output::GetQueuedFrames() const
{
    return (uint32_t)s_queuedFrames;
}

volatile long* XAudio2Output::QueuedFramesPtr()
{
    return &s_queuedFrames;
}
