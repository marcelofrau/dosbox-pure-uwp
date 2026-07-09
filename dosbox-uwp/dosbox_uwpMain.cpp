#include "pch.h"
#include "dosbox_uwpMain.h"
#include "libretro.h"
#include "Common\DirectXHelper.h"
#include <cmath>
#include <sstream>
#include <SDL.h>

#ifdef XB_INSPECTOR_ENABLED
#include <xray/inspector.hpp>
// File-scope variables expostas ao Lua REPL do XB-Inspector
struct PerfStats {
    double frame_ms, poll_ms, hud_ms, render_ms, total_ms;
    double target_fps;
    float fps;
    int audio_queued;
    char rom_name[256];
};
static PerfStats s_perf{};
static float s_debug_fps = 0.0f;
static double s_debug_target_fps = 60.0;
static double s_debug_frame_ms = 0.0;
static double s_debug_poll_ms = 0.0;
static double s_debug_hud_ms = 0.0;
static double s_debug_render_ms = 0.0;
static double s_debug_total_ms = 0.0;
static char s_rom_name[256] = "(none)";
#endif

using namespace dosbox_uwp;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Windows::System::Profile;
using namespace Concurrency;

dosbox_uwpMain::dosbox_uwpMain(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
    , m_clearColor{ 0.1764705882f, 0.1764705882f, 0.1764705882f, 1.0f }
    , m_defaultClearColor{ 0.1764705882f, 0.1764705882f, 0.1764705882f, 1.0f }
    , m_hasController(false)
    , m_eventText(L"")
    , m_eventTimer(0)
{
    QueryPerformanceFrequency(&m_qpcFreq);
    m_hFrameTimer = CreateWaitableTimerEx(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    m_deviceResources->RegisterDeviceNotify(this);

    m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));
    m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));
    m_retroScreen = std::unique_ptr<RetroScreenRenderer>(new RetroScreenRenderer(m_deviceResources));
    m_retroScreen->CreateDeviceDependentResources();

    m_retroCore = std::unique_ptr<RetroCore>(new RetroCore());

    m_xaudio2 = std::unique_ptr<XAudio2Output>(new XAudio2Output());
    if (m_xaudio2->Initialize())
    {
        RetroCore::SetAudioOutput(m_xaudio2.get());
        OutputDebugStringA("[dosbox-uwp] XAudio2: initialized\n");
    }
    else
    {
        OutputDebugStringA("[dosbox-uwp] XAudio2: FAILED to initialize\n");
    }

    m_sdlInput = std::unique_ptr<SdlInput>(new SdlInput());
    if (m_sdlInput->Initialize())
    {
        m_hasController = m_sdlInput->HasController();
        char buf[128];
        sprintf_s(buf, "SDL: controller=%s\n",
            m_hasController ? "CONNECTED" : "NONE (SPACE=btnA)");
        OutputDebugStringA(buf);
    }

