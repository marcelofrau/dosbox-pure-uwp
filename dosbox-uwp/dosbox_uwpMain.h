#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"
#include "Content\SampleFpsTextRenderer.h"
#include "Content\SdlInput.h"
#include "Content\RetroCore.h"
#include "Content\RetroScreenRenderer.h"
#include "Content\XAudio2Output.h"

namespace dosbox_uwp
{
    class dosbox_uwpMain : public DX::IDeviceNotify
    {
    public:
        dosbox_uwpMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~dosbox_uwpMain();
        void CreateWindowSizeDependentResources();
        void Update();
        void DoPacingSleep();
        bool Render();

        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();
		void OnKeyEvent(Windows::System::VirtualKey key, bool down, uint32_t scanCode = 0, bool isExtended = false);
#ifdef MOUSE_SUPPORT
		void OnPointerMove(float nx, float ny, float px, float py);
		void OnPointerDown(float nx, float ny, unsigned btn);
		void OnPointerUp(unsigned btn);
		void OnPointerRelease();
		void OnPointerWheel(int delta);
		void SetMousePointerId(uint32_t id);
		void PollMouseButtons();
#endif
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
        void PollKeyboard();

        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;
        std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;
        std::unique_ptr<SdlInput> m_sdlInput;
        std::unique_ptr<RetroCore> m_retroCore;
        std::unique_ptr<RetroScreenRenderer> m_retroScreen;
        std::unique_ptr<XAudio2Output> m_xaudio2;

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

#ifdef MOUSE_SUPPORT
        float m_pointerX = 0.5f;
        float m_pointerY = 0.5f;
        bool m_pointerDown = false;
        float m_lastPointerX = 0.5f;
        float m_lastPointerY = 0.5f;
        float m_lastPointerPX = 0.0f;
        float m_lastPointerPY = 0.0f;
        uint32_t m_mousePointerId = 0;
#endif

        bool m_activeVKeyState[256] = {};

        // Frame pacing tracking
        bool m_frameLate = false;
        int m_lateFrameCount = 0;
        int m_lateFramesHud = 0;
        LARGE_INTEGER m_lastFrameTime = {};
        LARGE_INTEGER m_qpcFreq = {};
        HANDLE m_hFrameTimer = nullptr;
    };
}
