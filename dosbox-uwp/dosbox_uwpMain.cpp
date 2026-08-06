#include "pch.h"
#include "dosbox_uwpMain.h"
#include "Content/SettingsManager.h"
#include "libretro.h"
#include "dosbox_pure_sta.h"
#include "Common\DirectXHelper.h"
#include <cmath>
#include <sstream>
#include <wincodec.h>
#include <wrl/client.h>
#include <windows.h>
#include <fileapifromapp.h>
#include <psapi.h>
#include <SDL.h>

#ifdef XB_INSPECTOR_ENABLED
#include <xray/inspector.hpp>
// File-scope variables expostas ao Lua REPL do XB-Inspector
struct PerfStats {
    double frame_ms, poll_ms, hud_ms, render_ms, total_ms;
    double target_fps;
    float fps;
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
static std::atomic<bool> s_paused{false};
#endif

using namespace dosbox_uwp;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Windows::System::Profile;
using namespace Windows::ApplicationModel::Core;
using namespace Concurrency;

// UWP-safe file existence + size check (replaces _wstat64 / _waccess which use CRT
// Win32 APIs blocked by Xbox sandbox even with broadFileSystemAccess).
static bool uwp_file_exists(const wchar_t* path, int64_t* outSize = nullptr)
{
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExFromAppW(path, GetFileExInfoStandard, &fad))
        return false;
    if (outSize)
    {
        LARGE_INTEGER li;
        li.HighPart = fad.nFileSizeHigh;
        li.LowPart = fad.nFileSizeLow;
        *outSize = li.QuadPart;
    }
    return true;
}

