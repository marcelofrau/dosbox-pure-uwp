#include "pch.h"
#include "dosbox_uwpMain.h"
#include "libretro.h"
#include "Common\DirectXHelper.h"
#include <cmath>
#include <sstream>
#include <SDL.h>

using namespace dosbox_uwp;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

dosbox_uwpMain::dosbox_uwpMain(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
    , m_clearColor(DirectX::Colors::CornflowerBlue)
    , m_defaultClearColor(DirectX::Colors::CornflowerBlue)
    , m_hasController(false)
    , m_eventText(L"")
    , m_eventTimer(0)
{
    QueryPerformanceFrequency(&m_qpcFreq);
    m_deviceResources->RegisterDeviceNotify(this);

    m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));
    m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));
    m_retroScreen = std::unique_ptr<RetroScreenRenderer>(new RetroScreenRenderer(m_deviceResources));
    m_retroScreen->CreateDeviceDependentResources();

    m_retroCore = std::unique_ptr<RetroCore>(new RetroCore());

    m_sdlInput = std::unique_ptr<SdlInput>(new SdlInput());
    if (m_sdlInput->Initialize())
    {
        m_hasController = m_sdlInput->HasController();
        char buf[128];
        sprintf_s(buf, "SDL: controller=%s audio=%s dev=%u\n",
            m_hasController ? "CONNECTED" : "NONE (SPACE=btnA)",
            m_sdlInput->IsAudioReady() ? "OK" : "FAIL",
            m_sdlInput->GetAudioDevice());
        OutputDebugStringA(buf);
    }

    // Wire SDL audio device so retro_audio can queue directly
    RetroCore::SetAudioDevice(m_sdlInput->GetAudioDevice());

    BootCore();
}

dosbox_uwpMain::~dosbox_uwpMain()
{
    CleanupTempFile();
    m_retroCore->Shutdown();
    m_deviceResources->RegisterDeviceNotify(nullptr);
}

void dosbox_uwpMain::BootCore()
{
    if (!m_retroCore->Init())
    {
        m_statusText = L"Core init FAILED";
        m_statusTimer = 300;
        OutputDebugStringA("[dosbox-uwp] retro_init FAILED\n");
        return;
    }

    auto localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
    std::wstring basePath = localFolder->Path->Data();

    _wmkdir((basePath + L"\\saves").c_str());
    _wmkdir((basePath + L"\\config").c_str());
    OutputDebugStringA("[dosbox-uwp] LocalFolder dirs: saves/, config/\n");

    OutputDebugStringA("[dosbox-uwp] Core initialized OK\n");
    OutputDebugStringA("[dosbox-uwp] Keyboard mapping active: VirtualKey->RETROK_\n");

    m_statusText = L"Core ready. Press F1 to load a game.";
    m_statusTimer = 300;
    m_retroRunning = true;
}

void dosbox_uwpMain::LoadRom(const std::wstring& path, std::vector<uint8_t> romData)
{
    if (!m_retroCore->Init())
    {
        m_statusText = L"Core init FAILED";
        m_statusTimer = 120;
        return;
    }

    {
        char buf[512];
        int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string pathUtf8(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &pathUtf8[0], len, nullptr, nullptr);
        sprintf_s(buf, "[dosbox-uwp] LoadRom path=%s data=%zu\n", pathUtf8.c_str(), romData.size());
        OutputDebugStringA(buf);
    }

    CleanupTempFile();
    m_currentTempPath = path;

    if (m_retroCore->LoadGame(path, romData))
    {
        m_statusText = L"Game loaded!";
        m_statusTimer = 120;
        OutputDebugStringA("[dosbox-uwp] Game loaded OK\n");
        m_retroRunning = true;
    }
    else
    {
        m_statusText = L"Load FAILED";
        m_statusTimer = 120;
        OutputDebugStringA("[dosbox-uwp] retro_load_game FAILED\n");
    }
}

