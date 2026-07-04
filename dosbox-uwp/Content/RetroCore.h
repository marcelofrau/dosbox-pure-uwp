#pragma once

#include "libretro.h"
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

struct retro_vfs_interface;

namespace dosbox_uwp
{
    struct RetroVideoFrame
    {
        std::vector<uint8_t> data;
        unsigned width = 0;
        unsigned height = 0;
        unsigned pitch = 0;
        bool valid = false;
    };

    class RetroCore
    {
    public:
        RetroCore();
        ~RetroCore();

        bool Init();
        bool LoadGame(const std::wstring& uwpPath, const std::vector<uint8_t>& romData);
        void RunFrame();
        void UnloadGame();
        void Shutdown();

        bool IsLoaded() const { return m_loaded; }
        bool IsInitialized() const { return m_initialized; }

        RetroVideoFrame GrabVideoFrame();
        void ToggleOSD();

        double GetTargetFps() const { return s_targetFps; }

        static void SetAudioDevice(unsigned int device);

        static bool IsShutdownRequested() { return s_shutdownRequested; }

        static void SetKeyState(unsigned key, bool down);
        static void SetMouseMove(int relX, int relY);
        static void SetPointer(float x, float y, bool down);

    private:
    bool m_initialized = false;
    bool m_loaded = false;

    static RetroVideoFrame s_lastFrame;
        static std::mutex s_frameMutex;
        static double s_targetFps;

        static bool s_keyboardState[RETROK_LAST];
        static retro_keyboard_event_t s_keyboardCallback;
        static retro_log_printf_t s_logCallback;
        static int s_mouseRelX;
        static int s_mouseRelY;
        static float s_ptrX;
        static float s_ptrY;
        static bool s_ptrDown;
        static bool s_shutdownRequested;

    public:
        static int retro_env(unsigned cmd, void* data);
        static void retro_video(const void* data, unsigned w, unsigned h, size_t pitch);
        static size_t retro_audio(const int16_t* data, size_t frames);
        static void retro_input_poll(void);
        static int16_t retro_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    };
}