dosbox_uwpMain::dosbox_uwpMain(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
    , m_clearColor{ 0.0f, 0.0f, 0.0f, 1.0f }
    , m_hasController(false)
{
    QueryPerformanceFrequency(&m_qpcFreq);
    m_deviceResources->RegisterDeviceNotify(this);

    m_retroD3D11 = std::unique_ptr<RetroD3D11Renderer>(new RetroD3D11Renderer(m_deviceResources));
    m_retroD3D11->CreateDeviceDependentResources();

    m_retroScreen = std::unique_ptr<RetroScreenRenderer>(new RetroScreenRenderer(m_deviceResources));
    m_retroScreen->CreateDeviceDependentResources();

    m_retroCore = std::unique_ptr<RetroCore>(new RetroCore());

    m_xaudio2 = std::unique_ptr<XAudio2Output>(new XAudio2Output());
    if (m_xaudio2->Initialize())
    {
        RetroCore::SetAudioOutput(m_xaudio2.get());
        spdlog::info("[dosbox-uwp] XAudio2: initialized OK");
    }
    else
    {
        spdlog::error("[dosbox-uwp] XAudio2: FAILED to initialize");
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

    // Initialize settings manager (loads dosbox-pure-settings.json)
    {
        std::string settingsDir;
        auto family = AnalyticsInfo::VersionInfo->DeviceFamily;
        std::wstring fw(family->Data());
        if (fw == L"Windows.Xbox")
        {
            CreateDirectoryA("E:\\dosbox", NULL);
            settingsDir = "E:\\dosbox";
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
                settingsDir = dir + "\\dosbox-pure";
            }
        }
        if (!settingsDir.empty())
        {
            SettingsManager::Initialize(settingsDir + "\\dosbox-pure-settings.json");
            m_menu.RefreshMenuItems();
        }
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
        xb::Xray::start("dosbox-uwp");
        {
            auto family = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;
            std::wstring fw(family->Data());
            char buf[64];
            WideCharToMultiByte(CP_UTF8, 0, fw.c_str(), -1, buf, sizeof(buf), nullptr, nullptr);
            xb::Xray::set_device_family(buf);
        }
        xb::Xray::bind("fps", &s_debug_fps);
        xb::Xray::bind("target_fps", &s_debug_target_fps);
        xb::Xray::bind("frame_ms", &s_debug_frame_ms);
        xb::Xray::bind("poll_ms", &s_debug_poll_ms);
        xb::Xray::bind("hud_ms", &s_debug_hud_ms);
        xb::Xray::bind("render_ms", &s_debug_render_ms);
        xb::Xray::bind("total_ms", &s_debug_total_ms);
        xb::Xray::bind_string("rom_name", s_rom_name, sizeof(s_rom_name));

        // Grouped struct — demonstrates bind_struct API
        static const xb::struct_field perf_fields[] = {
            xb::field("frame_ms",     &PerfStats::frame_ms),
            xb::field("poll_ms",      &PerfStats::poll_ms),
            xb::field("hud_ms",       &PerfStats::hud_ms),
            xb::field("render_ms",    &PerfStats::render_ms),
            xb::field("total_ms",     &PerfStats::total_ms),
            xb::field("target_fps",   &PerfStats::target_fps),
            xb::field("fps",          &PerfStats::fps),
            xb::field("rom_name",     &PerfStats::rom_name),
        };
        xb::Xray::bind_struct("perf", &s_perf, perf_fields,
            sizeof(perf_fields) / sizeof(perf_fields[0]));
        xb::Xray::set_on_terminate([]() {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        });
        xb::Xray::set_on_pause([]() {
            s_paused = true;
            while (s_paused) {
                Sleep(100);
                xb::Xray::update();
            }
        });
        xb::Xray::set_on_continue([]() {
            s_paused = false;
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

    // Wire FrontendMenu callbacks
    m_menu.onOpenFile = [this]() { m_menu.m_fileBrowser.Open(); };
    m_menu.m_fileBrowser.onFileSelected = [this](const std::wstring& path) {
        // Close file browser FIRST to prevent double-enter from key-repeat
        m_menu.m_fileBrowser.Close();
        std::string pathUtf8(path.begin(), path.end());
        int64_t fileSize = -1;
        bool exists = uwp_file_exists(path.c_str(), &fileSize);
        spdlog::info("[FileBrowser] onFileSelected: '{}' exists={} size={}",
            pathUtf8, exists ? 1 : 0, exists ? fileSize : -1);
        ActivateLoadingScreen();
        QueueLoadRom(path, {}, path);
    };
    m_menu.m_fileBrowser.onFolderSelected = [this](const std::wstring& path) {
        std::string pathUtf8(path.begin(), path.end());
        spdlog::info("[StartupFolder] Set to: '{}'", pathUtf8.empty() ? "(cleared)" : pathUtf8);
        SettingsManager::SetOption("frontend_startup_folder", pathUtf8.c_str());
        m_menu.RefreshOverlayItems();
    };
    m_menu.onOpenPuremenu = [this]() {
        if (m_retroCore && m_retroCore->IsLoaded()) {
            m_menu.Hide();
            m_retroCore->ToggleOSD();
        }
    };
    m_menu.onFileSelectedHistory = [this](const std::wstring& path) {
        std::string pathUtf8(path.begin(), path.end());
        int64_t fileSize = -1;
        bool exists = uwp_file_exists(path.c_str(), &fileSize);
        spdlog::info("[History] Loading '{}' exists={} size={}",
            pathUtf8, exists ? 1 : 0, exists ? fileSize : -1);
        ActivateLoadingScreen();
        QueueLoadRom(path, {}, path);
    };
    m_menu.onExit = []() {
        CoreApplication::Exit();
    };
    m_menu.onBeep = [this]() {
        const int sampleRate = 48000;
        const int numFrames = (int)(sampleRate * 0.12f);
        std::vector<int16_t> samples((size_t)numFrames * 2);
        const int halfPeriod = sampleRate / (1000 * 2);
        for (int i = 0; i < numFrames; i++)
        {
            float t = (float)i / sampleRate;
            int16_t v = ((i % (halfPeriod * 2)) < halfPeriod) ? 8000 : -8000;
            float env = 1.0f;
            if (t < 0.002f) env = t / 0.002f;
            else if (t > 0.115f) env = (0.12f - t) / 0.005f;
            v = (int16_t)(v * env);
            samples[i * 2] = v;
            samples[i * 2 + 1] = v;
        }
        // Write() blocks only if the ring is full (4 x 12ms = 48ms), which the
        // 120ms beep never fills; voice auto-starts via Write if it was stopped.
        if (m_xaudio2)
            m_xaudio2->Write(samples.data(), numFrames);
    };
    m_menu.onOptionChanged = [this](const char* key, const char* value) {
        if (!strcmp(key, "frontend_vsync"))
        {
            bool enabled = !strcmp(value, "On");
            // Frame limiter overrides vsync: when 60/70Hz software timing is active,
            // force syncInterval=0 (non-blocking Present) and let software timer pace.
            std::string fl = SettingsManager::GetOption("frontend_framelimit", "off");
            if (fl == "60" || fl == "70") enabled = false;
            m_deviceResources->SetVSync(enabled);
            RetroCore::s_vsyncEnabled = enabled;
            spdlog::info("[Settings] VSync = {} (fl={}, syncInterval={})", value, fl.c_str(), m_deviceResources->GetSyncInterval());
        }
        else if (!strcmp(key, "frontend_framelimit"))
        {
            m_frameLimitFps = atoi(value);
            // When switching to software timing, force syncInterval=0
            bool useVsync = (m_frameLimitFps == 0);
            if (useVsync) {
                auto vsync = SettingsManager::GetOption("frontend_vsync", "Off");
                useVsync = vsync == "On";
            }
            m_deviceResources->SetVSync(useVsync);
            RetroCore::s_vsyncEnabled = useVsync;
            spdlog::info("[Settings] FrameLimiter={}fps, vsync={}", m_frameLimitFps, useVsync);
        }
        else if (!strcmp(key, "frontend_scaler"))
        {
            D2D1_BITMAP_INTERPOLATION_MODE mode = !strcmp(value, "Nearest")
                ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
            m_retroScreen->SetInterpolationMode(mode);
            spdlog::info("[Settings] Scaler = {} (d2dMode={})", value, (int)mode);
        }
    };

        // Apply frontend-only settings at startup
        // VSync OFF by default — software frame limiter or manual pacing.
        {
            auto vsync = SettingsManager::GetOption("frontend_vsync", "Off");
            bool vsyncEnabled = vsync.empty() || vsync == "On";
            auto fl = SettingsManager::GetOption("frontend_framelimit", "off");
            m_frameLimitFps = atoi(fl.c_str());
            // Frame limiter overrides vsync
            if (m_frameLimitFps > 0) vsyncEnabled = false;
            m_deviceResources->SetVSync(vsyncEnabled);
            RetroCore::s_vsyncEnabled = vsyncEnabled;
            RetroCore::s_displayRefreshRate = m_deviceResources->GetDisplayRefreshRate();
            spdlog::info("[Settings] VSync={}, frameLimit={}fps, displayRefreshRate={:.1f}Hz", vsyncEnabled, m_frameLimitFps, RetroCore::s_displayRefreshRate.load());
        auto scaler = SettingsManager::GetOption("frontend_scaler", "Bilinear");
        D2D1_BITMAP_INTERPOLATION_MODE mode = (scaler == "Nearest")
            ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
            : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
        m_retroScreen->SetInterpolationMode(mode);
    }

    BootCore();
}

dosbox_uwpMain::~dosbox_uwpMain()
{
#ifdef XB_INSPECTOR_ENABLED
    xb::Xray::stop();
#endif
    m_retroCore->Shutdown();
    m_deviceResources->RegisterDeviceNotify(nullptr);
    // DoPacingSleep removed — visual frame rate (Present) handles timing; audio pacing via DRC
}

void dosbox_uwpMain::BootCore()
{
    if (!m_retroCore->Init())
    {
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

    m_retroRunning = true;
}

void dosbox_uwpMain::LoadRom(const std::wstring& path, std::vector<uint8_t> romData, const std::wstring& originalPath)
{
    int _len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string pathUtf8(_len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &pathUtf8[0], _len, nullptr, nullptr);
    std::string origUtf8;
    if (!originalPath.empty() && originalPath != path) {
        int olen = WideCharToMultiByte(CP_UTF8, 0, originalPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        origUtf8.resize(olen - 1);
        WideCharToMultiByte(CP_UTF8, 0, originalPath.c_str(), -1, &origUtf8[0], olen, nullptr, nullptr);
    }
    bool pathExists = uwp_file_exists(path.c_str());
    if (!pathExists)
        spdlog::error("[LoadRom] path='{}' data={} bytes original='{}' PATH DOES NOT EXIST",
            pathUtf8, romData.size(), origUtf8.empty() ? "(same)" : origUtf8);
    else
        spdlog::info("[LoadRom] path='{}' data={} bytes original='{}'",
            pathUtf8, romData.size(), origUtf8.empty() ? "(same)" : origUtf8);

    if (!m_retroCore->IsInitialized())
    {
        if (!m_retroCore->Init())
        {
            spdlog::error("[LoadRom] retro_init FAILED");
            m_loadState = LOAD_FAILED;
            m_loadingActive = false;
            return;
        }
    }
    else
    {
        spdlog::info("[LoadRom] core already initialized, skipping retro_init");
    }

    // Keep requested paths so the async load-success handler can update
    // history / menu without capturing temporaries.
    m_lastRequestedPath = path;
    m_lastRequestedOrigPath = originalPath.empty() ? path : originalPath;

    // Extract filename for xray binding
    auto slash = pathUtf8.find_last_of("/\\");
    std::string fname = (slash != std::string::npos) ? pathUtf8.substr(slash + 1) : pathUtf8;
#ifdef XB_INSPECTOR_ENABLED
    strncpy_s(s_rom_name, fname.c_str(), sizeof(s_rom_name) - 1);
    s_rom_name[sizeof(s_rom_name) - 1] = '\0';
    strncpy_s(s_perf.rom_name, fname.c_str(), sizeof(s_perf.rom_name) - 1);
    s_perf.rom_name[sizeof(s_perf.rom_name) - 1] = '\0';
#endif

    // Async: the emulation thread runs retro_load_game. Completion is polled
    // in Update() via ConsumeLoadResult().
    m_retroCore->LoadGame(path, romData);
    m_loadState = LOAD_BOOTING;
    m_loadTimer = 0;
    spdlog::info("[LoadRom] game load enqueued (async on emulation thread)");
}

void dosbox_uwpMain::QueueLoadRom(const std::wstring& path, std::vector<uint8_t> romData, const std::wstring& originalPath)
{
    m_pendingLoad = std::make_unique<PendingLoad>();
    m_pendingLoad->path = path;
    m_pendingLoad->originalPath = originalPath;
    m_pendingLoad->data = std::move(romData);
}

void dosbox_uwpMain::CreateWindowSizeDependentResources()
{
}

void dosbox_uwpMain::Update()
{
    // Process queued load — set flag so Render() shows loading screen, defer actual load
    // Loading screen already activated in OpenFilePicker before I/O; this guards
    // edge cases like programmatic QueueLoadRom without picker path
    if (m_pendingLoad && !m_loadingActive)
        ActivateLoadingScreen();

    // FPS tracking — only update when frames were produced (not on skip iterations).
    // This prevents the FPS counter from showing 4000fps on Windows when Present(0,0) spins.
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (m_fpsLastFrame.QuadPart != 0 && m_lastRetroRuns > 0)
        {
            double dt = (double)(now.QuadPart - m_fpsLastFrame.QuadPart) * 1000.0 / m_qpcFreq.QuadPart;
            m_fpsFrameTimes[m_fpsFrameIdx] = dt;
            m_fpsFrameIdx = (m_fpsFrameIdx + 1) % 60;
            if (m_fpsFrameCount < 60) m_fpsFrameCount++;
            double sum = 0;
            for (int i = 0; i < m_fpsFrameCount; i++) sum += m_fpsFrameTimes[i];
            m_frameTimeMs = sum / m_fpsFrameCount;
            m_currentFps = (m_frameTimeMs > 0.0) ? 1000.0 / m_frameTimeMs : 0.0;
        }
        if (m_lastRetroRuns > 0)
            m_fpsLastFrame = now;
    }

#ifdef XB_INSPECTOR_ENABLED
    xb::Xray::update();
#endif

    m_timer.Tick([&]()
    {
        static int s_diagRunsAccum = 0;
        // 1Hz [HEALTH] interval state (see pacing stats block below). UI
        // thread only — the emulation thread (PaceFrame) is not touched.
        static LARGE_INTEGER s_healthLast = {};
        static double s_healthFrameSum = 0.0;
        static int s_healthFrameCount = 0;
        static double s_healthFrameMax = 0.0;
        static int s_healthSkips = 0;
        static long long s_healthLastConsumed = 0;
        static long s_healthLastUnder = 0;
        static long s_healthLastOver = 0;
        LARGE_INTEGER _t0, _t1, _t2, _t3, _freq;
        QueryPerformanceFrequency(&_freq);

        m_frameLate = false;
        m_sdlInput->PollEvents();
        QueryPerformanceCounter(&_t0);

        // Read gamepad analog stick always — used for splash cursor + in-game
        float sx = 0.0f, sy = 0.0f;
        m_sdlInput->GetLeftStick(sx, sy);
        const float GAMEPAD_DEADZONE = 0.15f;
        if (fabs(sx) < GAMEPAD_DEADZONE) sx = 0.0f;
        if (fabs(sy) < GAMEPAD_DEADZONE) sy = 0.0f;

        // Update frontend menu core-loaded state
        m_menu.SetCoreLoaded(m_retroCore && m_retroCore->IsLoaded());

        // Menu navigation (absorbs gamepad input while visible)
        if (m_menu.IsVisible())
        {
            // DPad auto-repeat: first press fires immediately, then repeats after delay
            const int DPAD_INITIAL_DELAY_MS = 300;
            const int DPAD_REPEAT_RATE_MS = 50;

            auto dpadRepeat = [&](int btn, bool up) {
                bool justPressed = m_sdlInput->WasButtonJustPressed(btn);
                bool held = m_sdlInput->IsButtonHeld(btn);
                ULONGLONG now = GetTickCount64();

                if (justPressed) {
                    // First press: fire immediately, start repeat timer
                    m_menu.OnDPad(up);
                    m_dpadRepeatBtn = btn;
                    m_dpadRepeatStart = now;
                    m_dpadRepeatNext = now + DPAD_INITIAL_DELAY_MS;
                    return;
                }
                if (held && m_dpadRepeatBtn == btn && now >= m_dpadRepeatNext) {
                    // Repeat: fire and schedule next
                    m_menu.OnDPad(up);
                    m_dpadRepeatNext = now + DPAD_REPEAT_RATE_MS;
                }
                if (!held && m_dpadRepeatBtn == btn) {
                    m_dpadRepeatBtn = -1;
                }
            };
            dpadRepeat(BUTTON_DPAD_UP, true);
            dpadRepeat(BUTTON_DPAD_DOWN, false);

            // DPad Left/Right: toggle option values
            auto dpadLeftRight = [&](int btn, bool left) {
                bool justPressed = m_sdlInput->WasButtonJustPressed(btn);
                bool held = m_sdlInput->IsButtonHeld(btn);
                ULONGLONG now = GetTickCount64();
                if (justPressed) {
                    if (left) m_menu.OnDPadLeft(); else m_menu.OnDPadRight();
                    m_dpadRepeatBtn = btn;
                    m_dpadRepeatStart = now;
                    m_dpadRepeatNext = now + DPAD_INITIAL_DELAY_MS;
                    return;
                }
                if (held && m_dpadRepeatBtn == btn && now >= m_dpadRepeatNext) {
                    if (left) m_menu.OnDPadLeft(); else m_menu.OnDPadRight();
                    m_dpadRepeatNext = now + DPAD_REPEAT_RATE_MS;
                }
                if (!held && m_dpadRepeatBtn == btn) {
                    m_dpadRepeatBtn = -1;
                }
            };
            dpadLeftRight(BUTTON_DPAD_LEFT, true);
            dpadLeftRight(BUTTON_DPAD_RIGHT, false);

            if (m_sdlInput->WasButtonJustPressed(BUTTON_A))
                m_menu.OnConfirm();
            if (m_sdlInput->WasButtonJustPressed(BUTTON_START))
                m_menu.OnConfirm();
            if (m_sdlInput->WasButtonJustPressed(BUTTON_B))
                m_menu.OnBack();
            if (m_sdlInput->WasButtonJustPressed(BUTTON_L))
                m_menu.OnPageUp();
            if (m_sdlInput->WasButtonJustPressed(BUTTON_R))
                m_menu.OnPageDown();
        }
        else
        {
            // Reset repeat state when menu is hidden
            m_dpadRepeatBtn = -1;
        }

        // R3 -> PUREMENU toggle (after menu nav so WasButtonJustPressed not consumed)
        if (m_sdlInput->WasButtonJustPressed(BUTTON_R3) && m_retroCore && m_retroCore->IsLoaded()) {
            spdlog::info("[input] R3 -> toggle PUREMENU");
            m_retroCore->ToggleOSD();
        }

        // LB+RB+Select simultaneous press → toggle gamepad mouse mode
        // Mouse mode OFF (default): stick is game analog only (no mouse sim in DOS)
        // Mouse mode ON: stick → relative mouse in-game, A→Enter, B→Escape for Puremenu
        // Note: PUREMENU/FrontendMenu cursor is ALWAYS driven by the L-analog (see
        // the pointerContext block below) — the toggle only affects in-game mouse.
        {
            bool lb = m_sdlInput->IsButtonHeld(BUTTON_L);
            bool rb = m_sdlInput->IsButtonHeld(BUTTON_R);
            bool sel = m_sdlInput->IsButtonHeld(BUTTON_SELECT);
            bool allThree = lb && rb && sel;
            if (allThree && !m_lbrbsPrevHeld && m_retroCore && m_retroCore->IsLoaded()) {
                m_gamepadMouseMode = !m_gamepadMouseMode;
                spdlog::info("[input] Gamepad mouse mode: {}", m_gamepadMouseMode ? "ON" : "OFF");
            }
            m_lbrbsPrevHeld = allThree;
        }

#ifdef INPUT_DEBUG_ENABLED
        // Log every gamepad button press (Debug-only; too noisy for Release)
        struct { int btn; const char* name; } btns[] = {
            { BUTTON_A, "A" }, { BUTTON_B, "B" }, { BUTTON_X, "X" }, { BUTTON_Y, "Y" },
            { BUTTON_L, "L" }, { BUTTON_R, "R" }, { BUTTON_L2, "L2" }, { BUTTON_R2, "R2" },
            { BUTTON_START, "START" }, { BUTTON_SELECT, "SELECT" },
            { BUTTON_L3, "L3" }, { BUTTON_R3, "R3" },
            { BUTTON_DPAD_UP, "DPAD_UP" }, { BUTTON_DPAD_DOWN, "DPAD_DOWN" },
            { BUTTON_DPAD_LEFT, "DPAD_LEFT" }, { BUTTON_DPAD_RIGHT, "DPAD_RIGHT" },
        };
        for (auto& b : btns)
            if (m_sdlInput->WasButtonJustPressed(b.btn))
                spdlog::info("[input] {} pressed", b.name);
#endif

#ifdef MOUSE_SUPPORT
        // Splash screen: gamepad moves D2D cursor directly (no menu, no game)
        float dtSec = (float)m_timer.GetElapsedSeconds();
        if (!m_retroCore->IsLoaded() && !m_menu.IsVisible())
        {
            m_pointerX += sx * 1.34f * dtSec;
            m_pointerY += sy * 1.34f * dtSec;
            if (m_pointerX < 0.0f) m_pointerX = 0.0f;
            if (m_pointerX > 1.0f) m_pointerX = 1.0f;
            if (m_pointerY < 0.0f) m_pointerY = 0.0f;
            if (m_pointerY > 1.0f) m_pointerY = 1.0f;
        }
#endif

        // Gamepad → RetroPad (GENERICKEYBOARD preset) for core to translate to DOS keys
        if (!m_menu.IsVisible() && m_retroRunning && m_retroCore->IsLoaded())
        {
            PollMouseButtons();
            PollKeyboard();
            for (unsigned i = 0; i < 16; i++)
                RetroCore::SetJoypadButton(i, false);

            // Generic keyboard preset: gamepad buttons → RetroPad IDs
            struct { int btn; unsigned retroId; } padMap[] = {
                { BUTTON_DPAD_UP,    RETRO_DEVICE_ID_JOYPAD_UP },
                { BUTTON_DPAD_DOWN,  RETRO_DEVICE_ID_JOYPAD_DOWN },
                { BUTTON_DPAD_LEFT,  RETRO_DEVICE_ID_JOYPAD_LEFT },
                { BUTTON_DPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
                { BUTTON_A,          RETRO_DEVICE_ID_JOYPAD_A },
                { BUTTON_B,          RETRO_DEVICE_ID_JOYPAD_B },
                { BUTTON_X,          RETRO_DEVICE_ID_JOYPAD_X },
                { BUTTON_Y,          RETRO_DEVICE_ID_JOYPAD_Y },
                { BUTTON_SELECT,     RETRO_DEVICE_ID_JOYPAD_SELECT },
                { BUTTON_START,      RETRO_DEVICE_ID_JOYPAD_START },
                { BUTTON_L,          RETRO_DEVICE_ID_JOYPAD_L },
                { BUTTON_R,          RETRO_DEVICE_ID_JOYPAD_R },
                { BUTTON_L2,         RETRO_DEVICE_ID_JOYPAD_L2 },
                { BUTTON_R2,         RETRO_DEVICE_ID_JOYPAD_R2 },
                { BUTTON_L3,         RETRO_DEVICE_ID_JOYPAD_L3 },
                { BUTTON_R3,         RETRO_DEVICE_ID_JOYPAD_R3 },
            };
            for (auto& m : padMap)
                RetroCore::SetJoypadButton(m.retroId, m_sdlInput->IsButtonHeld(m.btn));

            // OSD swap: when PUREMENU/OSK/Mapper is active, swap A↔B so A=confirm, B=back
            if (RetroCore::IsOSDActive())
            {
                bool aHeld = m_sdlInput->IsButtonHeld(BUTTON_A);
                bool bHeld = m_sdlInput->IsButtonHeld(BUTTON_B);
                RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_A, bHeld);
                RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_B, aHeld);
                m_osdExitSuppressing = true; // suppress after OSD closes
            }
            else if (m_osdExitSuppressing)
            {
                RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_A, false);
                RetroCore::SetJoypadButton(RETRO_DEVICE_ID_JOYPAD_B, false);
                // Only stop suppressing once physical A AND B are both released
                if (!m_sdlInput->IsButtonHeld(BUTTON_A) && !m_sdlInput->IsButtonHeld(BUTTON_B))
                    m_osdExitSuppressing = false;
            }

        }

#ifdef MOUSE_SUPPORT
        // L-analog drives pointer whenever a pointer-capable UI is on screen:
        //  - FrontendMenu GUI  -> moves D2D cursor (m_pointerX/Y), drives hover selection
        //  - PUREMENU (OSD)    -> moves absolute pointer (DBPS_GetMouse)
        //  - In-game           -> only when gamepad mouse mode is ON (LB+RB+Select)
        {
            bool guiVisible = m_menu.IsVisible();
            bool osdActive = RetroCore::IsOSDActive();
            bool pointerContext = guiVisible || osdActive || m_gamepadMouseMode;
            if (pointerContext)
            {
                // Relative mouse (DOS games only, mouse mode ON)
                if (!guiVisible && !osdActive && m_gamepadMouseMode && (sx != 0.0f || sy != 0.0f))
                {
                    float curveX = (sx > 0 ? 1.0f : -1.0f) * sx * sx;
                    float curveY = (sy > 0 ? 1.0f : -1.0f) * sy * sy;
                    float inPixelsPerSec = 800.0f;
                    int dx = (int)(curveX * inPixelsPerSec * dtSec);
                    int dy = (int)(curveY * inPixelsPerSec * dtSec);
                    if (dx == 0 && sx != 0) dx = (sx > 0 ? 1 : -1);
                    if (dy == 0 && sy != 0) dy = (sy > 0 ? 1 : -1);
                    m_retroCore->SetMouseMove(dx, dy);
                }

                // Gamepad buttons → PUREMENU keyboard (A=Enter, B=Escape)
                // Only when OSD visible and menu NOT visible — prevents spurious input in DOS
                if (osdActive && !guiVisible && m_gamepadMouseMode)
                {
                    static bool prevA = false, prevB = false;
                    bool nowA = m_sdlInput->IsButtonHeld(BUTTON_A);
                    bool nowB = m_sdlInput->IsButtonHeld(BUTTON_B);
                    if (nowA != prevA) { RetroCore::SetKeyState(RETROK_RETURN, nowA); prevA = nowA; }
                    if (nowB != prevB) { RetroCore::SetKeyState(RETROK_ESCAPE, nowB); prevB = nowB; }
                }

                // Absolute pointer: PUREMENU cursor (OSD) or FrontendMenu hover (GUI)
                LARGE_INTEGER _gmnow;
                QueryPerformanceCounter(&_gmnow);
                double mouseIdleMs = 0.0;
                if (m_lastPointerTime.QuadPart != 0)
                    mouseIdleMs = (double)(_gmnow.QuadPart - m_lastPointerTime.QuadPart)
                                  * 1000.0 / m_qpcFreq.QuadPart;
                if (mouseIdleMs > 500.0 || m_lastPointerTime.QuadPart == 0)
                {
                    float cursorDx = sx * 0.80f * dtSec;
                    float cursorDy = sy * 0.80f * dtSec;
                    m_virtualCursorX += cursorDx;
                    m_virtualCursorY += cursorDy;
                    if (m_virtualCursorX < 0.0f) m_virtualCursorX = 0.0f;
                    if (m_virtualCursorX > 1.0f) m_virtualCursorX = 1.0f;
                    if (m_virtualCursorY < 0.0f) m_virtualCursorY = 0.0f;
                    if (m_virtualCursorY > 1.0f) m_virtualCursorY = 1.0f;
                    if (guiVisible)
                    {
                        m_pointerX = m_virtualCursorX;
                        m_pointerY = m_virtualCursorY;
                        auto logicalSize = m_deviceResources->GetLogicalSize();
                        m_menu.HandlePointerMove(m_pointerX * logicalSize.Width, m_pointerY * logicalSize.Height);
                    }
                    else if (osdActive)
                    {
                        m_retroCore->SetPointer(m_virtualCursorX, m_virtualCursorY, false);
                    }
                }
            } // end pointer context
        }
#endif

        // Emulation runs on its own thread (audio-paced). The UI tick only
        // tracks whether a new frame was produced (FPS/DIAG/SKIP reporting).
        {
            uint64_t nowCount = RetroCore::GetEmulatedFrameCount();
            m_lastRetroRuns = (nowCount > m_lastEmuFrameCount) ? 1 : 0;
            m_lastEmuFrameCount = nowCount;
            s_diagRunsAccum += m_lastRetroRuns;
        }

        // Core requested SHUTDOWN → the emulation thread already unloaded the
        // game (RetroCore handles it internally). Do the UI-side cleanup here.
        if (RetroCore::ConsumeUnloadEvent())
        {
            OutputDebugStringA("[dosbox-uwp] Emulation thread unloaded game (core SHUTDOWN)\n");
            m_retroRunning = false;
            m_lastRetroRuns = 0;
            m_clearColor = DirectX::Colors::Black;

            // Reset FPS tracking — stale data from old game leaks into overlay
            m_currentFps = 0.0;
            m_frameTimeMs = 0.0;
            m_fpsFrameCount = 0;
            m_fpsFrameIdx = 0;
            memset(m_fpsFrameTimes, 0, sizeof(m_fpsFrameTimes));
            m_fpsLastFrame = {};

            // Reset input state — prevent carry-over to next game
            m_gamepadMouseMode = false;
            m_lbrbsPrevHeld = false;
            m_dpadRepeatBtn = -1;
            m_osdExitSuppressing = false;

            // Flush XAudio2 — old audio queue would play briefly with new game
            if (m_xaudio2) {
                m_xaudio2->Stop();
                m_xaudio2->Flush();
            }

            m_loadState = LOAD_IDLE;
            m_menu.Show();
        }

        // Async load completion: the emulation thread ran retro_load_game.
        if (m_loadState == LOAD_BOOTING)
        {
            int result = RetroCore::ConsumeLoadResult();
            if (result == 1)
            {
                spdlog::info("[LoadRom] Game loaded OK (emulation thread)");
                m_retroRunning = true;
                m_clearColor = DirectX::Colors::Black;
                m_menu.Hide();

                // Push all saved option values to core so check_variables() picks them up
                SettingsManager::ForEachOption([](const char* key, const char* value) {
                    if (strncmp(key, "frontend_", 9) != 0) // skip frontend-only options
                        RetroCore::SetOptionValue(key, value);
                });

                // Add to history — store original path, not temp path
                {
                    const std::wstring& histPath = m_lastRequestedOrigPath;
                    std::string pathUtf8;
                    {
                        int len = WideCharToMultiByte(CP_UTF8, 0, histPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (len > 0)
                        {
                            pathUtf8.resize(len - 1);
                            WideCharToMultiByte(CP_UTF8, 0, histPath.c_str(), -1, &pathUtf8[0], len, nullptr, nullptr);
                        }
                    }
                    if (!pathUtf8.empty())
                    {
                        auto slash = pathUtf8.find_last_of("/\\");
                        std::string fnameUtf8 = (slash != std::string::npos) ? pathUtf8.substr(slash + 1) : pathUtf8;
                        SettingsManager::AddToHistory(fnameUtf8, pathUtf8);
                    }
                    m_menu.SetCoreLoaded(true);
                }

                m_loadState = LOAD_DONE;

                // Flush the old game's audio ring; the emulation thread's next
                // Write() auto-starts the voice.
                if (m_xaudio2)
                    m_xaudio2->Flush();
                m_loadingActive = false;
                m_loadingDisc.Reset();
            }
            else if (result == 0)
            {
                spdlog::error("[LoadRom] retro_load_game FAILED (emulation thread)");
                m_loadState = LOAD_FAILED;
                m_loadingActive = false;
                m_loadingDisc.Reset();
            }
        }

        // Auto-stop XAudio2 voice: only when NO game is loaded (beep finished,
        // before a load). With a game loaded the blocking audio Write() is the
        // frame pacer — stopping the voice here would freeze OnBufferEnd and
        // make every SubmitFullBuffer wait the full WRITE_TIMEOUT.
        if (m_xaudio2 && !(m_retroCore && m_retroCore->IsLoaded()) &&
            m_lastRetroRuns == 0 && m_xaudio2->GetQueuedFrames() <= 0)
        {
            m_xaudio2->Stop();
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
#endif
            // Per-tick accumulation for the 1Hz [HEALTH] log.
            s_healthFrameSum += frameMs;
            s_healthFrameCount++;
            if (frameMs > s_healthFrameMax) s_healthFrameMax = frameMs;

            // 1Hz gate via QPC (the old tick-counter gate fired only every
            // ~50s on Xbox at 60 UI fps — useless as a live health log).
            LARGE_INTEGER _hnow;
            QueryPerformanceCounter(&_hnow);
            double healthElapsed = (s_healthLast.QuadPart != 0)
                ? (double)(_hnow.QuadPart - s_healthLast.QuadPart) * 1000.0 / _freq.QuadPart
                : 0.0;

            if (s_healthLast.QuadPart == 0 || healthElapsed >= 1000.0)
            {
                bool loaded = m_retroCore && m_retroCore->IsLoaded();
                // [HEALTH] + overlay both gated by frontend_diag (Settings >
                // General > Debug Overlay). Off by default to keep the log
                // clean outside of debugging sessions.
                bool diagHealth = SettingsManager::GetOption("frontend_diag", "Off") == "On";
                if (s_healthLast.QuadPart != 0 && loaded && diagHealth)
                {
                    int qf = m_xaudio2 ? m_xaudio2->GetQueuedFrames() : 0;
                    int buffers = m_xaudio2 ? m_xaudio2->GetBuffers() : 0;
                    unsigned vw = m_retroCore ? m_retroCore->GetFrameWidth() : 0;
                    unsigned vh = m_retroCore ? m_retroCore->GetFrameHeight() : 0;

                    long long consumedNow = m_xaudio2 ? m_xaudio2->GetTotalConsumed() : 0;
                    long underNow = m_xaudio2 ? m_xaudio2->GetUnderruns() : 0;
                    long overNow = m_xaudio2 ? m_xaudio2->GetOverruns() : 0;
                    long long consDelta = (consumedNow >= s_healthLastConsumed)
                        ? (consumedNow - s_healthLastConsumed) : 0;
                    double consPerSec = consDelta * 1000.0 / healthElapsed;
                    long underDelta = (underNow >= s_healthLastUnder)
                        ? (underNow - s_healthLastUnder) : 0;
                    long overDelta = (overNow >= s_healthLastOver)
                        ? (overNow - s_healthLastOver) : 0;

                    double frameAvg = s_healthFrameCount
                        ? s_healthFrameSum / s_healthFrameCount : 0.0;

                    // Real (Windows) process CPU time
                    FILETIME creation, exit, kernel, user;
                    GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
                    ULARGE_INTEGER uk, uu;
                    uk.LowPart = kernel.dwLowDateTime; uk.HighPart = kernel.dwHighDateTime;
                    uu.LowPart = user.dwLowDateTime; uu.HighPart = user.dwHighDateTime;
                    double cpuMs = (double)(uk.QuadPart + uu.QuadPart) / 10000.0;

                    // Real memory
                    PROCESS_MEMORY_COUNTERS pmc;
                    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
                    float memMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);

                    spdlog::info(
                        "[HEALTH] ui={:.1f} runs={} target={:.0f} "
                        "frame={:.2f}ms(max{:.1f}) "
                        "audio_q={}ms({}/{}buf) under={}/s over={}/s cons={:.0f}/s "
                        "skip={} cpu={:.0f}ms mem={:.1f}MB "
                        "cycles={} cpu_core={} video={}x{}",
                        m_currentFps, s_diagRunsAccum, targetFps,
                        frameAvg, s_healthFrameMax,
                        qf, buffers, XAudio2Output::MAX_BUFFERS,
                        underDelta, overDelta, consPerSec,
                        s_healthSkips, cpuMs, memMB,
                        RetroCore::GetCyclesMax(),
                        RetroCore::GetCpuDecoderName(),
                        vw, vh);
                }

                // Reset interval state (also refreshes the audio baselines so
                // the first in-game sample after the menu is not stale).
                s_healthLast = _hnow;
                s_healthLastConsumed = m_xaudio2 ? m_xaudio2->GetTotalConsumed() : 0;
                s_healthLastUnder = m_xaudio2 ? m_xaudio2->GetUnderruns() : 0;
                s_healthLastOver = m_xaudio2 ? m_xaudio2->GetOverruns() : 0;
                s_healthFrameSum = 0.0;
                s_healthFrameCount = 0;
                s_healthFrameMax = 0.0;
                s_healthSkips = 0;
                s_diagRunsAccum = 0;

                if (diagHealth)
                {
                    // Frame drop detection: sustained drop below 80% of target
                    static double s_lowFpsSum = 0.0;
                    static int s_lowFpsCount = 0;
                    static int s_dropLogCooldown = 0;
                    if (loaded && m_currentFps > 0 && m_currentFps < targetFps * 0.80)
                    {
                        s_lowFpsSum += m_currentFps;
                        s_lowFpsCount++;
                    }
                    else
                    {
                        s_lowFpsSum = 0;
                        s_lowFpsCount = 0;
                    }
                    if (s_lowFpsCount >= 5 && s_dropLogCooldown <= 0)
                    {
                        double avgLow = s_lowFpsSum / s_lowFpsCount;
                        spdlog::warn(
                            "[DROP] {} consecutive seconds below 80% target: "
                            "avg={:.1f} target={:.0f} — possible frame drop or stall",
                            s_lowFpsCount, avgLow, targetFps);
                        s_dropLogCooldown = 300; // don't spam
                    }
                    if (s_dropLogCooldown > 0) s_dropLogCooldown--;

                    // Emulation stall: sustained sub-30fps while a game is loaded
                    if (loaded && m_currentFps > 0 && m_currentFps < 30.0)
                    {
                        spdlog::warn("[CPU] emulation slow: {:.0f}fps — CPU/IO spike?",
                            m_currentFps);
                    }
                }
            }
        }

        {
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
        }
        QueryPerformanceCounter(&_t2);

        // loading screen animation (time-based, not frame-based)
        if (m_loadingActive)
        {
            LARGE_INTEGER _animNow;
            QueryPerformanceCounter(&_animNow);
            double animElapsed = (double)(_animNow.QuadPart - m_loadingStart.QuadPart)
                                 * 1000.0 / m_qpcFreq.QuadPart;
            m_loadingAngle = fmod((float)(animElapsed * 0.36f), 360.0f);
            m_loadingDots = ((int)(animElapsed / 200.0)) % 4;
        }

        QueryPerformanceCounter(&_t3);
#ifdef XB_INSPECTOR_ENABLED
        s_debug_poll_ms = (double)(_t0.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_hud_ms = (double)(_t2.QuadPart - _t1.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_render_ms = (double)(_t3.QuadPart - _t2.QuadPart) * 1000.0 / _freq.QuadPart;
        s_debug_total_ms = (double)(_t3.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
        s_perf.poll_ms = s_debug_poll_ms;
        s_perf.hud_ms = s_debug_hud_ms;
        s_perf.render_ms = s_debug_render_ms;
        s_perf.total_ms = s_debug_total_ms;
#endif
        {
            static unsigned _tc = 0;
            _tc++;
            // Micro-skip detection: log any individual tick where total exceeds 40ms
            // (~3 frame periods at 70fps). Catches the 50ms skip every ~10s on Xbox.
            double tickMs = (double)(_t3.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
            double frameMs = (double)(_t1.QuadPart - _t0.QuadPart) * 1000.0 / _freq.QuadPart;
            double hudMs = (double)(_t2.QuadPart - _t1.QuadPart) * 1000.0 / _freq.QuadPart;
            double sceneMs = (double)(_t3.QuadPart - _t2.QuadPart) * 1000.0 / _freq.QuadPart;
            if (tickMs > 20.0 && m_lastRetroRuns > 0 && !m_menu.IsVisible())
            {
                s_healthSkips++;
                int qf2 = m_xaudio2 ? m_xaudio2->GetQueuedFrames() : 0;
                spdlog::warn("[SKIP] tick={} total={:.1f}ms frame={:.1f} hud={:.1f} scene={:.1f} "
                    "runs={} q={}",
                    _tc, tickMs, frameMs, hudMs, sceneMs, m_lastRetroRuns, qf2);
            }
            if ((_tc % 6000) == 0)
            {
                double total_ms = tickMs;
                float fps       = m_timer.GetFramesPerSecond();
                unsigned long long memBytes = 0;
                try { memBytes = Windows::System::MemoryManager::AppMemoryUsage; } catch (...) { }
                char _dbg[512];
                sprintf_s(_dbg, "[dosbox-uwp] TICK #%u: frame=%.1fms hud=%.1f scene=%.1f fps=%.0f "
                    "MEM=%lluMB total=%.1f\n",
                    _tc, frameMs, hudMs, sceneMs, fps,
                    memBytes / (1024 * 1024), total_ms);
                OutputDebugStringA(_dbg);
            }
        }
    });
}

bool dosbox_uwpMain::Render()
{
    if (m_timer.GetFrameCount() == 0)
        return false;

    auto context = m_deviceResources->GetD3DDeviceContext();
    ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
    context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());
    context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), m_clearColor);
    context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (m_retroCore && m_retroCore->IsLoaded())
    {
        RetroCore::FrameView frame;
        if (RetroCore::AcquireFrame(frame))
        {
            m_retroD3D11->UpdateVideoFrame(
                frame.data, frame.w, frame.h, frame.pitch);
            RetroCore::ReleaseFrame();
        }
        m_retroD3D11->Render();
    }

    // D2D rendering: single BeginDraw/EndDraw for all overlays
    auto d2dContext = m_deviceResources->GetD2DDeviceContext();
    auto logicalSize = m_deviceResources->GetLogicalSize();
    auto dwrite = m_deviceResources->GetDWriteFactory();
    d2dContext->BeginDraw();

    // Loading screen (full black, spinning disc + Loading...)
    if (m_loadingActive)
    {
        RenderLoadingScreen(d2dContext, dwrite, D2D1::SizeF(logicalSize.Width, logicalSize.Height));
    }
    else
    {
        // Full-screen FrontendMenu (DOS style)
        if (m_menu.IsVisible())
        {
            m_menu.RenderFullScreen(d2dContext, dwrite, logicalSize.Width, logicalSize.Height);
        }

#ifdef MOUSE_SUPPORT
        // Draw simple cursor overlay when menu visible or on splash screen
        if (m_menu.IsVisible() || !m_retroCore->IsLoaded())
    {
        if (!m_cursorBrush)
        {
            d2dContext->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White, 0.85f), &m_cursorBrush);
        }

        float cx = m_pointerX * logicalSize.Width;
        float cy = m_pointerY * logicalSize.Height;
        const float CS = 10.0f;

        d2dContext->DrawLine(
            D2D1::Point2F(cx - CS, cy), D2D1::Point2F(cx + CS, cy),
            m_cursorBrush.Get(), 1.5f);
        d2dContext->DrawLine(
            D2D1::Point2F(cx, cy - CS), D2D1::Point2F(cx, cy + CS),
            m_cursorBrush.Get(), 1.5f);
    }
#endif
        // Debug overlay (top-right corner) — matches Unleashed ZILLALOG HUD
        // Gated by frontend_diag setting (Settings > General > Debug Overlay)
        if (m_currentFps > 0.0 && SettingsManager::GetOption("frontend_diag", "Off") != "Off")
        {
            int qf = m_xaudio2 ? m_xaudio2->GetQueuedFrames() : 0;
            unsigned vw = m_retroCore ? m_retroCore->GetFrameWidth() : 0;
            unsigned vh = m_retroCore ? m_retroCore->GetFrameHeight() : 0;
            int cyclesMax = m_retroCore ? RetroCore::GetCyclesMax() : 0;
            wchar_t fpsText[512];
            swprintf_s(fpsText,
                L"FPS: %5.1f  Frame: %5.1fms\n"
                L"Video: %ux%u  Viewport: %.0fx%.0f\n"
                L"Runs: %d  Target: %.0f\n"
                L"Audio Q: %d frames\n"
                L"CyclesMax: %d",
                m_currentFps, m_frameTimeMs,
                vw, vh, logicalSize.Width, logicalSize.Height,
                m_lastRetroRuns,
                m_retroCore ? m_retroCore->GetTargetFps() : 60.0,
                qf, cyclesMax);

            // Create text format (cached)
            static Microsoft::WRL::ComPtr<IDWriteTextFormat> s_fpsFormat;
            if (!s_fpsFormat)
            {
                dwrite->CreateTextFormat(L"Consolas", nullptr,
                    DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                    DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us",
                    &s_fpsFormat);
                if (s_fpsFormat) s_fpsFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            }

            if (s_fpsFormat)
            {
                static Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> s_fpsBrush;
                if (!s_fpsBrush)
                    d2dContext->CreateSolidColorBrush(
                        D2D1::ColorF(D2D1::ColorF::Yellow, 0.9f), &s_fpsBrush);

                if (s_fpsBrush)
                {
                    D2D1_RECT_F layout = {
                        logicalSize.Width - 400.0f, 8.0f,
                        logicalSize.Width - 12.0f, 100.0f
                    };
                    d2dContext->DrawText(fpsText, (UINT32)wcslen(fpsText),
                        s_fpsFormat.Get(), &layout, s_fpsBrush.Get());
                }
            }
        }

        } // end else (not loading)

    HRESULT hr = d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        m_cursorBrush.Reset();
    }

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

    // Route keys to FrontendMenu when visible
    if (m_menu.IsVisible())
    {
        if (down)
        {
            switch ((int)key)
            {
            case 0x26: m_menu.OnDPad(true); return;  // Up
            case 0x28: m_menu.OnDPad(false); return; // Down
            case 0x25: m_menu.OnDPadLeft(); return;  // Left
            case 0x27: m_menu.OnDPadRight(); return; // Right
            case 0x0D: m_menu.OnConfirm(); return;   // Enter
            case 0x1B: m_menu.OnBack();  return;     // Escape
            case 0x21: m_menu.OnPageUp(); return;    // PageUp
            case 0x22: m_menu.OnPageDown(); return;  // PageDown
            case 0x45: m_menu.OnEasterEgg(); return;  // E = next quote
            }
        }
        return; // absorb all keyboard while menu is visible
    }

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
#ifdef DEBUG_KEYBOARD
            char buf[128];
            sprintf_s(buf, "[dosbox-uwp] Key: VK=0x%02X down=%d retroKey=%u\n", vk, down, retroKey);
            OutputDebugStringA(buf);
#endif
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

    // Route to FrontendMenu when visible
    if (m_menu.IsVisible())
    {
        auto logicalSize = m_deviceResources->GetLogicalSize();
        m_menu.HandlePointerMove(nx * logicalSize.Width, ny * logicalSize.Height);
        return;
    }

    int relX = (int)(px - m_lastPointerPX);
    int relY = (int)(py - m_lastPointerPY);
    m_lastPointerPX = px;
    m_lastPointerPY = py;

    QueryPerformanceCounter(&m_lastPointerTime);

    m_retroCore->SetMouseMove(relX, relY);
    m_retroCore->SetPointer(nx, ny, m_pointerDown);
}

void dosbox_uwpMain::OnPointerDown(float nx, float ny, unsigned btn)
{
    if (!m_retroCore) return;

    // Route to FrontendMenu when visible
    if (m_menu.IsVisible())
    {
        auto logicalSize = m_deviceResources->GetLogicalSize();
        m_menu.HandlePointerDown(nx * logicalSize.Width, ny * logicalSize.Height, btn);
        return;
    }

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
    if (m_menu.IsVisible())
    {
        m_menu.HandlePointerWheel(delta);
        return;
    }
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
    m_retroScreen->ReleaseDeviceDependentResources();
    m_loadingDisc.Reset();
}

void dosbox_uwpMain::EnsureLoadingDisc()
{
    if (m_loadingDisc) return;
    auto d2d = m_deviceResources->GetD2DDeviceContext();
    if (!d2d) return;

    Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wic));
    if (FAILED(hr)) return;

    wchar_t imgPath[MAX_PATH];
    // Path 1: InstalledLocation (AppX)
    auto installPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
    wcscpy_s(imgPath, installPath->Data());
    size_t plen = wcslen(imgPath);
    if (plen + 30 >= MAX_PATH) return;
    if (imgPath[plen - 1] != L'\\') { imgPath[plen] = L'\\'; plen++; }
    wcscpy_s(imgPath + plen, MAX_PATH - plen, L"Assets\\disc.png");

    for (int attempt = 0; attempt < 2; attempt++)
    {
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        hr = wic->CreateDecoderFromFilename(imgPath, nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder);
        if (SUCCEEDED(hr))
        {
            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(0, &frame);
            if (SUCCEEDED(hr))
            {
                Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
                hr = wic->CreateFormatConverter(&conv);
                if (SUCCEEDED(hr))
                {
                    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
                    if (FAILED(hr))
                        hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                            WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
                    if (SUCCEEDED(hr))
                    {
                        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bmp;
                        hr = d2d->CreateBitmapFromWicBitmap(conv.Get(), &bmp);
                        if (SUCCEEDED(hr) && bmp)
                        {
                            m_loadingDisc = bmp.Get();
                            spdlog::info("Disc loaded");
                            return;
                        }
                    }
                }
            }
        }
        // Path 2: parent of InstalledLocation (dev build)
        if (attempt == 0)
        {
            wchar_t* ls = wcsrchr(imgPath, L'\\');
            if (!ls) break;
            *ls = L'\0';
            ls = wcsrchr(imgPath, L'\\');
            if (!ls) break;
            *ls = L'\0';
            plen = wcslen(imgPath);
            if (plen + 30 >= MAX_PATH) break;
            if (imgPath[plen - 1] != L'\\') { imgPath[plen] = L'\\'; plen++; }
            wcscpy_s(imgPath + plen, MAX_PATH - plen, L"Assets\\disc.png");
        }
    }
    spdlog::warn("Disc: failed to load disc.png");
}

