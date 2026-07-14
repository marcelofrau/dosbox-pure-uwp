#include "pch.h"
#include "SdlAudio.h"
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <spdlog/spdlog.h>

using namespace dosbox_uwp;

extern float DBPS_AudioMix(short* buffer, unsigned int samples, float speed, int max_wait);

bool SdlAudio::Initialize()
{
    if (m_initialized)
    {
        spdlog::warn("[SdlAudio] already initialized");
        return true;
    }

    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", 1);

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        spdlog::error("[SdlAudio] SDL_InitSubSystem FAILED: {}", SDL_GetError());
        return false;
    }
    spdlog::info("[SdlAudio] driver: {}", SDL_GetCurrentAudioDriver());

    SDL_AudioSpec desired{};
    desired.freq = 44100;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 2048;
    desired.callback = nullptr;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (dev == 0)
    {
        spdlog::error("[SdlAudio] SDL_OpenAudioDevice FAILED: {}", SDL_GetError());
        return false;
    }

    m_deviceId = dev;
    m_initialized = true;
    m_playbackStarted = false;

    spdlog::info("[SdlAudio] OK: dev={}, 44100Hz S16 stereo, queue mode (pre-buffering)", dev);
    return true;
}

void SdlAudio::Shutdown()
{
    if (m_initialized && m_deviceId != 0)
    {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
        m_initialized = false;
        m_playbackStarted = false;
        spdlog::info("[SdlAudio] shutdown");
    }
}

void SdlAudio::PullAndQueue(unsigned int targetSamples)
{
    if (!m_initialized || m_deviceId == 0 || targetSamples == 0)
        return;

    static short s_pullBuf[4096 * 2]; // stereo
    unsigned int samples = targetSamples;
    if (samples > 4096) samples = 4096;

    float speed = DBPS_AudioMix(s_pullBuf, samples, 1.0f, 10);

    unsigned int bytes = samples * 2 * sizeof(int16_t);
    if (SDL_QueueAudio(m_deviceId, s_pullBuf, bytes) < 0)
    {
        spdlog::error("[SdlAudio] SDL_QueueAudio FAILED: {}", SDL_GetError());
    }

    // Pre-buffer: wait for 150ms of audio before starting playback
    // (WASAPI consumes 46.4ms/chunk; need headroom above that)
    if (!m_playbackStarted)
    {
        unsigned int queuedBytes = SDL_GetQueuedAudioSize(m_deviceId);
        double queuedMs = (double)queuedBytes / (44100.0 * 2 * sizeof(int16_t)) * 1000.0;
        if (queuedMs >= 150.0)
        {
            SDL_PauseAudioDevice(m_deviceId, 0);
            m_playbackStarted = true;
            spdlog::info("[SdlAudio] pre-buffer OK: {:.0f}ms queued, playback started", queuedMs);
        }
    }

    // Diagnostic — every ~70 calls (~0.5s at pull rate of ~140/sec)
    static unsigned int s_callCount = 0;
    static LARGE_INTEGER s_lastPullTime = {};
    static double s_lastQueueMs = 0.0;
    static LARGE_INTEGER s_freq = {};
    if (s_freq.QuadPart == 0) QueryPerformanceFrequency(&s_freq);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double pullGapMs = 0.0;
    if (s_lastPullTime.QuadPart != 0)
        pullGapMs = (double)(now.QuadPart - s_lastPullTime.QuadPart) * 1000.0 / (double)s_freq.QuadPart;
    s_lastPullTime = now;

    unsigned int queuedBytes = SDL_GetQueuedAudioSize(m_deviceId);
    double queuedMs = (double)queuedBytes / (44100.0 * 2 * sizeof(int16_t)) * 1000.0;
    double queueDeltaMs = queuedMs - s_lastQueueMs;
    s_lastQueueMs = queuedMs;

    if ((++s_callCount % 70) == 0)
    {
        spdlog::info("[SdlAudio] p#{}: spd={:.3f} q={:.0f}ms gap={:.0f}ms dq={:+.0f}ms",
            s_callCount, speed, queuedMs, pullGapMs, queueDeltaMs);
    }
    if (queuedMs < 10.0)
        spdlog::warn("[SdlAudio] LOW Q: {:.0f}ms gap={:.0f}ms dq={:+.0f}ms spd={:.3f}",
            queuedMs, pullGapMs, queueDeltaMs, speed);
    if (pullGapMs > 30.0)
        spdlog::warn("[SdlAudio] STALL: gap={:.0f}ms (expected ~7ms) q={:.0f}ms dq={:+.0f}ms",
            pullGapMs, queuedMs, queueDeltaMs);
    if (queueDeltaMs < -55.0)
        spdlog::warn("[SdlAudio] QUEUE DROP: dq={:+.0f}ms q={:.0f}ms gap={:.0f}ms",
            queueDeltaMs, queuedMs, pullGapMs);
}

void SdlAudio::SubmitBeep(const int16_t* data, uint32_t frames)
{
    if (!m_initialized || m_deviceId == 0 || !data || frames == 0)
        return;

    uint32_t bytes = frames * 2 * sizeof(int16_t);
    if (SDL_QueueAudio(m_deviceId, data, bytes) < 0)
    {
        spdlog::error("[SdlAudio] SDL_QueueAudio (beep) FAILED: {}", SDL_GetError());
    }
    else
    {
        spdlog::info("[SdlAudio] beep queued: {} frames ({:.0f}ms)",
            frames, (double)frames * 1000.0 / 44100.0);
    }
}
