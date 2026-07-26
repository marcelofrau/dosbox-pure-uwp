#pragma once

#include <cstdint>
#include <windows.h>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace dosbox_uwp
{
    class XAudio2Output
    {
    public:
        static const int MAX_BUFFERS = 16;
        static const int FRAMES_PER_BUFFER = 960; // 20ms @ 48kHz (absorbs frame-time spikes)
        static const int TARGET_BUFFERS = 3;      // pace when queue > 60ms (3 × 960 = 2880 frames)

        struct RingSlot
        {
            int16_t data[FRAMES_PER_BUFFER * 2]; // stereo int16
            uint32_t frames;
        };

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

    private:
        IXAudio2* m_pXAudio2;
        IXAudio2MasteringVoice* m_pMasterVoice;
        IXAudio2SourceVoice* m_pSourceVoice;
        bool m_initialized;
        bool m_started;

        RingSlot m_ring[MAX_BUFFERS];
        volatile long m_buffers;
        int m_writeIdx;
        int m_writeOffset;
        HANDLE m_hEvent;
    };
}
