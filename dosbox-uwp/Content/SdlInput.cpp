#include "pch.h"
#include "SdlInput.h"
#include <cstring>
#include <vector>
#include <cmath>

using namespace dosbox_uwp;

using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;

SdlInput::SdlInput()
    : m_controller(nullptr)
    , m_audioDevice(0)
    , m_uwpGamepad(nullptr)
    , m_controllerCount(0)
    , m_initialized(false)
    , m_hasController(false)
    , m_audioReady(false)
    , m_audioSampleRate(0)
{
    memset(m_buttonHeld, 0, sizeof(m_buttonHeld));
    memset(m_buttonJustPressed, 0, sizeof(m_buttonJustPressed));
    m_lastEventStr[0] = '\0';
    m_controllerName[0] = '\0';
}

SdlInput::~SdlInput()
{
    m_uwpGamepad = nullptr;
    if (m_audioDevice > 0)
    {
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    if (m_controller)
    {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
    }
    if (m_initialized)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);
    }
}

bool SdlInput::Initialize()
{
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) < 0)
    {
        OutputDebugStringA("SDL_Init FAILED\n");
        return false;
    }
    m_initialized = true;

    // init SDL audio device at 44100Hz stereo (matches core output)
    SDL_AudioSpec desired = {};
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;

    SDL_AudioSpec obtained = {};
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (m_audioDevice > 0)
    {
        m_audioReady = true;
        m_audioSampleRate = obtained.freq;
        char buf[128];
        sprintf_s(buf, "SDL audio: %dHz %dch %d samples/period\n",
            obtained.freq, obtained.channels, obtained.samples);
        OutputDebugStringA(buf);
        if (obtained.freq != 44100 || obtained.channels != 2)
            OutputDebugStringA("[dosbox-uwp] WARNING: SDL audio format mismatch (expected 44100Hz stereo)\n");
        // Start paused — retro_audio will unpause after pre-buffer fills
        SDL_PauseAudioDevice(m_audioDevice, 1);
    }
    else
    {
        char buf[128];
        sprintf_s(buf, "SDL audio: FAILED (%s)\n", SDL_GetError());
        OutputDebugStringA(buf);
    }

    m_controllerCount = SDL_NumJoysticks();
    if (m_controllerCount > 0)
    {
        m_controller = SDL_GameControllerOpen(0);
        m_hasController = (m_controller != nullptr);
        if (m_hasController)
        {
            const char* name = SDL_GameControllerName(m_controller);
            strcpy_s(m_controllerName, sizeof(m_controllerName), name ? name : "Unknown");
            char buf[128];
            sprintf_s(buf, "Controller: %s\n", m_controllerName);
            OutputDebugStringA(buf);
        }
    }
    else
    {
        OutputDebugStringA("No SDL controller. Will try UWP Gamepad API...\n");
        m_hasController = false;
    }
    return true;
}

void SdlInput::PollEvents()
{
    // clear edge-triggered flags
    memset(m_buttonJustPressed, 0, sizeof(m_buttonJustPressed));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_CONTROLLERDEVICEADDED:
        {
            int joystickIndex = event.cdevice.which;
            if (!m_controller)
            {
                m_controller = SDL_GameControllerOpen(joystickIndex);
                if (m_controller)
                {
                    m_hasController = true;
                    const char* name = SDL_GameControllerName(m_controller);
                    strcpy_s(m_controllerName, sizeof(m_controllerName), name ? name : "Unknown");
                    // close UWP fallback since SDL now owns the controller
                    m_uwpGamepad = nullptr;
                    sprintf_s(m_lastEventStr, "CTL:CONNECTED %s", m_controllerName);
                    char buf[256];
                    sprintf_s(buf, "SDL controller connected: %s\n", m_controllerName);
                    OutputDebugStringA(buf);
                }
            }
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED:
        {
            SDL_GameController* removed = SDL_GameControllerFromInstanceID(event.cdevice.which);
            if (removed == m_controller)
            {
                SDL_GameControllerClose(m_controller);
                m_controller = nullptr;
                m_hasController = false;
                m_controllerName[0] = '\0';
                memset(m_buttonHeld, 0, sizeof(m_buttonHeld));
                sprintf_s(m_lastEventStr, "CTL:DISCONNECTED");
                OutputDebugStringA("SDL controller disconnected\n");
                // UWP fallback will re-open in PollUwpGamepad next frame
            }
            break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
        {
            int btn = event.cbutton.button;
            if (btn >= 0 && btn < MAX_BUTTONS)
            {
                m_buttonHeld[btn] = true;
                m_buttonJustPressed[btn] = true;
                sprintf_s(m_lastEventStr, "CTL:btn%d DOWN", btn);
            }
            break;
        }
        case SDL_CONTROLLERBUTTONUP:
        {
            int btn = event.cbutton.button;
            if (btn >= 0 && btn < MAX_BUTTONS)
            {
                m_buttonHeld[btn] = false;
                sprintf_s(m_lastEventStr, "CTL:btn%d UP", btn);
            }
            break;
        }
        case SDL_KEYDOWN:
            if (!event.key.repeat && event.key.keysym.sym == SDLK_SPACE)
            {
                m_buttonHeld[BUTTON_A] = true;
                m_buttonJustPressed[BUTTON_A] = true;
                sprintf_s(m_lastEventStr, "KB:SPACE=A DOWN");
            }
            break;
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_SPACE)
            {
                m_buttonHeld[BUTTON_A] = false;
                sprintf_s(m_lastEventStr, "KB:SPACE=A UP");
            }
            break;
        }
    }

    // Poll current button state as safety net for missed SDL events
    if (m_controller)
    {
        struct { SDL_GameControllerButton sdl; int local; } map[] = {
            { SDL_CONTROLLER_BUTTON_A, BUTTON_A },
            { SDL_CONTROLLER_BUTTON_B, BUTTON_B },
            { SDL_CONTROLLER_BUTTON_X, BUTTON_X },
            { SDL_CONTROLLER_BUTTON_Y, BUTTON_Y },
            { SDL_CONTROLLER_BUTTON_LEFTSTICK, BUTTON_L3 },
            { SDL_CONTROLLER_BUTTON_RIGHTSTICK, BUTTON_R3 },
            { SDL_CONTROLLER_BUTTON_BACK, BUTTON_SELECT },
        };
        for (auto& m : map)
        {
            bool held = SDL_GameControllerGetButton(m_controller, m.sdl) != 0;
            if (held && !m_buttonHeld[m.local])
                m_buttonJustPressed[m.local] = true;
            m_buttonHeld[m.local] = held;
        }
    }

    // UWP Gamepad API: works on Xbox where SDL joystick API is unavailable
    // only used when SDL controller is not connected
    if (!m_controller)
        PollUwpGamepad();
}

