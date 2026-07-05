#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"
#include "Content\SampleFpsTextRenderer.h"
#include "Content\SdlInput.h"
#include "Content\RetroCore.h"
#include "Content\RetroScreenRenderer.h"

namespace dosbox_uwp
{
    class dosbox_uwpMain : public DX::IDeviceNotify
    {
    public:
        dosbox_uwpMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~dosbox_uwpMain();
        void CreateWindowSizeDependentResources();
        void Update();
        bool Render();

        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();
        void OnKeyEvent(Windows::System::VirtualKey key, bool down);
        void ToggleOSD();
        void LoadRom(const std::wstring& path, std::vector<uint8_t> romData);
        enum LoadState { LOAD_IDLE, LOAD_PICKING, LOAD_READING, LOAD_BOOTING, LOAD_DONE, LOAD_FAILED };
        bool WasFilePickerRequested() { bool r = m_requestFilePicker; m_requestFilePicker = false; return r; }
        bool WasFrameLate() const { return m_frameLate; }
        int GetLateFrameCount() const { return m_lateFrameCount; }
        LoadState GetLoadState() const { return m_loadState; }
        void SetLoadState(LoadState s) { m_loadState = s; }
        bool IsLoaded() const { return m_retroCore && m_retroCore->IsLoaded(); }

    private:
        void BootCore();

        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;
        std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;
        std::unique_ptr<SdlInput> m_sdlInput;
        std::unique_ptr<RetroCore> m_retroCore;
        std::unique_ptr<RetroScreenRenderer> m_retroScreen;

        DX::StepTimer m_timer;

        DirectX::XMVECTORF32 m_clearColor;
        DirectX::XMVECTORF32 m_defaultClearColor;
        bool m_requestFilePicker = false;
        bool m_spaceHeld = false;
        bool m_hasController;

        std::wstring m_eventText;
        int m_eventTimer;

        bool m_retroRunning = false;
        std::wstring m_statusText;
        int m_statusTimer = 0;

        std::wstring m_currentTempPath;
        void CleanupTempFile();

        // Load state tracking (for hang detection)
        LoadState m_loadState = LOAD_IDLE;
        int m_loadTimer = 0;

        // Frame pacing tracking
        bool m_frameLate = false;
        int m_lateFrameCount = 0;
        int m_lateFramesHud = 0;
        bool m_pacingEnabled = false;
        LARGE_INTEGER m_lastFrameTime = {};
        LARGE_INTEGER m_qpcFreq = {};
    };
}
