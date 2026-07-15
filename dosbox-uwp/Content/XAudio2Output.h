#pragma once

#include <cstdint>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace dosbox_uwp
{
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
        bool ConsumeVoiceStarted(); // returns+clears flag when voice transitions started→true
        uint32_t GetQueuedFrames() const;
        uint32_t GetAndResetUnderrunCount();
        void WaitForDrain(); // Block until queue drops below HIGH_WATERMARK (call from main thread, not from Submit)
        static const long TARGET_FRAMES = 6615; // ~150ms — pre-buffer threshold
        static const long HIGH_WATERMARK = 4410; // ~100ms — start waiting when queue exceeds this
        static const long LOW_WATERMARK = 3307;  // ~75ms  — resume when queue drops below this
        static volatile long* QueuedFramesPtr();
        static volatile long long* TotalProducedPtr();
        static volatile long long* TotalConsumedPtr();

        // Buffer pool — pre-allocated slots, zero heap alloc in hot path
        static const int POOL_SIZE = 32;
        static const int MAX_FRAME_SIZE = 2048; // max frames per Submit (≈42ms@48kHz)

        struct BufferSlot
        {
            int16_t data[MAX_FRAME_SIZE * 2];
            uint32_t frames;
            long flushGen;
            volatile long inUse; // 0=free, 1=claimed by Submit, released by OnBufferEnd
        };

    private:
        uint32_t GetSampleRate() const { return 48000; }
        void EnsureDrainEvent();

        IXAudio2* m_pXAudio2;
        IXAudio2MasteringVoice* m_pMasterVoice;
        IXAudio2SourceVoice* m_pSourceVoice;
        bool m_initialized;
        bool m_started;
        bool m_voiceStartedFlag; // set by Submit when voice starts, cleared by ConsumeVoiceStarted
        HANDLE m_drainEvent;   // signaled by OnBufferEnd when queue drops below LOW_WATERMARK
    };
}