void SdlInput::SetKeyboardButton(int btn, bool held)
{
    if (btn >= 0 && btn < MAX_BUTTONS)
    {
        m_buttonHeld[btn] = held;
        if (held)
        {
            m_buttonJustPressed[btn] = true;
            sprintf_s(m_lastEventStr, "KB:btn%d %s", btn, "DOWN");
        }
        else
        {
            sprintf_s(m_lastEventStr, "KB:btn%d %s", btn, "UP");
        }
    }
}

void SdlInput::PollUwpGamepad()
{
    if (!m_uwpGamepad)
    {
        auto gamepads = Gamepad::Gamepads;
        if (gamepads->Size > 0)
        {
            m_uwpGamepad = gamepads->GetAt(0);
            m_hasController = true;
            strcpy_s(m_controllerName, sizeof(m_controllerName), "UWP Gamepad");
            OutputDebugStringA("UWP Gamepad connected\n");
        }
    }

    if (m_uwpGamepad)
    {
        auto reading = m_uwpGamepad->GetCurrentReading();

        struct { GamepadButtons flag; int btn; const char* name; } map[] = {
            { GamepadButtons::A, BUTTON_A, "A" },
            { GamepadButtons::B, BUTTON_B, "B" },
            { GamepadButtons::X, BUTTON_X, "X" },
            { GamepadButtons::Y, BUTTON_Y, "Y" },
            { GamepadButtons::LeftThumbstick, BUTTON_L3, "L3" },
            { GamepadButtons::RightThumbstick, BUTTON_R3, "R3" },
            { GamepadButtons::View, BUTTON_SELECT, "Select" },
        };

        for (auto& m : map)
        {
            bool held = (reading.Buttons & m.flag) != GamepadButtons::None;
            if (held && !m_buttonHeld[m.btn])
            {
                m_buttonHeld[m.btn] = true;
                m_buttonJustPressed[m.btn] = true;
                sprintf_s(m_lastEventStr, "UWP:%s DOWN", m.name);
            }
            else if (!held && m_buttonHeld[m.btn])
            {
                m_buttonHeld[m.btn] = false;
                sprintf_s(m_lastEventStr, "UWP:%s UP", m.name);
            }
        }
    }
}

bool SdlInput::IsButtonHeld(int btn) const
{
    if (btn < 0 || btn >= MAX_BUTTONS) return false;
    return m_buttonHeld[btn];
}

bool SdlInput::WasButtonJustPressed(int btn)
{
    if (btn < 0 || btn >= MAX_BUTTONS) return false;
    bool result = m_buttonJustPressed[btn];
    m_buttonJustPressed[btn] = false;
    return result;
}

void SdlInput::PlayBeep(float frequency, float duration, float volume)
{
    if (!m_audioReady) return;

    unsigned numSamples = (unsigned)(m_audioSampleRate * duration);
    if (numSamples < 1) numSamples = 1;

    std::vector<int16_t> buffer(numSamples);
    for (unsigned i = 0; i < numSamples; i++)
    {
        float t = (float)i / (float)m_audioSampleRate;
        float sample = sinf(2.0f * 3.14159265f * frequency * t);
        buffer[i] = (int16_t)(sample * volume * 32767.0f);
    }

    if (SDL_QueueAudio(m_audioDevice, buffer.data(), (Uint32)(buffer.size() * sizeof(int16_t))) == 0)
    {
        OutputDebugStringA("Beep (SDL)\n");
    }
    else
    {
        char buf[128];
        sprintf_s(buf, "Beep SDL FAILED: %s\n", SDL_GetError());
        OutputDebugStringA(buf);
    }
}
