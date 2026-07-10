#pragma once

#include <cstdint>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace dosbox_uwp
{
    struct XA2SubmittedBuffer
    {
        int16_t* data;
        uint32_t frames;
        long flushGen;
    };

    class XAudio2Output
    {
    public:
        XAudio2Output();
        ~XAudio2Output();

        bool Initialize();
        void Submit(const int16_t* data, uint32_t frames);
        void Flush();
        void Start();
        void Stop();
        bool IsReady() const { return m_initialized; }
        bool IsStarted() const { return m_started; }
        uint32_t GetQueuedFrames() const;
        static const long TARGET_FRAMES = 3307; // ~75ms @44100Hz
        static volatile long* QueuedFramesPtr();
        static volatile long long* TotalProducedPtr();
        static volatile long long* TotalConsumedPtr();

    private:
        uint32_t GetSampleRate() const { return 44100; }

        IXAudio2* m_pXAudio2;
        IXAudio2MasteringVoice* m_pMasterVoice;
        IXAudio2SourceVoice* m_pSourceVoice;
        bool m_initialized;
        bool m_started;
    };
}