#ifdef XB_INSPECTOR_ENABLED
    {
        std::string logPath;
        auto family = AnalyticsInfo::VersionInfo->DeviceFamily;
        std::wstring fw(family->Data());
        if (fw == L"Windows.Xbox")
        {
            CreateDirectoryA("E:\\dosbox", NULL);
            CreateDirectoryA("E:\\dosbox\\logs", NULL);
            logPath = "E:\\dosbox\\logs\\";
        }
        else
        {
            char tmp[MAX_PATH];
            if (GetTempPathA(MAX_PATH, tmp) != 0)
            {
                std::string dir(tmp);
                if (!dir.empty() && dir.back() == '\\')
                    dir.pop_back();
                CreateDirectoryA((dir + "\\dosbox-pure").c_str(), NULL);
                CreateDirectoryA((dir + "\\dosbox-pure\\logs").c_str(), NULL);
                logPath = dir + "\\dosbox-pure\\logs\\";
            }
        }
        xb::Xray::set_log_path(logPath.c_str());
        xb::Xray::start("DOSBox-Pure");
        xb::Xray::bind("audio_queued", (long*)XAudio2Output::QueuedFramesPtr());
        xb::Xray::bind("fps", &s_debug_fps);
        xb::Xray::bind("target_fps", &s_debug_target_fps);
        xb::Xray::bind("frame_ms", &s_debug_frame_ms);
        xb::Xray::bind("poll_ms", &s_debug_poll_ms);
        xb::Xray::bind("hud_ms", &s_debug_hud_ms);
        xb::Xray::bind("render_ms", &s_debug_render_ms);
        xb::Xray::bind("total_ms", &s_debug_total_ms);
        xb::Xray::bind_string("rom_name", s_rom_name, sizeof(s_rom_name));

        // Grouped struct — demonstrates bind_struct API
        static constexpr xb::struct_field perf_fields[] = {
            xb::field("frame_ms",     &PerfStats::frame_ms),
            xb::field("poll_ms",      &PerfStats::poll_ms),
            xb::field("hud_ms",       &PerfStats::hud_ms),
            xb::field("render_ms",    &PerfStats::render_ms),
            xb::field("total_ms",     &PerfStats::total_ms),
            xb::field("target_fps",   &PerfStats::target_fps),
            xb::field("fps",          &PerfStats::fps),
            xb::field("audio_queued", &PerfStats::audio_queued),
            xb::field("rom_name",     &PerfStats::rom_name),
        };
        xb::Xray::bind_struct("perf", &s_perf, perf_fields,
            sizeof(perf_fields) / sizeof(perf_fields[0]));
        xb::Xray::set_on_terminate([]() {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        });
        spdlog::info("{}", "--- XB-Inspector ---");
        spdlog::info("File log dir: {}", logPath.empty() ? "(none)" : logPath);
        uint16_t bp = xb::Xray::bound_port();
        spdlog::info("ODS: ON  |  TCP port: {} ({})  |  REPL: ON", bp, bp ? "OK" : "BIND FAILED");

        if (bp == 0) {
            spdlog::error("[xray] TCP bind failed, continuing without inspector");
        }
        else {
            spdlog::info("[xray] TCP port {} bound", bp);
        }
    }
#endif

    BootCore();
}