void dosbox_uwpMain::OnDeviceRestored()
{
    m_retroScreen->CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
    // disc loaded lazily in RenderLoadingScreen via EnsureLoadingDisc()
}

void dosbox_uwpMain::ProcessPendingLoad()
{
    if (!m_pendingLoad) return;
    // Debug delay: 2 seconds so the loading screen is visible for testing
    LARGE_INTEGER _now;
    QueryPerformanceCounter(&_now);
    double elapsedMs = (double)(_now.QuadPart - m_loadingStart.QuadPart) * 1000.0 / m_qpcFreq.QuadPart;
    if (elapsedMs < 2000.0) return;
    LoadRom(m_pendingLoad->path, std::move(m_pendingLoad->data), m_pendingLoad->originalPath);
    m_pendingLoad.reset();
    // Completion handled asynchronously in Update() via ConsumeLoadResult().
    // Loading screen stays up until the emulation thread reports success/failure.
}

void dosbox_uwpMain::PauseEmulation()
{
    if (m_retroCore)
        m_retroCore->Pause();
    spdlog::info("[dosbox-uwp] Emulation paused");
}

void dosbox_uwpMain::ResumeEmulation()
{
    if (m_retroCore)
        m_retroCore->Resume();
    spdlog::info("[dosbox-uwp] Emulation resumed");
}

