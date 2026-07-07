#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdint>
#include <windows.gaming.input.h>

namespace dosbox_uwp
{
    enum { BUTTON_A = 0, BUTTON_B = 1, BUTTON_X = 2, BUTTON_Y = 3, BUTTON_SELECT = 6, BUTTON_R3 = 11, BUTTON_L3 = 12 };

    class SdlInput
    {
    public:
        SdlInput();
        ~SdlInput();

        bool Initialize();
        void PollEvents();

        bool IsButtonHeld(int btn) const;
        bool WasButtonJustPressed(int btn);
        bool IsAnyButtonHeld() const { return m_buttonHeld[0]; }
        bool HasController() const { return m_hasController; }
        bool IsInitialized() const { return m_initialized; }
        bool HasControllerSDL() const { return m_controller != nullptr; }
        bool HasControllerUWP() const { return m_uwpGamepad != nullptr; }
        const char* GetControllerName() const { return m_controllerName; }
        const char* GetLastEventText() const { return m_lastEventStr; }
        void SetKeyboardButton(int btn, bool held);

    private:
        static const int MAX_BUTTONS = 32;
        void PollUwpGamepad();

        SDL_GameController* m_controller;
        Windows::Gaming::Input::Gamepad^ m_uwpGamepad;
        bool m_buttonHeld[MAX_BUTTONS];
        bool m_buttonJustPressed[MAX_BUTTONS];
        int m_controllerCount;
        bool m_initialized;
        bool m_hasController;
        char m_lastEventStr[64];
        char m_controllerName[128];
    };
}