dosbox_uwpMain::~dosbox_uwpMain()
{
#ifdef XB_INSPECTOR_ENABLED
    xb::Xray::stop();
#endif
    CleanupTempFile();
    m_retroCore->Shutdown();
    m_deviceResources->RegisterDeviceNotify(nullptr);
    if (m_hFrameTimer)
    {
        CloseHandle(m_hFrameTimer);
        m_hFrameTimer = nullptr;
    }
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

    m_statusText = L"Core ready. Press F11 to load a game. F10 = Puremenu.";
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

        // Extract filename for xray binding
        auto slash = pathUtf8.find_last_of("/\\");
        std::string fname = (slash != std::string::npos) ? pathUtf8.substr(slash + 1) : pathUtf8;
        strncpy_s(s_rom_name, fname.c_str(), sizeof(s_rom_name) - 1);
        s_rom_name[sizeof(s_rom_name) - 1] = '\0';
        strncpy_s(s_perf.rom_name, fname.c_str(), sizeof(s_perf.rom_name) - 1);
        s_perf.rom_name[sizeof(s_perf.rom_name) - 1] = '\0';
    }

    CleanupTempFile();
    m_currentTempPath = path;

    if (m_retroCore->LoadGame(path, romData))
    {
        m_statusText = L"Game loaded!";
        m_statusTimer = 120;
        OutputDebugStringA("[dosbox-uwp] Game loaded OK\n");
        m_retroRunning = true;
        m_defaultClearColor = DirectX::Colors::Black;
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

void dosbox_uwpMain::DoPacingSleep()
{
    if (!m_retroRunning || !m_retroCore->IsLoaded())
        return;

    double targetFps = m_retroCore->GetTargetFps();
    if (targetFps <= 0)
        return;

    LONGLONG framePeriod = (LONGLONG)((double)m_qpcFreq.QuadPart / targetFps);

    if (m_lastFrameTime.QuadPart != 0)
    {
        m_lastFrameTime.QuadPart += framePeriod;

        LARGE_INTEGER _now;
        QueryPerformanceCounter(&_now);

        if (m_lastFrameTime.QuadPart - _now.QuadPart > framePeriod * 3)
            m_lastFrameTime.QuadPart = _now.QuadPart + framePeriod;

        if (_now.QuadPart < m_lastFrameTime.QuadPart)
        {
            double remainingMs = (double)(m_lastFrameTime.QuadPart - _now.QuadPart) * 1000.0 / m_qpcFreq.QuadPart;

            if (remainingMs > 2.0 && m_hFrameTimer)
            {
                LARGE_INTEGER dueTime;
                dueTime.QuadPart = -(LONGLONG)((remainingMs - 1.0) * 10000.0);
                SetWaitableTimer(m_hFrameTimer, &dueTime, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(m_hFrameTimer, INFINITE);
            }

            do {
                QueryPerformanceCounter(&_now);
            } while (_now.QuadPart < m_lastFrameTime.QuadPart);
        }
    }
    else
    {
        QueryPerformanceCounter(&m_lastFrameTime);
    }
}

void dosbox_uwpMain::Update()
{
#ifdef XB_INSPECTOR_ENABLED
    xb::Xray::update();
#endif

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

        if (m_sdlInput->WasButtonJustPressed(BUTTON_L3) && m_retroCore && m_retroCore->IsLoaded()) {
            OutputDebugStringA("[dosbox-uwp] L3 -> toggle PUREMENU\n");
            m_retroCore->ToggleOSD();
        }

        if (m_retroRunning && m_retroCore->IsLoaded())
        {
            PollMouseButtons();
            PollKeyboard();
            // Sync physical gamepad → libretro JOYPAD state.
            // SdlInput button IDs (BUTTON_A=0..BUTTON_R3=11) don't match
            // libretro RETRO_DEVICE_ID_JOYPAD_* (B=0, Y=1, SELECT=2, ... R3=15).
            // Until full mapping table is wired, ClearJoypad ensures no stale state.
            //
            // TODO(gamepad): Once SdlInput is connected to a real controller,
            // build a translation table here:
            //   RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_B,   m_sdlInput->IsButtonHeld(BUTTON_A));
            //   RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_Y,   m_sdlInput->IsButtonHeld(BUTTON_X));
            //   RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_SELECT, m_sdlInput->IsButtonHeld(BUTTON_SELECT));
            //   ... etc.
            for (unsigned i = 0; i < 16; i++)
                RetroCore::SetJoypadButton(i, false);
            m_retroCore->RunFrame();
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
#ifdef XB_INSPECTOR_ENABLED
            s_debug_fps = m_timer.GetFramesPerSecond();
            s_debug_target_fps = targetFps;
            s_debug_frame_ms = frameMs;
            s_perf.fps = s_debug_fps;
            s_perf.target_fps = s_debug_target_fps;
            s_perf.frame_ms = s_debug_frame_ms;
            s_perf.audio_queued = m_xaudio2 ? m_xaudio2->GetQueuedFrames() : 0;
#endif
            static int pacingLogCounter = 0;
            if ((++pacingLogCounter % 600) == 0)
            {
                uint32_t audioQueued = 0;
                if (m_xaudio2 && m_xaudio2->IsStarted())
                    audioQueued = m_xaudio2->GetQueuedFrames();
                char buf[256];
                sprintf_s(buf, "[dosbox-uwp] PACE: target=%.0f frame=%.2fms audioQueued=%u\n",
                    targetFps, frameMs, audioQueued);
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
            swprintf_s(buf, L"%s  SDL:%s CTL:%s XA2:%s INP:%s FPS:%.0f LATE:%d\n"
                L"CTLR:%hs MEM:%lluMB %ls\n%ls",
                retroStatus.c_str(),
                m_sdlInput->IsInitialized() ? L"OK" : L"FAIL",
                m_hasController ? L"CONN" : L"NONE",
                (m_xaudio2 && m_xaudio2->IsReady()) ? L"OK" : L"FAIL",
                inputSrc,
                currentFps,
                m_lateFramesHud,
                m_sdlInput->GetControllerName(),
                memMB,
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
#ifdef XB_INSPECTOR_ENABLED
        s_debug_poll_ms = (double)(_t0.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_hud_ms = (double)(_t2.QuadPart - _t1.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_render_ms = (double)(_t3.QuadPart - _t2.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_total_ms = (double)(_t3.QuadPart) * 1000.0 / _freq.QuadPart;
        s_perf.poll_ms = s_debug_poll_ms;
        s_perf.hud_ms = s_debug_hud_ms;
        s_perf.render_ms = s_debug_render_ms;
        s_perf.total_ms = s_debug_total_ms;
#endif
        {
            static unsigned _tc = 0;
            if ((++_tc % 600) == 0)
            {
                double poll_ms  = (double)(_t0.QuadPart) * 1000.0 / _freq.QuadPart;
                double frame_ms = (double)(_t1.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
                double hud_ms   = (double)(_t2.QuadPart - _t1.QuadPart) * 1000.0 / _freq.QuadPart;
                double scene_ms = (double)(_t3.QuadPart - _t2.QuadPart) * 1000.0 / _freq.QuadPart;
                double total_ms = (double)(_t3.QuadPart) * 1000.0 / _freq.QuadPart;
                float fps       = m_timer.GetFramesPerSecond();
                uint32_t xa2Frames = 0;
                if (m_xaudio2 && m_xaudio2->IsStarted())
                    xa2Frames = m_xaudio2->GetQueuedFrames();
                unsigned long long memBytes = 0;
                try { memBytes = Windows::System::MemoryManager::AppMemoryUsage; } catch (...) { }
                char _dbg[512];
                sprintf_s(_dbg, "[dosbox-uwp] TICK #%u: frame=%.1fms hud=%.1f scene=%.1f fps=%.0f "
                    "XA2frames=%u MEM=%lluMB total=%.1f\n",
                    _tc, frame_ms, hud_ms, scene_ms, fps,
                    xa2Frames, memBytes / (1024*1024), total_ms);
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
        bool haveFrame = m_retroCore->HasFrame();

        if ((renderCount % 600) == 0)
        {
            char buf[128];
            sprintf_s(buf, "[dosbox-uwp] Render #%d: frame.valid=%d w=%u h=%u\n",
                renderCount, haveFrame,
                m_retroCore->GetFrameWidth(), m_retroCore->GetFrameHeight());
            OutputDebugStringA(buf);
        }

        LARGE_INTEGER _r0, _r1, _rfreq;
        QueryPerformanceFrequency(&_rfreq);
        QueryPerformanceCounter(&_r0);

        if (haveFrame)
        {
            m_retroScreen->UpdateVideoFrame(
                (const uint8_t*)m_retroCore->GetFrameData(),
                m_retroCore->GetFrameWidth(),
                m_retroCore->GetFrameHeight(),
                m_retroCore->GetFramePitch());
            m_retroCore->ClearFrame();
        }

        m_retroScreen->Render();

        QueryPerformanceCounter(&_r1);
        {
            static unsigned _rc = 0;
            if ((++_rc % 600) == 0)
            {
                double r_ms = (double)(_r1.QuadPart - _r0.QuadPart) * 1000.0 / _rfreq.QuadPart;
                char _dbg[256];
                sprintf_s(_dbg, "[dosbox-uwp] RENDER #%u: %.1fms  valid=%d %ux%u\n",
                    _rc, r_ms, haveFrame,
                    m_retroCore->GetFrameWidth(), m_retroCore->GetFrameHeight());
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

void dosbox_uwpMain::ToggleOSD()
{
    if (!m_retroCore)
        return;
    m_retroCore->ToggleOSD();
}

void dosbox_uwpMain::OnKeyEvent(Windows::System::VirtualKey key, bool down, uint32_t scanCode, bool isExtended)
{
    if (!m_retroCore)
        return;

    int vk = (int)key;
    unsigned retroKey = RETROK_UNKNOWN;

    switch (vk)
    {
    // Modifiers — UWP VirtualKey enum (not Win32 VK_)
    case 0x10:
        retroKey = (scanCode == 0x36) ? RETROK_RSHIFT : RETROK_LSHIFT;
        break;
    case 0x11:
        retroKey = isExtended ? RETROK_RCTRL : RETROK_LCTRL;
        break;
    case 0x12:
        retroKey = isExtended ? RETROK_RALT : RETROK_LALT;
        break;

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

    if (vk >= 0 && vk < 256)
        m_activeVKeyState[vk] = down;

    if (retroKey != RETROK_UNKNOWN)
    {
        if (!m_retroCore->IsLoaded())
        {
            char buf[128];
            sprintf_s(buf, "[dosbox-uwp] Key: VK=0x%02X ignored — core not loaded\n", vk);
            OutputDebugStringA(buf);
        }
        else
        {
            char buf[128];
            sprintf_s(buf, "[dosbox-uwp] Key: VK=0x%02X down=%d retroKey=%u\n", vk, down, retroKey);
            OutputDebugStringA(buf);
            RetroCore::SetKeyState(retroKey, down);
        }
    }
}

void dosbox_uwpMain::PollKeyboard()
{
    if (!m_retroCore) return;
    try
    {
        auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
        if (!window) return;
        for (int vk = 0; vk < 256; vk++)
        {
            if (!m_activeVKeyState[vk]) continue;
            auto state = window->GetKeyState(static_cast<Windows::System::VirtualKey>(vk));
            bool stillDown = (state & Windows::UI::Core::CoreVirtualKeyStates::Down) != Windows::UI::Core::CoreVirtualKeyStates::None;
            if (!stillDown)
            {
                m_activeVKeyState[vk] = false;
                OnKeyEvent(static_cast<Windows::System::VirtualKey>(vk), false);
            }
        }
    }
    catch (Platform::Exception^) { }
}

#ifdef MOUSE_SUPPORT
void dosbox_uwpMain::OnPointerMove(float nx, float ny, float px, float py)
{
    if (!m_retroCore) return;

    m_pointerX = nx;
    m_pointerY = ny;

    int relX = (int)(px - m_lastPointerPX);
    int relY = (int)(py - m_lastPointerPY);
    m_lastPointerPX = px;
    m_lastPointerPY = py;

    m_retroCore->SetMouseMove(relX, relY);
    m_retroCore->SetPointer(nx, ny, m_pointerDown);
}

void dosbox_uwpMain::OnPointerDown(float nx, float ny, unsigned btn)
{
    if (!m_retroCore) return;

    m_pointerX = nx;
    m_pointerY = ny;
    m_lastPointerX = nx;
    m_lastPointerY = ny;
    m_pointerDown = true;

    m_retroCore->SetPointer(nx, ny, true);
    m_retroCore->SetMouseButton(btn, true);
}

void dosbox_uwpMain::OnPointerUp(unsigned btn)
{
    if (!m_retroCore) return;
    m_retroCore->SetMouseButton(btn, false);
}

void dosbox_uwpMain::OnPointerRelease()
{
    if (!m_retroCore) return;
    m_pointerDown = false;
    m_retroCore->SetPointer(m_pointerX, m_pointerY, false);
}

void dosbox_uwpMain::OnPointerWheel(int delta)
{
    if (!m_retroCore) return;
    m_retroCore->SetMouseWheel(delta);
}

void dosbox_uwpMain::SetMousePointerId(uint32_t id)
{
    m_mousePointerId = id;
}

void dosbox_uwpMain::PollMouseButtons()
{
    if (m_mousePointerId == 0 || !m_retroCore) return;
    try
    {
        auto pt = Windows::UI::Input::PointerPoint::GetCurrentPoint(m_mousePointerId);
        if (!pt) return;
        auto props = pt->Properties;
        m_retroCore->SetMouseButton(1, props->IsLeftButtonPressed);
        m_retroCore->SetMouseButton(2, props->IsRightButtonPressed);
        m_retroCore->SetMouseButton(3, props->IsMiddleButtonPressed);
    }
    catch (Platform::Exception^) { }
}
#endif

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