void dosbox_uwpMain::ShutdownNow()
{
    if (m_retroCore)
        m_retroCore->Shutdown();
    spdlog::info("[dosbox-uwp] Emulation thread joined (ShutdownNow)");
}

void dosbox_uwpMain::RenderLoadingScreen(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, D2D1_SIZE_F logicalSize)
{
    const auto& theme = SettingsManager::GetTheme();
    float w = logicalSize.width;
    float h = logicalSize.height;

    float dpiscale;
    { FLOAT dx, dy; d2d->GetDpi(&dx, &dy); dpiscale = dx / 96.0f; }

    // Semi-transparent fullscreen background
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> blackBg;
    d2d->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, theme.overlay_alpha), &blackBg);
    if (blackBg) d2d->FillRectangle(D2D1::RectF(0, 0, w, h), blackBg.Get());

    // Centered dialog panel
    float panelW = min(w * 0.50f, 360.0f);
    float panelH = min(h * 0.40f, 260.0f);
    float panelX = (w - panelW) * 0.5f;
    float panelY = (h - panelH) * 0.5f;

    // Panel background
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bgBrush;
    d2d->CreateSolidColorBrush(D2D1::ColorF(theme.bg_panel), &bgBrush);
    if (bgBrush) d2d->FillRectangle(D2D1::RectF(panelX, panelY, panelX + panelW, panelY + panelH), bgBrush.Get());

    // Panel border (double border like AboutDialog)
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> frameBrush;
    d2d->CreateSolidColorBrush(D2D1::ColorF(theme.frame), &frameBrush);
    if (frameBrush)
    {
        d2d->DrawRectangle(D2D1::RectF(panelX, panelY, panelX + panelW, panelY + panelH), frameBrush.Get(), 2.0f);
        d2d->DrawRectangle(D2D1::RectF(panelX + 4, panelY + 4, panelX + panelW - 4, panelY + panelH - 4), frameBrush.Get(), 1.0f);
    }

    // Title bar (animated alpha pulse)
    float TITLE_H = 38.0f * dpiscale;
    float titleTop = panelY + 8;
    D2D1_RECT_F titleBg = { panelX + 8, titleTop, panelX + panelW - 8, titleTop + TITLE_H };
    float t = (float)(GetTickCount64() % 3000) / 3000.0f;
    float alpha = 0.70f + 0.30f * (0.5f + 0.5f * sinf(t * 6.283185f));
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> titlePulseBrush;
    D2D1_COLOR_F titleCol = D2D1::ColorF(theme.title_bg);
    titleCol.a = alpha;
    d2d->CreateSolidColorBrush(titleCol, &titlePulseBrush);
    if (titlePulseBrush) d2d->FillRectangle(titleBg, titlePulseBrush.Get());

    // Title text "LOADING" centered
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFmt;
    dwrite->CreateTextFormat(L"VCR OSD Mono", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 28.0f * dpiscale, L"en-US", &titleFmt);
    if (titleFmt)
    {
        titleFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        titleFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        float titleBarW = panelW - 32.0f;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> titleLayout;
        dwrite->CreateTextLayout(L"LOADING", 7, titleFmt.Get(), titleBarW, TITLE_H, &titleLayout);
        if (titleLayout && m_menu.GetFontCollection())
        {
            DWRITE_TEXT_RANGE r = { 0, 7 };
            titleLayout->SetFontCollection(m_menu.GetFontCollection(), r);
        }
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> titleTextBrush;
        d2d->CreateSolidColorBrush(D2D1::ColorF(theme.text_title), &titleTextBrush);
        if (titleLayout && titleTextBrush)
            d2d->DrawTextLayout(D2D1::Point2F(panelX + 16, titleTop), titleLayout.Get(), titleTextBrush.Get());
    }

    // Content area
    float contentTop = panelY + 8 + TITLE_H + 16.0f;
    float contentBottom = panelY + panelH - 16.0f;
    float contentCenterX = panelX + panelW * 0.5f;

    // Spinning disc centered
    float discSize = 80.0f;
    float discCenterY = (contentTop + contentBottom) * 0.5f - 20.0f;
    EnsureLoadingDisc();

    if (m_loadingDisc)
    {
        D2D1_RECT_F discRect = {
            contentCenterX - discSize * 0.5f, discCenterY - discSize * 0.5f,
            contentCenterX + discSize * 0.5f, discCenterY + discSize * 0.5f
        };
        d2d->SetTransform(D2D1::Matrix3x2F::Rotation(m_loadingAngle,
            D2D1::Point2F(contentCenterX, discCenterY)));
        d2d->DrawBitmap(m_loadingDisc.Get(), discRect, 1.0f);
        d2d->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    else
    {
        // Fallback: orange circle
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> orangeBrush;
        d2d->CreateSolidColorBrush(D2D1::ColorF(0xff8800), &orangeBrush);
        if (orangeBrush)
            d2d->FillEllipse(D2D1::Ellipse(D2D1::Point2F(contentCenterX, discCenterY),
                discSize * 0.5f, discSize * 0.5f), orangeBrush.Get());
    }

    // "Loading..." text — bottom-left of panel, fixed width so dots don't shift
    static const wchar_t* dotStates[] = { L".", L"..", L"...", L"" };
    wchar_t loadingText[64];
    swprintf_s(loadingText, L"Loading%s", dotStates[m_loadingDots]);

    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFmt;
    dwrite->CreateTextFormat(L"VCR OSD Mono", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f * dpiscale, L"en-US", &textFmt);
    if (textFmt)
    {
        textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
        float fixedTextW = 160.0f * dpiscale;
        float textX = panelX + 16.0f;
        float textY = panelY + panelH - 40.0f;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
        dwrite->CreateTextLayout(loadingText, (UINT32)wcslen(loadingText), textFmt.Get(),
            fixedTextW, 30.0f, &textLayout);
        if (textLayout && m_menu.GetFontCollection())
        {
            DWRITE_TEXT_RANGE r = { 0, (UINT32)wcslen(loadingText) };
            textLayout->SetFontCollection(m_menu.GetFontCollection(), r);
        }
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
        d2d->CreateSolidColorBrush(D2D1::ColorF(0xcccccc), &textBrush);
        if (textLayout && textBrush)
            d2d->DrawTextLayout(D2D1::Point2F(textX, textY), textLayout.Get(), textBrush.Get());
    }
}
