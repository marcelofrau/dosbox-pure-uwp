#include "pch.h"
#include "dosbox_uwpMain.h"
#include "libretro.h"
#include "Common\DirectXHelper.h"
#include <cmath>
#include <sstream>

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
        sprintf_s(buf, "SDL: controller=%s audio=%s\n",
            m_hasController ? "CONNECTED" : "NONE (SPACE=btnA)",
            m_sdlInput->IsAudioReady() ? "OK" : "FAIL");
        OutputDebugStringA(buf);
    }

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

    OutputDebugStringA("[dosbox-uwp] Loading ROM...\n");

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
        m_sdlInput->PollEvents();

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

        if (m_retroRunning && m_retroCore->IsLoaded())
        {
            m_retroCore->RunFrame();
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

            wchar_t buf[512];
            swprintf_s(buf, L"%s  SDL:%s CTL:%s AUDIO:%s INP:%s  %s\n"
                L"CTLR:%hs SND:%dHz  %s",
                retroStatus.c_str(),
                m_sdlInput->IsInitialized() ? L"OK" : L"FAIL",
                m_hasController ? L"CONN" : L"NONE",
                m_sdlInput->IsAudioReady() ? L"OK" : L"FAIL",
                inputSrc,
                (m_eventTimer > 0) ? m_eventText.c_str() : L"",
                m_sdlInput->GetControllerName(),
                m_sdlInput->GetAudioSampleRate(),
                statusLine.c_str());
            if (m_eventTimer > 0) m_eventTimer--;
            if (m_statusTimer > 0) m_statusTimer--;
            m_fpsTextRenderer->SetDebugText(buf);
        }

        m_sceneRenderer->Update(m_timer);
        m_fpsTextRenderer->Update(m_timer);
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
            sprintf_s(buf, "[dosbox-uwp] Render #%d: frame.valid=%d w=%u h=%u\n",
                renderCount, frame.valid, frame.width, frame.height);
            OutputDebugStringA(buf);
        }

        if (frame.valid)
        {
            m_retroScreen->UpdateVideoFrame(frame.data.data(), frame.width, frame.height, frame.pitch);
        }
        else
        {
            if ((renderCount % 300) == 0)
                OutputDebugStringA("[dosbox-uwp] Render: no valid frame, skipping UpdateVideoFrame\n");
        }
        m_retroScreen->Render();
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
    if (key == Windows::System::VirtualKey::Space)
    {
        m_spaceHeld = down;
        m_sdlInput->SetKeyboardButton(BUTTON_A, down);
        if (down) m_sdlInput->PlayBeep(880.0f, 0.15f, 0.5f);
    }

    if (m_retroCore)
    {
        int vk = (int)key;
        unsigned retroKey = 0;

        if (vk >= 'A' && vk <= 'Z')
            retroKey = (unsigned)vk;
        else if (vk >= '0' && vk <= '9')
            retroKey = (unsigned)vk;
        else if (key == Windows::System::VirtualKey::Enter)
            retroKey = 0x0D;
        else if (key == Windows::System::VirtualKey::Escape)
            retroKey = 0x1B;
        else if (key == Windows::System::VirtualKey::Back)
            retroKey = 0x08;
        else if (key == Windows::System::VirtualKey::Tab)
            retroKey = 0x09;
        else if (key == Windows::System::VirtualKey::Up)
            retroKey = 0x26;
        else if (key == Windows::System::VirtualKey::Down)
            retroKey = 0x28;
        else if (key == Windows::System::VirtualKey::Left)
            retroKey = 0x25;
        else if (key == Windows::System::VirtualKey::Right)
            retroKey = 0x27;
        else if (key == Windows::System::VirtualKey::Shift)
            retroKey = 0x10;
        else if (key == Windows::System::VirtualKey::Control)
            retroKey = 0x11;
        else if (key == Windows::System::VirtualKey::Menu)
            retroKey = 0x12;

        if (retroKey > 0)
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
