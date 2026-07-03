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
        void LoadRom(const std::wstring& path, std::vector<uint8_t> romData);
        bool WasFilePickerRequested() { bool r = m_requestFilePicker; m_requestFilePicker = false; return r; }

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
    };
}
