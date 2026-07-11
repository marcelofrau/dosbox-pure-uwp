#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <functional>
#include "FileBrowser.h"

namespace dosbox_uwp
{
    enum class MenuAction
    {
        NONE,
        SUBMENU,
        BACK,
        CONTINUE_GAME,
        OPEN_FILE,
        OPEN_PUREMENU,
        SETTINGS,
        VIDEO,
        AUDIO,
        STATE,
        TOGGLE_VALUE,
        ABOUT,
        EXIT
    };

    struct MenuItem
    {
        std::string label;
        MenuAction action;
        std::vector<MenuItem> children;
        std::vector<std::string> values;
        int currentValue = 0;
        bool enabled = true;
    };

    struct MenuPage
    {
        std::string title;
        std::vector<MenuItem> items;
    };

    class FrontendMenu
    {
    public:
        FrontendMenu();

        void Render(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH);
        void OnDPad(bool up);
        void OnConfirm();
        void OnBack();
        void OnPageUp();
        void OnPageDown();
        int HitTest(float sx, float sy);
        void SelectItem(int idx);
        void HandlePointerMove(float sx, float sy);
        void HandlePointerDown(float sx, float sy, unsigned btn);
        void HandlePointerWheel(int delta);
        void RenderFullScreen(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH);

        bool IsVisible() const { return m_visible; }
        void Show() { m_visible = true; }
        void Hide() { m_visible = false; }

        void SetCoreLoaded(bool loaded);
        void LoadLogoBitmap(ID2D1DeviceContext* d2d);

        void SetBiosInfo(const std::vector<std::wstring>& lines) { m_biosLines = lines; }
        bool IsBootAnimComplete() const { return m_animPhase >= ANIM_COMPLETE; }
        bool IsBeepGracePeriod() const { return m_beepPlayed && (GetTickCount64() - m_animCompleteTick) < 300; }
        void ResetBootAnim() { m_animPhase = ANIM_INITIAL_DELAY; m_animStartTick = GetTickCount64(); m_beepPlayed = false; }

        std::function<void()> onOpenFile;
        std::function<void()> onOpenPuremenu;
        std::function<void()> onExit;
        std::function<void()> onBeep;

        FileBrowser m_fileBrowser;

    private:
        void BuildMenuTree();
        void RebuildItems();
        void EnsureResources(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH);

        bool m_visible = true;
        bool m_coreLoadedPrev = false;

        enum AnimPhase { ANIM_INITIAL_DELAY, ANIM_BIOS_POST, ANIM_MEMORY_COUNT, ANIM_COMPLETE };
        AnimPhase m_animPhase = ANIM_INITIAL_DELAY;
        ULONGLONG m_animStartTick = 0;
        int m_biosLinesToShow = 0;
        int m_animMemoryDisplayedKB = 0;
        int m_memoryTotalMB = 0;
        bool m_beepPlayed = false;
        ULONGLONG m_animCompleteTick = 0;

        int m_selected = 0;
        int m_scrollOffset = 0;

        std::vector<MenuItem> m_mainItems;
        std::vector<MenuItem> m_settingsItems;
        std::vector<MenuItem> m_videoItems;
        std::vector<MenuItem> m_audioItems;
        std::vector<MenuItem> m_stateItems;
        std::vector<MenuItem> m_aboutItems;
        std::vector<std::wstring> m_biosLines;

        struct PageRef
        {
            std::string title;
            std::vector<MenuItem>* items;
        };
        std::vector<PageRef> m_stack;

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushTitleBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushSelected;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushItemText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushTitleText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushValueText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushDisabled;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushFooter;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushFrame;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushBios;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushBlack;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushWhite;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatTitle;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatItem;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatFooter;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_epaLogo;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_dosboxLogo;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> m_fontCollection;

    bool m_resourcesCreated = false;

        static constexpr float TITLE_HEIGHT = 36.0f;
        static constexpr float ITEM_HEIGHT = 30.0f;
        static constexpr float ITEM_INDENT = 24.0f;
        static constexpr float FOOTER_HEIGHT = 22.0f;
        static constexpr float MAX_VISIBLE = 14.0f;
        static constexpr float PANEL_MARGIN = 20.0f;
        static constexpr float PANEL_WIDTH_RATIO = 0.45f;
        static constexpr float PANEL_MAX_WIDTH = 480.0f;
        static constexpr float PANEL_FIXED_HEIGHT = 360.0f;
        static constexpr float LOGO_SIZE = 100.0f;
        static constexpr float LOGO_MARGIN = 20.0f;

        float m_lastPanelX = 0, m_lastPanelY = 0;
        float m_lastPanelW = 0, m_lastPanelH = 0;
        float m_lastScreenW = 0, m_lastScreenH = 0;
        std::wstring m_versionStr;
    };
}
