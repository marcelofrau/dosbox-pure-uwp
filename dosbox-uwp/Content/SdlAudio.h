#pragma once

#include <cstdint>

namespace dosbox_uwp
{
    class SdlAudio
    {
    public:
        bool Initialize();
        void Shutdown();
        bool IsInitialized() const { return m_initialized; }

        // Pull audio from DOSBox mixer and push to SDL audio device (queue mode).
        // targetSamples: desired samples to pull (e.g. 315 = 7ms at 44100Hz stereo)
        void PullAndQueue(unsigned int targetSamples);

        void SubmitBeep(const int16_t* data, uint32_t frames);

    private:
        bool m_initialized = false;
        bool m_playbackStarted = false;
        unsigned int m_deviceId = 0;
    };
}
