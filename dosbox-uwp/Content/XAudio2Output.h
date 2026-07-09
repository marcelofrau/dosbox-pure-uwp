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
        bool IsReady() const { return m_initialized; }
        bool IsStarted() const { return m_started; }
        uint32_t GetQueuedFrames() const;
        static volatile long* QueuedFramesPtr();

    private:
        uint32_t GetSampleRate() const { return 44100; }

    private:
        IXAudio2* m_pXAudio2;
        IXAudio2MasteringVoice* m_pMasterVoice;
        IXAudio2SourceVoice* m_pSourceVoice;
        bool m_initialized;
        bool m_started;
    };
}
