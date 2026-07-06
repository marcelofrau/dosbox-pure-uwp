#pragma once

#include "libretro.h"
#include <string>
#include <vector>
#include <cstdint>

struct retro_vfs_interface;

namespace dosbox_uwp
{
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

        void ToggleOSD();

        static bool HasFrame() { return s_frameValid; }
        static const void* GetFrameData() { return s_frameData; }
        static unsigned GetFrameWidth() { return s_frameWidth; }
        static unsigned GetFrameHeight() { return s_frameHeight; }
        static unsigned GetFramePitch() { return s_framePitch; }
        static void ClearFrame() { s_frameValid = false; s_frameData = nullptr; }

        double GetTargetFps() const { return s_targetFps; }

        static void SetAudioDevice(unsigned int device);

        static bool IsShutdownRequested() { return s_shutdownRequested; }

        static void SetKeyState(unsigned key, bool down);
        static void SetMouseMove(int relX, int relY);
        static void SetPointer(float x, float y, bool down);

    private:
        bool m_initialized = false;
        bool m_loaded = false;

        static const void* s_frameData;
        static unsigned s_frameWidth;
        static unsigned s_frameHeight;
        static unsigned s_framePitch;
        static bool s_frameValid;
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