void dosbox_uwpMain::CleanupTempFile()
{
    if (m_currentTempPath.empty())
        return;

    char buf[256];
    int len = WideCharToMultiByte(CP_UTF8, 0, m_currentTempPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string pathUtf8(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, m_currentTempPath.c_str(), -1, &pathUtf8[0], len, nullptr, nullptr);
    sprintf_s(buf, "[dosbox-uwp] Cleanup temp: %s\n", pathUtf8.c_str());
    OutputDebugStringA(buf);

    _wremove(m_currentTempPath.c_str());
    m_currentTempPath.clear();
}

void dosbox_uwpMain::CreateWindowSizeDependentResources()
{
    m_sceneRenderer->CreateWindowSizeDependentResources();
}

void dosbox_uwpMain::Update()
{
    m_timer.Tick([&]()
    {
        LARGE_INTEGER _t0, _t1, _t2, _t3, _freq;
        QueryPerformanceFrequency(&_freq);

        m_frameLate = false;
        m_sdlInput->PollEvents();
        QueryPerformanceCounter(&_t0);

        if (m_spaceHeld) {
            m_clearColor = DirectX::Colors::Orange;
        } else if (m_sdlInput->IsButtonHeld(BUTTON_Y)) {
            m_clearColor = DirectX::Colors::Gold;
        } else if (m_sdlInput->IsButtonHeld(BUTTON_X)) {
            m_clearColor = DirectX::Colors::RoyalBlue;
        } else if (m_sdlInput->IsButtonHeld(BUTTON_B)) {
            m_clearColor = DirectX::Colors::Crimson;
        } else if (m_sdlInput->IsButtonHeld(BUTTON_A)) {
            m_clearColor = DirectX::Colors::ForestGreen;
        } else {
            m_clearColor = m_defaultClearColor;
        }

        if (m_sdlInput->WasButtonJustPressed(BUTTON_R3) || m_sdlInput->WasButtonJustPressed(BUTTON_SELECT)) {
            m_requestFilePicker = true;
            if (m_sdlInput->WasButtonJustPressed(BUTTON_R3))
                OutputDebugStringA("[dosbox-uwp] R3 -> file picker\n");
            else
                OutputDebugStringA("[dosbox-uwp] Select -> file picker\n");
        }

        // Frame pacing: tight spin-wait to hit exact target FPS
        // No SwitchToThread — scheduler granularity (1-2ms) causes jitter,
        // accumulates audio deficit, drains SDL queue → crackle.
        m_pacingEnabled = m_retroRunning && m_retroCore->IsLoaded();
        if (m_pacingEnabled)
        {
            double targetFps = m_retroCore->GetTargetFps();
            if (targetFps > 0 && m_lastFrameTime.QuadPart != 0)
            {
                double targetMs = 1000.0 / targetFps;
                LARGE_INTEGER _now;
                do {
                    QueryPerformanceCounter(&_now);
                    double elapsedMs = (double)(_now.QuadPart - m_lastFrameTime.QuadPart) * 1000.0 / m_qpcFreq.QuadPart;
                    if (elapsedMs >= targetMs) break;
                } while (true);
            }
            QueryPerformanceCounter(&m_lastFrameTime);
            m_retroCore->RunFrame();
            // Exit if core requested shutdown (e.g. Exit from PUREMENU)
            if (RetroCore::IsShutdownRequested())
            {
                OutputDebugStringA("[dosbox-uwp] Shutdown requested by core, exiting app\n");
                Windows::ApplicationModel::Core::CoreApplication::Exit();
                return;
            }
        }
        QueryPerformanceCounter(&_t1);

        // Frame pacing stats
        {
            double frameMs = (double)(_t1.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
            double targetFps = m_retroCore->IsLoaded() ? m_retroCore->GetTargetFps() : 60.0;
            double targetMs = 1000.0 / targetFps;
            static int pacingLogCounter = 0;
            if ((++pacingLogCounter % 300) == 0)
            {
                bool skipped = !m_pacingEnabled && m_retroRunning && m_retroCore->IsLoaded();
                Uint32 sdlQueued = 0;
                if (m_sdlInput->IsAudioReady())
                    sdlQueued = SDL_GetQueuedAudioSize(m_sdlInput->GetAudioDevice());
                char buf[256];
                sprintf_s(buf, "[dosbox-uwp] PACE: target=%.0f frame=%.2fms skip=%d audioQueued=%u\n",
                    targetFps, frameMs, skipped ? 1 : 0, sdlQueued);
                OutputDebugStringA(buf);
            }
        }

        {
            const char* lastEvent = m_sdlInput->GetLastEventText();
            if (lastEvent && lastEvent[0])
            {
                m_eventText = L"";
                for (const char* p = lastEvent; *p; p++)
                    m_eventText += (wchar_t)*p;
                m_eventTimer = 60;
            }

            const wchar_t* inputSrc = m_sdlInput->HasControllerSDL() ? L"SDL" :
                m_sdlInput->HasControllerUWP() ? L"UWP" : L"KB";

            std::wstring retroStatus = m_retroCore->IsLoaded() ? L"CORE:RUNNING" :
                (m_retroCore->IsInitialized() ? L"CORE:READY" : L"CORE:OFF");

            std::wstring statusLine = (m_statusTimer > 0) ? m_statusText : L"";

            // SDL audio buffer size
            Uint32 sdlQueuedBytes = 0;
            if (m_sdlInput->IsAudioReady())
                sdlQueuedBytes = SDL_GetQueuedAudioSize(m_sdlInput->GetAudioDevice());

            // App memory usage (UWP API)
            unsigned long long memMB = 0;
            try {
                auto memUsage = Windows::System::MemoryManager::AppMemoryUsage;
                memMB = memUsage / (1024 * 1024);
            } catch (...) { }

            float currentFps = m_timer.GetFramesPerSecond();

            // Rolling late-frame rate for HUD
            {
                static int hudCounter = 0;
                if (m_frameLate) m_lateFramesHud++;
                if ((++hudCounter % 60) == 0) { m_lateFramesHud = 0; }
            }

            // Load state watchdog: if stuck in BOOTING >5s, log warning
            if (m_loadState == LOAD_BOOTING)
            {
                if (++m_loadTimer > 300)
                {
                    static bool warned = false;
                    if (!warned) { warned = true;
                        OutputDebugStringA("[dosbox-uwp] WARNING: Load stuck in BOOTING >5s (possible hang)\n");
                    }
                }
            }
            else
            {
                m_loadTimer = 0;
            }

            static const wchar_t* loadStateNames[] = {
                L"", L"PICKING...", L"READING...", L"BOOTING...", L"", L"FAILED!"
            };
            const wchar_t* loadLabel = loadStateNames[m_loadState];

            wchar_t buf[512];
            swprintf_s(buf, L"%s  SDL:%s CTL:%s AUDIO:%s INP:%s FPS:%.0f LATE:%d\n"
                L"CTLR:%hs SND:%dHz MEM:%lluMB SDLbuf:%u/%u %ls\n%ls",
                retroStatus.c_str(),
                m_sdlInput->IsInitialized() ? L"OK" : L"FAIL",
                m_hasController ? L"CONN" : L"NONE",
                m_sdlInput->IsAudioReady() ? L"OK" : L"FAIL",
                inputSrc,
                currentFps,
                m_lateFramesHud,
                m_sdlInput->GetControllerName(),
                m_sdlInput->GetAudioSampleRate(),
                memMB,
                sdlQueuedBytes,
                m_sdlInput->GetAudioDevice(),
                loadLabel,
                (m_eventTimer > 0 || m_statusTimer > 0) ?
                    (m_statusTimer > 0 ? m_statusText.c_str() : m_eventText.c_str()) : L"");

            if (m_eventTimer > 0) m_eventTimer--;
            if (m_statusTimer > 0) m_statusTimer--;
            m_fpsTextRenderer->SetDebugText(buf);
        }
        QueryPerformanceCounter(&_t2);

        m_sceneRenderer->Update(m_timer);
        m_fpsTextRenderer->Update(m_timer);

        QueryPerformanceCounter(&_t3);
        {
            static unsigned _tc = 0;
            if ((++_tc % 60) == 0)
            {
                double poll_ms  = (double)(_t0.QuadPart) * 1000.0 / _freq.QuadPart;
                double frame_ms = (double)(_t1.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
                double hud_ms   = (double)(_t2.QuadPart - _t1.QuadPart) * 1000.0 / _freq.QuadPart;
                double scene_ms = (double)(_t3.QuadPart - _t2.QuadPart) * 1000.0 / _freq.QuadPart;
                double total_ms = (double)(_t3.QuadPart) * 1000.0 / _freq.QuadPart;
                float fps       = m_timer.GetFramesPerSecond();
                Uint32 sdlBuf   = m_sdlInput->IsAudioReady() ? SDL_GetQueuedAudioSize(m_sdlInput->GetAudioDevice()) : 0;
                unsigned long long memBytes = 0;
                try { memBytes = Windows::System::MemoryManager::AppMemoryUsage; } catch (...) { }
                char _dbg[512];
                sprintf_s(_dbg, "[dosbox-uwp] TICK #%u: frame=%.1fms hud=%.1f scene=%.1f fps=%.0f "
                    "SDLbuf=%u MEM=%lluMB total=%.1f\n",
                    _tc, frame_ms, hud_ms, scene_ms, fps,
                    sdlBuf, memBytes / (1024*1024), total_ms);
                OutputDebugStringA(_dbg);
            }
        }
    });
}

bool dosbox_uwpMain::Render()
{
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    auto context = m_deviceResources->GetD3DDeviceContext();

    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
    context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

    context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), m_clearColor);
    context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (m_retroCore && m_retroCore->IsLoaded())
    {
        static int renderCount = 0;
        renderCount++;
        auto frame = m_retroCore->GrabVideoFrame();

        if ((renderCount % 300) == 0)
        {
            char buf[128];
            sprintf_s(buf, "[dosbox-uwp] Render #%d: frame.valid=%d w=%u h=%u data.size=%zu\n",
                renderCount, frame.valid, frame.width, frame.height, frame.data.size());
            OutputDebugStringA(buf);
        }

        LARGE_INTEGER _r0, _r1, _rfreq;
        QueryPerformanceFrequency(&_rfreq);
        QueryPerformanceCounter(&_r0);

        if (frame.valid)
        {
            m_retroScreen->UpdateVideoFrame(frame.data.data(), frame.width, frame.height, frame.pitch);
        }

        m_retroScreen->Render();

        QueryPerformanceCounter(&_r1);
        {
            static unsigned _rc = 0;
            if ((++_rc % 60) == 0)
            {
                double r_ms = (double)(_r1.QuadPart - _r0.QuadPart) * 1000.0 / _rfreq.QuadPart;
                char _dbg[256];
                sprintf_s(_dbg, "[dosbox-uwp] RENDER #%u: %.1fms  valid=%d %ux%u\n",
                    _rc, r_ms, frame.valid, frame.width, frame.height);
                OutputDebugStringA(_dbg);
            }
        }
    }
    else
    {
        m_sceneRenderer->Render();
    }

    m_fpsTextRenderer->Render();

    return true;
}

void dosbox_uwpMain::OnKeyEvent(Windows::System::VirtualKey key, bool down)
{
    if (!m_retroCore)
        return;

    // F1 toggles PUREMENU
    if (down && key == Windows::System::VirtualKey::F1)
    {
        m_retroCore->ToggleOSD();
    }

    int vk = (int)key;
    unsigned retroKey = RETROK_UNKNOWN;

    switch (vk)
    {
    // Control chars — VirtualKey value matches RETROK_
    case 0x08: retroKey = RETROK_BACKSPACE; break;
    case 0x09: retroKey = RETROK_TAB;       break;
    case 0x0C: retroKey = RETROK_CLEAR;     break;
    case 0x0D: retroKey = RETROK_RETURN;    break;
    case 0x1B: retroKey = RETROK_ESCAPE;    break;
    case 0x20: retroKey = RETROK_SPACE;     break;

    // Digits 0-9 — Direct ASCII match
    case 0x30: retroKey = RETROK_0; break;
    case 0x31: retroKey = RETROK_1; break;
    case 0x32: retroKey = RETROK_2; break;
    case 0x33: retroKey = RETROK_3; break;
    case 0x34: retroKey = RETROK_4; break;
    case 0x35: retroKey = RETROK_5; break;
    case 0x36: retroKey = RETROK_6; break;
    case 0x37: retroKey = RETROK_7; break;
    case 0x38: retroKey = RETROK_8; break;
    case 0x39: retroKey = RETROK_9; break;

    // Letters A-Z (VK uppercase 65-90 → RETROK lowercase 97-122)
    case 0x41: retroKey = RETROK_a; break; case 0x42: retroKey = RETROK_b; break;
    case 0x43: retroKey = RETROK_c; break; case 0x44: retroKey = RETROK_d; break;
    case 0x45: retroKey = RETROK_e; break; case 0x46: retroKey = RETROK_f; break;
    case 0x47: retroKey = RETROK_g; break; case 0x48: retroKey = RETROK_h; break;
    case 0x49: retroKey = RETROK_i; break; case 0x4A: retroKey = RETROK_j; break;
    case 0x4B: retroKey = RETROK_k; break; case 0x4C: retroKey = RETROK_l; break;
    case 0x4D: retroKey = RETROK_m; break; case 0x4E: retroKey = RETROK_n; break;
    case 0x4F: retroKey = RETROK_o; break; case 0x50: retroKey = RETROK_p; break;
    case 0x51: retroKey = RETROK_q; break; case 0x52: retroKey = RETROK_r; break;
    case 0x53: retroKey = RETROK_s; break; case 0x54: retroKey = RETROK_t; break;
    case 0x55: retroKey = RETROK_u; break; case 0x56: retroKey = RETROK_v; break;
    case 0x57: retroKey = RETROK_w; break; case 0x58: retroKey = RETROK_x; break;
    case 0x59: retroKey = RETROK_y; break; case 0x5A: retroKey = RETROK_z; break;

    // Navigation (VirtualKey values — NOT printable)
    case 0x21: retroKey = RETROK_PAGEUP;   break;
    case 0x22: retroKey = RETROK_PAGEDOWN; break;
    case 0x23: retroKey = RETROK_END;      break;
    case 0x24: retroKey = RETROK_HOME;     break;
    case 0x25: retroKey = RETROK_LEFT;     break;
    case 0x26: retroKey = RETROK_UP;       break;
    case 0x27: retroKey = RETROK_RIGHT;    break;
    case 0x28: retroKey = RETROK_DOWN;     break;
    case 0x2D: retroKey = RETROK_INSERT;   break;
    case 0x2E: retroKey = RETROK_DELETE;   break;
    case 0x2F: retroKey = RETROK_HELP;     break;

    // Modifiers
    case 0xA0: retroKey = RETROK_LSHIFT; break;
    case 0xA1: retroKey = RETROK_RSHIFT; break;
    case 0xA2: retroKey = RETROK_LCTRL;  break;
    case 0xA3: retroKey = RETROK_RCTRL;  break;
    case 0xA4: retroKey = RETROK_LALT;   break;
    case 0xA5: retroKey = RETROK_RALT;   break;

    // Super / Menu
    case 0x5B: retroKey = RETROK_LSUPER; break;
    case 0x5C: retroKey = RETROK_RSUPER; break;
    case 0x5D: retroKey = RETROK_MENU;   break;

    // Numpad
    case 0x60: retroKey = RETROK_KP0;          break;
    case 0x61: retroKey = RETROK_KP1;          break;
    case 0x62: retroKey = RETROK_KP2;          break;
    case 0x63: retroKey = RETROK_KP3;          break;
    case 0x64: retroKey = RETROK_KP4;          break;
    case 0x65: retroKey = RETROK_KP5;          break;
    case 0x66: retroKey = RETROK_KP6;          break;
    case 0x67: retroKey = RETROK_KP7;          break;
    case 0x68: retroKey = RETROK_KP8;          break;
    case 0x69: retroKey = RETROK_KP9;          break;
    case 0x6A: retroKey = RETROK_KP_MULTIPLY;  break;
    case 0x6B: retroKey = RETROK_KP_PLUS;      break;
    case 0x6D: retroKey = RETROK_KP_MINUS;     break;
    case 0x6E: retroKey = RETROK_KP_PERIOD;    break;
    case 0x6F: retroKey = RETROK_KP_DIVIDE;    break;
    case 0x6C: retroKey = RETROK_KP_ENTER;     break;

    // Function keys F1-F12
    case 0x70: retroKey = RETROK_F1;  break;
    case 0x71: retroKey = RETROK_F2;  break;
    case 0x72: retroKey = RETROK_F3;  break;
    case 0x73: retroKey = RETROK_F4;  break;
    case 0x74: retroKey = RETROK_F5;  break;
    case 0x75: retroKey = RETROK_F6;  break;
    case 0x76: retroKey = RETROK_F7;  break;
    case 0x77: retroKey = RETROK_F8;  break;
    case 0x78: retroKey = RETROK_F9;  break;
    case 0x79: retroKey = RETROK_F10; break;
    case 0x7A: retroKey = RETROK_F11; break;
    case 0x7B: retroKey = RETROK_F12; break;

    // Lock / special
    case 0x13: retroKey = RETROK_PAUSE;     break;
    case 0x14: retroKey = RETROK_CAPSLOCK;  break;
    case 0x90: retroKey = RETROK_NUMLOCK;   break;
    case 0x91: retroKey = RETROK_SCROLLOCK; break;
    case 0x2C: retroKey = RETROK_PRINT;     break;
    case 0x2A: retroKey = RETROK_PRINT;     break;
    case 0xB7: retroKey = RETROK_SYSREQ;    break;
    case 0x1C: retroKey = RETROK_BREAK;     break;

    // OEM keys — US layout physical position
    case 0xBA: retroKey = RETROK_SEMICOLON;    break;
    case 0xBB: retroKey = RETROK_EQUALS;       break;
    case 0xBC: retroKey = RETROK_COMMA;        break;
    case 0xBD: retroKey = RETROK_MINUS;        break;
    case 0xBE: retroKey = RETROK_PERIOD;       break;
    case 0xBF: retroKey = RETROK_SLASH;        break;
    case 0xC0: retroKey = RETROK_BACKQUOTE;    break;
    case 0xDB: retroKey = RETROK_LEFTBRACKET;  break;
    case 0xDC: retroKey = RETROK_BACKSLASH;    break;
    case 0xDD: retroKey = RETROK_RIGHTBRACKET; break;
    case 0xDE: retroKey = RETROK_QUOTE;        break;

    default: break;
    }

    if (retroKey != RETROK_UNKNOWN)
    {
        char buf[128];
        sprintf_s(buf, "[dosbox-uwp] Key: VK=0x%02X down=%d retroKey=%u\n", vk, down, retroKey);
        OutputDebugStringA(buf);
        RetroCore::SetKeyState(retroKey, down);
    }
}

void dosbox_uwpMain::OnDeviceLost()
{
    m_sceneRenderer->ReleaseDeviceDependentResources();
    m_fpsTextRenderer->ReleaseDeviceDependentResources();
    m_retroScreen->ReleaseDeviceDependentResources();
}

void dosbox_uwpMain::OnDeviceRestored()
{
    m_sceneRenderer->CreateDeviceDependentResources();
    m_fpsTextRenderer->CreateDeviceDependentResources();
    m_retroScreen->CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
