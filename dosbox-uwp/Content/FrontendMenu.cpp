#include "pch.h"
#include "FrontendMenu.h"
#include "SettingsManager.h"
#include "RetroCore.h"
#include <wincodec.h>
#include <sysinfoapi.h>
#include <memoryapi.h>
#include <windows.h>
#include <intrin.h>
#include <Windows.Security.ExchangeActiveSyncProvisioning.h>
#include <Windows.System.Profile.h>
#include <Windows.ApplicationModel.h>
#include <cmath>

using namespace dosbox_uwp;
using namespace Microsoft::WRL;
using namespace Windows::Security::ExchangeActiveSyncProvisioning;
using namespace Windows::System::Profile;
using namespace Windows::ApplicationModel;

static std::wstring TrimStr(std::wstring s)
{
    auto end = s.find_last_not_of(L' ');
    if (end != std::wstring::npos) s.erase(end + 1);
    auto start = s.find_first_not_of(L' ');
    if (start != std::wstring::npos) s.erase(0, start);
    return s;
}

static std::wstring GetCpuBrand()
{
#ifdef _M_AMD64
    int CPUInfo[4] = { -1 };
    __cpuid(CPUInfo, 0x80000000);
    unsigned int nExIds = CPUInfo[0];
    if (nExIds >= 0x80000004)
    {
        char brand[0x40] = { 0 };
        for (unsigned int i = 0x80000002; i <= 0x80000004; i++)
        {
            __cpuid(CPUInfo, i);
            memcpy(brand + (i - 0x80000002) * 16, CPUInfo, sizeof(CPUInfo));
        }
        int wlen = MultiByteToWideChar(CP_UTF8, 0, brand, -1, nullptr, 0);
        if (wlen > 0)
        {
            std::wstring result;
            result.resize(wlen - 1);
            MultiByteToWideChar(CP_UTF8, 0, brand, -1, &result[0], wlen);
            return TrimStr(result);
        }
    }
#endif
    return L"Unknown CPU";
}

static std::wstring GetMemoryStr()
{
    MEMORYSTATUSEX memInfo = { sizeof(memInfo) };
    if (GlobalMemoryStatusEx(&memInfo))
    {
        double mb = (double)memInfo.ullTotalPhys / (1024.0 * 1024.0);
        wchar_t buf[32];
        swprintf_s(buf, L"Memory: %.0f MB OK", mb);
        return buf;
    }
    return L"Memory: Unknown";
}

static std::wstring GetConsoleModel()
{
    try
    {
        auto deviceInfo = ref new EasClientDeviceInformation();
        std::wstring family(AnalyticsInfo::VersionInfo->DeviceFamily->Data());
        std::wstring model(deviceInfo->SystemProductName->Data());
        if (family == L"Windows.Xbox")
            return model.empty() ? L"Xbox" : model;
    }
    catch (...) {}
    return L"";
}

FrontendMenu::FrontendMenu()
{
    std::wstring cpuLine = L"CPU: " + GetCpuBrand();
    std::wstring memLine = GetMemoryStr();

    m_biosLines.push_back(L"DOSBox Pure Unleashed BIOS (UWP Edition)");
    m_biosLines.push_back(L"Copyright (C) 2024-2025 Unleashed Project");
    m_biosLines.push_back(L"");
    m_biosLines.push_back(cpuLine);
    m_biosLines.push_back(memLine);

    std::wstring consoleModel = GetConsoleModel();
    if (!consoleModel.empty())
        m_biosLines.push_back(L"System: " + consoleModel);

    m_biosLines.push_back(L"");
    m_biosLines.push_back(L"Award Plug and Play BIOS Extension v1.0A");
    m_biosLines.push_back(L"Copyright (C) 1997 Award Software, Inc.");
    m_biosLines.push_back(L"");
    m_biosLines.push_back(L"F10 / L3 = Menu     L = Open Game     ESC = Exit");

    // Parse total memory MB for count-up animation
    {
        MEMORYSTATUSEX memInfo = { sizeof(memInfo) };
        if (GlobalMemoryStatusEx(&memInfo))
            m_memoryTotalMB = (int)(memInfo.ullTotalPhys / (1024ULL * 1024ULL));
        else
            m_memoryTotalMB = 64;
    }

    try
    {
        auto pkg = Package::Current;
        auto ver = pkg->Id->Version;
        wchar_t buf[64];
        swprintf_s(buf, L"v%hu.%hu.%hu.%hu", ver.Major, ver.Minor, ver.Build, ver.Revision);
        m_versionStr = buf;
    }
    catch (...) { m_versionStr = L"v?.?.?.?"; }

    m_animStartTick = GetTickCount64();

    BuildMenuTree();
    m_visible = true;
}

void FrontendMenu::SetCoreLoaded(bool loaded)
{
    if (m_coreLoadedPrev == loaded) return;
    m_coreLoadedPrev = loaded;
    RebuildItems();
}

void FrontendMenu::BuildMenuTree()
{
    m_mainItems =
    {
        { "Load File",           MenuAction::OPEN_FILE },
        { "",                    MenuAction::NONE },
        { "Settings",            MenuAction::SETTINGS, {}, {}, 0, true },
        { "Controls",            MenuAction::OPEN_PUREMENU, {}, {}, 0, false },
        { "",                    MenuAction::NONE },
        { "Exit",                MenuAction::EXIT },
    };

    m_settingsItems =
    {
        { "Video",               MenuAction::VIDEO },
        { "Audio",               MenuAction::AUDIO },
        { "Core Options",        MenuAction::OPEN_PUREMENU },
        { "",                    MenuAction::NONE },
        { "Back",                MenuAction::BACK },
    };

    // Helper to find current value index from settings
    auto findVal = [](const std::string& key, const std::vector<std::string>& vals, const char* def) -> int {
        std::string cur = SettingsManager::GetOption(key.c_str(), def);
        for (int i = 0; i < (int)vals.size(); i++)
            if (vals[i] == cur) return i;
        return 0;
    };

    // Video options
    {
        std::vector<std::string> vsyncVals = { "Off", "On" };
        std::vector<std::string> scalerVals = { "Nearest", "Bilinear" };
        std::vector<std::string> aspectVals = { "Off", "On", "Doublescan", "Padded", "Padded+Doublescan" };
        std::vector<std::string> machineVals = { "SVGA", "VGA", "EGA", "CGA", "Tandy", "Hercules", "PCJR" };
        std::vector<std::string> svgaMemVals = { "0 (512KB)", "1 (1MB)", "2 (2MB)", "3 (3MB)", "4 (4MB)", "8 (8MB)" };
        std::vector<std::string> overscanVals = { "0", "1", "2", "3" };

        m_videoItems = {
            { "VSync",             MenuAction::TOGGLE_VALUE, {}, vsyncVals, findVal("frontend_vsync", vsyncVals, "On"), true, "frontend_vsync" },
            { "Scaler",            MenuAction::TOGGLE_VALUE, {}, scalerVals, findVal("frontend_scaler", scalerVals, "Bilinear"), true, "frontend_scaler" },
            { "",                  MenuAction::NONE },
            { "Aspect Ratio",      MenuAction::TOGGLE_VALUE, {}, aspectVals, findVal("dosbox_pure_aspect_correction", aspectVals, "Off"), true, "dosbox_pure_aspect_correction" },
            { "Graphics Chip",     MenuAction::TOGGLE_VALUE, {}, machineVals, findVal("dosbox_pure_machine", machineVals, "SVGA"), true, "dosbox_pure_machine" },
            { "SVGA Memory",       MenuAction::TOGGLE_VALUE, {}, svgaMemVals, findVal("dosbox_pure_svgamem", svgaMemVals, "2 (2MB)"), true, "dosbox_pure_svgamem" },
            { "Overscan",          MenuAction::TOGGLE_VALUE, {}, overscanVals, findVal("dosbox_pure_overscan", overscanVals, "0"), true, "dosbox_pure_overscan" },
            { "",                  MenuAction::NONE },
            { "Back",              MenuAction::BACK },
        };
    }

    // Audio options
    {
        std::vector<std::string> rateVals = { "48000", "44100", "32000", "22050", "16000", "11025" };
        std::vector<std::string> sbTypeVals = { "SB16", "SBPro2", "SBPro1", "SB2", "SB1", "Game Blaster", "None" };
        std::vector<std::string> volVals = { "50%", "100%", "150%", "200%", "300%", "500%" };
        std::vector<std::string> midiVals = { "Disabled" };

        m_audioItems = {
            { "Sample Rate",       MenuAction::TOGGLE_VALUE, {}, rateVals, findVal("dosbox_pure_audiorate", rateVals, "48000"), true, "dosbox_pure_audiorate" },
            { "SoundBlaster Type", MenuAction::TOGGLE_VALUE, {}, sbTypeVals, findVal("dosbox_pure_sblaster_type", sbTypeVals, "SB16"), true, "dosbox_pure_sblaster_type" },
            { "",                  MenuAction::NONE },
            { "Volume: SB",        MenuAction::TOGGLE_VALUE, {}, volVals, findVal("dosbox_pure_volume_sb", volVals, "100%"), true, "dosbox_pure_volume_sb" },
            { "Volume: MIDI",      MenuAction::TOGGLE_VALUE, {}, volVals, findVal("dosbox_pure_volume_midi", volVals, "100%"), true, "dosbox_pure_volume_midi" },
            { "Volume: Adlib",     MenuAction::TOGGLE_VALUE, {}, volVals, findVal("dosbox_pure_volume_adlib", volVals, "100%"), true, "dosbox_pure_volume_adlib" },
            { "Volume: Speaker",   MenuAction::TOGGLE_VALUE, {}, volVals, findVal("dosbox_pure_volume_speaker", volVals, "100%"), true, "dosbox_pure_volume_speaker" },
            { "",                  MenuAction::NONE },
            { "Back",              MenuAction::BACK },
        };
    }

    m_stateItems =
    {
        { "Save State",          MenuAction::TOGGLE_VALUE, {}, {}, 0, false },
        { "Load State",          MenuAction::TOGGLE_VALUE, {}, {}, 0, false },
        { "Slot",                MenuAction::TOGGLE_VALUE, {}, { "1", "2", "3", "4", "5" }, 0 },
        { "",                    MenuAction::NONE },
        { "Back",                MenuAction::BACK },
    };

    m_aboutItems =
    {
        { "DOSBox Pure Unleashed", MenuAction::NONE },
        { "UWP Frontend",          MenuAction::NONE },
        { "Based on dosbox-pure",  MenuAction::NONE },
        { "libretro core",         MenuAction::NONE },
        { "",                      MenuAction::NONE },
        { "Back",                  MenuAction::BACK },
    };

    RebuildItems();
}

void FrontendMenu::RebuildItems()
{
    m_stack.clear();
    m_stack.push_back({ "DOSBox Pure Unleashed", &m_mainItems });
    m_selected = 0;
    m_scrollOffset = 0;
    auto& items = *m_stack.back().items;
    for (int i = 0; i < (int)items.size(); i++)
    {
        if (!items[i].label.empty() && items[i].enabled && items[i].action != MenuAction::NONE)
        {
            m_selected = i;
            break;
        }
    }
}

static bool TryLoadImgFromPath(ID2D1DeviceContext* d2d, const wchar_t* imgPath, ID2D1Bitmap1** bitmap, IWICImagingFactory* wicFactory)
{
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wicFactory->CreateDecoderFromFilename(imgPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wicFactory->CreateFormatConverter(&converter))) return false;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr))
    {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr)) return false;
    }
    hr = d2d->CreateBitmapFromWicBitmap(converter.Get(), bitmap);
    return SUCCEEDED(hr);
}

static void LoadImg(ID2D1DeviceContext* d2d, const wchar_t* filename, ID2D1Bitmap1** bitmap)
{
    ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) { spdlog::warn("LoadImg: CoCreateInstance WIC failed hr={:08x}", (unsigned)hr); return; }

    // Try path 1: Package::InstalledLocation (AppX deployed)
    {
        wchar_t imgPath[MAX_PATH];
        try
        {
            auto installPath = Package::Current->InstalledLocation->Path;
            wcscpy_s(imgPath, installPath->Data());
            size_t plen = wcslen(imgPath);
            if (plen > 0 && plen < MAX_PATH - 60)
            {
                if (imgPath[plen - 1] != L'\\') { imgPath[plen] = L'\\'; plen++; }
                wcscpy_s(imgPath + plen, MAX_PATH - plen, filename);
            }
        }
        catch (...) { return; }
        if (TryLoadImgFromPath(d2d, imgPath, bitmap, wicFactory.Get()))
        {
            spdlog::info("LoadImg: OK from InstalledLocation");
            return;
        }
        spdlog::warn("LoadImg: InstalledLocation failed, trying parent dir");
    }

    // Try path 2: parent of InstalledLocation (project output dir, CopyToOutputDirectory)
    {
        wchar_t imgPath[MAX_PATH];
        try
        {
            auto installPath = Package::Current->InstalledLocation->Path;
            wcscpy_s(imgPath, installPath->Data());
            wchar_t* lastSlash = wcsrchr(imgPath, L'\\');
            if (lastSlash) *lastSlash = L'\0';
            size_t plen = wcslen(imgPath);
            if (plen > 0 && plen < MAX_PATH - 60)
            {
                if (imgPath[plen - 1] != L'\\') { imgPath[plen] = L'\\'; plen++; }
                wcscpy_s(imgPath + plen, MAX_PATH - plen, filename);
            }
        }
        catch (...) { return; }
        if (TryLoadImgFromPath(d2d, imgPath, bitmap, wicFactory.Get()))
        {
            spdlog::info("LoadImg: OK from parent dir");
            return;
        }
        spdlog::warn("LoadImg: all paths failed");
    }
}

void FrontendMenu::LoadLogoBitmap(ID2D1DeviceContext* d2d)
{
    if (!m_epaLogo)
        LoadImg(d2d, L"Assets\\EPA_logo.png", &m_epaLogo);
    if (!m_dosboxLogo)
        LoadImg(d2d, L"Assets\\dosbox-transparent.png", &m_dosboxLogo);
}

void FrontendMenu::EnsureResources(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH)
{
    if (m_resourcesCreated) return;

    // Load VCR OSD Mono from DWrite custom font collection
    wchar_t fontPath[MAX_PATH];
    auto installPath = Package::Current->InstalledLocation->Path;
    wcscpy_s(fontPath, installPath->Data());
    size_t flen = wcslen(fontPath);
    if (flen > 0 && flen < MAX_PATH - 60)
    {
        if (fontPath[flen - 1] != L'\\') { fontPath[flen] = L'\\'; flen++; }
        wcscpy_s(fontPath + flen, MAX_PATH - flen, L"Assets\\Fonts\\VCR_OSD_MONO_1.001.ttf");
            ComPtr<IDWriteFontFile> fontFile;
            if (SUCCEEDED(dwrite->CreateFontFileReference(fontPath, nullptr, &fontFile)))
            {
                ComPtr<IDWriteFactory5> dwrite5;
                if (SUCCEEDED(dwrite->QueryInterface(IID_PPV_ARGS(&dwrite5))))
                {
                    ComPtr<IDWriteFontSetBuilder1> builder;
                    if (SUCCEEDED(dwrite5->CreateFontSetBuilder(&builder)))
                    {
                        if (SUCCEEDED(builder->AddFontFile(fontFile.Get())))
                        {
                            ComPtr<IDWriteFontSet> fontSet;
                            if (SUCCEEDED(builder->CreateFontSet(&fontSet)))
                            {
                                ComPtr<IDWriteFontCollection1> col1;
                                if (SUCCEEDED(dwrite5->CreateFontCollectionFromFontSet(fontSet.Get(), &col1)))
                                {
                                    col1.As(&m_fontCollection);
                                }
                            }
                        }
                    }
                }
            }
    }

    float fontSizeTitle = 33.0f;
    float fontSizeItem = 27.0f;
    float fontSizeFooter = 22.0f;
    if (screenW < 800.0f)
    {
        fontSizeTitle = 27.0f;
        fontSizeItem = 22.0f;
        fontSizeFooter = 18.0f;
    }

    {
        const auto& c = SettingsManager::GetTheme();
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.bg_fullscreen), &m_brushBlack);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_disabled), &m_brushDisabled);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.bg_panel), &m_brushBg);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.title_bg), &m_brushTitleBg);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.selection_text), &m_brushSelected);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_normal), &m_brushItemText);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.selection_bg), &m_brushTitleText);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_value), &m_brushValueText);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_disabled), &m_brushFooter);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.frame), &m_brushFrame);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_bios), &m_brushBios);
        d2d->CreateSolidColorBrush(D2D1::ColorF(c.text_title), &m_brushWhite);
    }

    dwrite->CreateTextFormat(
        L"VCR OSD Mono", nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSizeTitle, L"en-US", &m_textFormatTitle);
    dwrite->CreateTextFormat(
        L"VCR OSD Mono", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSizeItem, L"en-US", &m_textFormatItem);
    dwrite->CreateTextFormat(
        L"VCR OSD Mono", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSizeFooter, L"en-US", &m_textFormatFooter);

    LoadLogoBitmap(d2d);

    m_resourcesCreated = true;
}

void FrontendMenu::Render(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH)
{
    if (!m_visible) return;
}

static constexpr double ANIM_INITIAL_SEC = 0.5;
static constexpr double ANIM_LINE_INTERVAL = 0.18;
static constexpr double ANIM_EMPTY_INTERVAL = 0.03;
static constexpr double ANIM_MEMORY_DELAY = 0.3;
static constexpr double ANIM_MEMORY_DURATION = 1.2;

static void DrawTextLine(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, IDWriteFontCollection* fc,
    IDWriteTextFormat* fmt, const wchar_t* text, UINT32 len, float x, float y, float w, float h,
    ID2D1Brush* brush)
{
    ComPtr<IDWriteTextLayout> layout;
    dwrite->CreateTextLayout(text, len, fmt, w, h, &layout);
    if (layout && fc)
    {
        DWRITE_TEXT_RANGE r = { 0, len };
        layout->SetFontCollection(fc, r);
    }
    if (layout)
        d2d->DrawTextLayout(D2D1::Point2F(x, y), layout.Get(), brush);
}

void FrontendMenu::RenderFullScreen(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite, float screenW, float screenH)
{
    if (!m_visible) return;

    EnsureResources(d2d, dwrite, screenW, screenH);

    m_lastScreenW = screenW;
    m_lastScreenH = screenH;

    // Full black background
    D2D1_RECT_F fullBg = { 0, 0, screenW, screenH };
    d2d->FillRectangle(fullBg, m_brushBlack.Get());

    // EPA logo top-right with pulsing animation
    if (m_epaLogo)
    {
        D2D1_SIZE_F epaSize = m_epaLogo->GetSize();
        float epaW = 150.0f;
        float epaH = epaSize.height * (epaW / epaSize.width);
        float epaX = screenW - PANEL_MARGIN - epaW;
        double t = GetTickCount64() / 1000.0;
        float opacity = 0.4f + 0.3f * (sinf((float)(t * 3.0)) + 1.0f);
        D2D1_RECT_F epaRect = { epaX, PANEL_MARGIN, epaX + epaW, PANEL_MARGIN + epaH };
        d2d->DrawBitmap(m_epaLogo.Get(), epaRect, opacity);
    }

    // Boot animation — derive phases + continuous memory count
    double elapsedSec = (GetTickCount64() - m_animStartTick) / 1000.0;

    AnimPhase newPhase;
    int linesToShow = 0;
    int memDisplayedKB = 0;

    double totalLines = (double)m_biosLines.size();
    double lineSeqTime = ANIM_INITIAL_SEC + totalLines * ANIM_LINE_INTERVAL;
    double memRevealTime = ANIM_INITIAL_SEC + 5 * ANIM_LINE_INTERVAL; // line index 4 (approximate, skips empty-line variance)

    if (elapsedSec < ANIM_INITIAL_SEC)
    {
        newPhase = ANIM_INITIAL_DELAY;
    }
    else if (elapsedSec < lineSeqTime)
    {
        newPhase = ANIM_BIOS_POST;
        double textElapsed = elapsedSec - ANIM_INITIAL_SEC;
        double timeUsed = 0.0;
        for (int i = 0; i < (int)m_biosLines.size() && timeUsed <= textElapsed; i++)
        {
            linesToShow = i + 1;
            timeUsed += m_biosLines[i].empty() ? ANIM_EMPTY_INTERVAL : ANIM_LINE_INTERVAL;
        }
    }
    else if (elapsedSec < lineSeqTime + ANIM_MEMORY_DELAY + ANIM_MEMORY_DURATION)
    {
        newPhase = ANIM_MEMORY_COUNT;
        linesToShow = (int)m_biosLines.size();
    }
    else
    {
        newPhase = ANIM_COMPLETE;
        linesToShow = (int)m_biosLines.size();
    }

    // Continuous memory count — starts from 0 when line 4 is revealed
    if (elapsedSec > memRevealTime)
    {
        double me = elapsedSec - memRevealTime;
        double progress = me / ANIM_MEMORY_DURATION;
        if (progress > 1.0) progress = 1.0;
        memDisplayedKB = (int)(m_memoryTotalMB * 1024 * progress);
    }

    // Trigger beep on transition to COMPLETE
    if (newPhase >= ANIM_COMPLETE && m_animPhase < ANIM_COMPLETE && onBeep && !m_beepPlayed)
    {
        onBeep();
        m_beepPlayed = true;
        m_animCompleteTick = GetTickCount64();
    }
    m_animPhase = newPhase;
    m_biosLinesToShow = linesToShow;
    m_animMemoryDisplayedKB = memDisplayedKB;

    bool showPanel = (m_animPhase >= ANIM_COMPLETE);

    // Panel metrics (computed early for BIOS text width regardless of visibility)
    float panelW = screenW * PANEL_WIDTH_RATIO;
    if (panelW > PANEL_MAX_WIDTH) panelW = PANEL_MAX_WIDTH;
    float panelX = PANEL_MARGIN;
    float panelH = PANEL_FIXED_HEIGHT;
    float panelY = screenH - panelH - PANEL_MARGIN;

    m_lastPanelX = panelX;
    m_lastPanelY = panelY;
    m_lastPanelW = panelW;
    m_lastPanelH = panelH;

    // BIOS POST text (full width)
    float biosTextW = screenW - panelX - PANEL_MARGIN;
    float biosY = PANEL_MARGIN + 10.0f;
    float biosLineH = 24.0f;

    for (int li = 0; li < (int)m_biosLines.size() && li < linesToShow; li++)
    {
        std::wstring displayLine = m_biosLines[li];

        // Memory line — always show count-up (starts 0, no jump, no MB gap)
        if (li == 4)
        {
            wchar_t memBuf[64];
            swprintf_s(memBuf, L"Memory: %dK OK", m_animMemoryDisplayedKB);
            displayLine = memBuf;
        }

        DrawTextLine(d2d, dwrite, m_fontCollection.Get(), m_textFormatFooter.Get(),
            displayLine.c_str(), (UINT32)displayLine.size(),
            panelX + 4.0f, biosY, biosTextW, biosLineH,
            m_brushBios.Get());

        biosY += biosLineH + 2.0f;
    }

    // DOSBox watermark — bottom-right, fixed size, above version
    if (m_dosboxLogo)
    {
        D2D1_SIZE_F dbSize = m_dosboxLogo->GetSize();
        float aspect = dbSize.width / dbSize.height;
        float logoSz = 280.0f;
        float dbW = logoSz;
        float dbH = dbW / aspect;
        float dbX = screenW - dbW - PANEL_MARGIN;
        float dbY = screenH - dbH - LOGO_MARGIN - FOOTER_HEIGHT - LOGO_MARGIN;
        D2D1_RECT_F dbRect = { dbX, dbY, dbX + dbW, dbY + dbH };
        d2d->DrawBitmap(m_dosboxLogo.Get(), dbRect, 1.0f);
    }

    // Version bottom-right (always)
    {
        ComPtr<IDWriteTextLayout> verLayout;
        dwrite->CreateTextLayout(m_versionStr.c_str(), (UINT32)m_versionStr.size(), m_textFormatFooter.Get(),
            300.0f, FOOTER_HEIGHT, &verLayout);
        if (verLayout && m_fontCollection)
        {
            DWRITE_TEXT_RANGE fr = { 0, (UINT32)m_versionStr.size() };
            verLayout->SetFontCollection(m_fontCollection.Get(), fr);
        }
        if (verLayout)
        {
            DWRITE_TEXT_METRICS tm;
            verLayout->GetMetrics(&tm);
            d2d->DrawTextLayout(
                D2D1::Point2F(screenW - tm.width - LOGO_MARGIN, screenH - tm.height - LOGO_MARGIN),
                verLayout.Get(), m_brushFooter.Get());
        }
    }

    // Panel (dialog) — only after boot animation completes
    if (!showPanel || m_stack.empty()) return;

    auto& items = *m_stack.back().items;

    // Panel background
    D2D1_RECT_F panelBg = { panelX, panelY, panelX + panelW, panelY + panelH };
    d2d->FillRectangle(panelBg, m_brushBg.Get());

    // Panel outer frame
    float frameW = 2.0f;
    d2d->DrawRectangle(panelBg, m_brushFrame.Get(), frameW);
    D2D1_RECT_F innerFrame = { panelX + 4.0f, panelY + 4.0f, panelX + panelW - 4.0f, panelY + panelH - 4.0f };
    d2d->DrawRectangle(innerFrame, m_brushFrame.Get(), 1.0f);

    // Title bar
    float titleY = panelY + 8.0f;
    D2D1_RECT_F titleBg = { panelX + 8.0f, titleY, panelX + panelW - 8.0f, titleY + TITLE_HEIGHT };
    d2d->FillRectangle(titleBg, m_brushTitleBg.Get());

    ComPtr<IDWriteTextLayout> titleLayout;
    std::wstring wtitle(m_stack.back().title.begin(), m_stack.back().title.end());
    dwrite->CreateTextLayout(wtitle.c_str(), (UINT32)wtitle.size(), m_textFormatTitle.Get(),
        panelW - ITEM_INDENT * 2, TITLE_HEIGHT, &titleLayout);
    if (titleLayout)
    {
        if (m_fontCollection)
            { DWRITE_TEXT_RANGE fr = { 0, (UINT32)wtitle.size() }; titleLayout->SetFontCollection(m_fontCollection.Get(), fr); }
        DWRITE_TEXT_METRICS tm;
        titleLayout->GetMetrics(&tm);
        float tx = panelX + (panelW - tm.width) * 0.5f;
        d2d->DrawTextLayout(
            D2D1::Point2F(tx, titleY + 4.0f),
            titleLayout.Get(), m_brushWhite.Get());
    }

    // Item list — aligned to bottom, gap between title and items
    float itemAreaBottom = panelY + panelH - 8.0f - 8.0f;
    float listAvailable = itemAreaBottom - (titleY + TITLE_HEIGHT + 8.0f);
    int maxFit = (int)(listAvailable / ITEM_HEIGHT);
    if (maxFit < 1) maxFit = 1;
    if (maxFit > MAX_VISIBLE) maxFit = (int)MAX_VISIBLE;

    int itemCount = (int)items.size();
    int visibleCount = min(itemCount, maxFit);

    if (m_scrollOffset > itemCount - visibleCount)
        m_scrollOffset = itemCount - visibleCount;
    if (m_scrollOffset < 0) m_scrollOffset = 0;

    float listY = itemAreaBottom - visibleCount * ITEM_HEIGHT;

    for (int i = 0; i < visibleCount; i++)
    {
        int idx = m_scrollOffset + i;

        auto& item = items[idx];
        float iy = listY + i * ITEM_HEIGHT;

        if (idx == m_selected && item.action != MenuAction::NONE)
        {
            D2D1_RECT_F selRect = { panelX + 8.0f, iy, panelX + panelW - 8.0f, iy + ITEM_HEIGHT };
            d2d->FillRectangle(selRect, m_brushTitleText.Get());

            std::wstring wlabel(item.label.begin(), item.label.end());
            ComPtr<IDWriteTextLayout> itemLayout;
            auto textBrush = item.enabled ? m_brushSelected.Get() : m_brushDisabled.Get();
            dwrite->CreateTextLayout(wlabel.c_str(), (UINT32)wlabel.size(), m_textFormatItem.Get(),
                panelW - ITEM_INDENT * 3, ITEM_HEIGHT, &itemLayout);
            if (itemLayout)
            {
                if (m_fontCollection)
                {
                    DWRITE_TEXT_RANGE fr = { 0, (UINT32)wlabel.size() };
                    itemLayout->SetFontCollection(m_fontCollection.Get(), fr);
                }
                d2d->DrawTextLayout(
                    D2D1::Point2F(panelX + ITEM_INDENT, iy),
                    itemLayout.Get(), textBrush);
            }

            if (!item.values.empty())
            {
                std::wstring wval = L": " + std::wstring(item.values[item.currentValue].begin(), item.values[item.currentValue].end());
                ComPtr<IDWriteTextLayout> valLayout;
                dwrite->CreateTextLayout(wval.c_str(), (UINT32)wval.size(), m_textFormatItem.Get(),
                    panelW * 0.35f, ITEM_HEIGHT, &valLayout);
                if (valLayout)
                {
                    if (m_fontCollection)
                    {
                        DWRITE_TEXT_RANGE fr = { 0, (UINT32)wval.size() };
                        valLayout->SetFontCollection(m_fontCollection.Get(), fr);
                    }
                    DWRITE_TEXT_METRICS tm;
                    valLayout->GetMetrics(&tm);
                    d2d->DrawTextLayout(
                        D2D1::Point2F(panelX + panelW - tm.width - ITEM_INDENT, iy),
                        valLayout.Get(), m_brushSelected.Get());
                }
            }
        }
        else if (item.label.empty())
        {
            float sepY = iy + ITEM_HEIGHT * 0.5f;
            d2d->DrawLine(
                D2D1::Point2F(panelX + 20.0f, sepY),
                D2D1::Point2F(panelX + panelW - 20.0f, sepY),
                m_brushFooter.Get(), 1.0f);
            continue;
        }
        else
        {
            std::wstring wlabel(item.label.begin(), item.label.end());
            ComPtr<IDWriteTextLayout> itemLayout;
            dwrite->CreateTextLayout(wlabel.c_str(), (UINT32)wlabel.size(), m_textFormatItem.Get(),
                panelW - ITEM_INDENT * 3, ITEM_HEIGHT, &itemLayout);
            if (itemLayout)
            {
                if (m_fontCollection)
                {
                    DWRITE_TEXT_RANGE fr = { 0, (UINT32)wlabel.size() };
                    itemLayout->SetFontCollection(m_fontCollection.Get(), fr);
                }
                auto brush = item.enabled ? m_brushItemText.Get() : m_brushFooter.Get();
                d2d->DrawTextLayout(
                    D2D1::Point2F(panelX + ITEM_INDENT, iy),
                    itemLayout.Get(), brush);
            }

            if (!item.values.empty())
            {
                std::wstring wval = L": " + std::wstring(item.values[item.currentValue].begin(), item.values[item.currentValue].end());
                ComPtr<IDWriteTextLayout> valLayout;
                dwrite->CreateTextLayout(wval.c_str(), (UINT32)wval.size(), m_textFormatItem.Get(),
                    panelW * 0.35f, ITEM_HEIGHT, &valLayout);
                if (valLayout)
                {
                    if (m_fontCollection)
                    {
                        DWRITE_TEXT_RANGE fr = { 0, (UINT32)wval.size() };
                        valLayout->SetFontCollection(m_fontCollection.Get(), fr);
                    }
                    DWRITE_TEXT_METRICS tm;
                    valLayout->GetMetrics(&tm);
                    d2d->DrawTextLayout(
                        D2D1::Point2F(panelX + panelW - tm.width - ITEM_INDENT, iy),
                        valLayout.Get(), m_brushValueText.Get());
                }
            }
        }
    }

    // FileBrowser overlay (drawn on top of menu panel)
    m_fileBrowser.Render(d2d, dwrite, screenW, screenH);
}

int FrontendMenu::HitTest(float sx, float sy)
{
    if (!m_visible) return -1;
    if (sx < m_lastPanelX || sx > m_lastPanelX + m_lastPanelW) return -1;

    auto& items = *m_stack.back().items;
    float titleY = m_lastPanelY + 8.0f;
    float itemAreaBottom = m_lastPanelY + m_lastPanelH - 16.0f;
    float listAvailable = itemAreaBottom - (titleY + TITLE_HEIGHT + 8.0f);
    int maxFit = (int)(listAvailable / ITEM_HEIGHT);
    int visibleCount = min((int)items.size(), maxFit);
    float listY = itemAreaBottom - visibleCount * ITEM_HEIGHT;

    if (sy < listY) return -1;
    int idx = (int)((sy - listY) / ITEM_HEIGHT) + m_scrollOffset;
    if (idx < 0 || idx >= (int)items.size()) return -1;
    if (items[idx].label.empty() || !items[idx].enabled) return -1;
    return idx;
}

void FrontendMenu::SelectItem(int idx)
{
    if (!m_visible || idx < 0) return;
    auto& items = *m_stack.back().items;
    if (idx >= (int)items.size()) return;
    m_selected = idx;
    int visibleCount = (int)MAX_VISIBLE;
    if (m_selected < m_scrollOffset)
        m_scrollOffset = m_selected;
    if (m_selected >= m_scrollOffset + visibleCount)
        m_scrollOffset = m_selected - visibleCount + 1;
}

void FrontendMenu::HandlePointerMove(float sx, float sy)
{
    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.HandlePointerMove(sx, sy);
        return;
    }
    int idx = HitTest(sx, sy);
    if (idx >= 0) m_selected = idx;
}

void FrontendMenu::HandlePointerDown(float sx, float sy, unsigned btn)
{
    if (!m_visible) return;
    if (m_fileBrowser.IsVisible())
    {
        if (btn == 1)
            m_fileBrowser.HandlePointerDown(sx, sy);
        return;
    }
    // Click on FrontendMenu items
    if (btn == 1)
    {
        int idx = HitTest(sx, sy);
        if (idx >= 0)
        {
            m_selected = idx;
            OnConfirm();
        }
    }
}

void FrontendMenu::HandlePointerWheel(int delta)
{
    if (!m_visible) return;
    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.HandlePointerWheel(delta);
        return;
    }
}

void FrontendMenu::OnDPad(bool up)
{
    if (!m_visible) return;

    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.OnDPad(up);
        return;
    }

    auto& items = *m_stack.back().items;
    int count = (int)items.size();

    if (up)
    {
        do {
            m_selected--;
            if (m_selected < 0) m_selected = count - 1;
        } while (!items[m_selected].enabled || items[m_selected].label.empty());
    }
    else
    {
        do {
            m_selected++;
            if (m_selected >= count) m_selected = 0;
        } while (!items[m_selected].enabled || items[m_selected].label.empty());
    }

    int visibleCount = (int)MAX_VISIBLE;
    if (m_selected < m_scrollOffset)
        m_scrollOffset = m_selected;
    if (m_selected >= m_scrollOffset + visibleCount)
        m_scrollOffset = m_selected - visibleCount + 1;
}

void FrontendMenu::OnConfirm()
{
    if (!m_visible) return;

    // Route to FileBrowser if visible
    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.OnConfirm();
        return;
    }

    auto& items = *m_stack.back().items;
    if (m_selected < 0 || m_selected >= (int)items.size()) return;
    auto& item = items[m_selected];
    if (!item.enabled) return;

    switch (item.action)
    {
    case MenuAction::OPEN_FILE:
        spdlog::info("[FrontendMenu] OPEN_FILE -> FileBrowser.Open()");
        m_fileBrowser.Open();
        break;

    case MenuAction::OPEN_PUREMENU:
        if (onOpenPuremenu) onOpenPuremenu();
        break;

    case MenuAction::SETTINGS:
        m_stack.push_back({ "Settings", &m_settingsItems });
        m_selected = 0;
        m_scrollOffset = 0;
        break;

    case MenuAction::VIDEO:
        m_stack.push_back({ "Video", &m_videoItems });
        m_selected = 0;
        m_scrollOffset = 0;
        break;

    case MenuAction::AUDIO:
        m_stack.push_back({ "Audio", &m_audioItems });
        m_selected = 0;
        m_scrollOffset = 0;
        break;

    case MenuAction::TOGGLE_VALUE:
        if (!item.values.empty())
        {
            items[m_selected].currentValue =
                (item.currentValue + 1) % (int)item.values.size();

            // Push to core or frontend
            if (!item.optionKey.empty())
            {
                const char* newVal = item.values[item.currentValue].c_str();
                spdlog::info("[FrontendMenu] TOGGLE: {} = {}", item.optionKey, newVal);
                SettingsManager::SetOption(item.optionKey.c_str(), newVal);
                RetroCore::SetOptionValue(item.optionKey.c_str(), newVal);
                if (onOptionChanged)
                    onOptionChanged(item.optionKey.c_str(), newVal);
            }
        }
        break;

    case MenuAction::EXIT:
        m_visible = false;
        if (onExit) onExit();
        break;

    default:
        break;
    }
}

void FrontendMenu::OnBack()
{
    if (!m_visible) return;

    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.OnBack();
        return;
    }

    if (m_stack.size() > 1)
    {
        m_stack.pop_back();
        m_selected = 0;
        m_scrollOffset = 0;
    }
    // else: root menu — B does nothing (no hide, no exit)
}

void FrontendMenu::OnPageUp()
{
    if (!m_visible) return;
    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.OnPageUp();
        return;
    }
    // Scroll menu items by MAX_VISIBLE
    m_selected -= (int)MAX_VISIBLE;
    if (m_selected < 0) m_selected = 0;
    m_scrollOffset -= (int)MAX_VISIBLE;
    if (m_scrollOffset < 0) m_scrollOffset = 0;
}

void FrontendMenu::OnPageDown()
{
    if (!m_visible) return;
    if (m_fileBrowser.IsVisible())
    {
        m_fileBrowser.OnPageDown();
        return;
    }
    auto& items = *m_stack.back().items;
    m_selected += (int)MAX_VISIBLE;
    if (m_selected >= (int)items.size()) m_selected = (int)items.size() - 1;
    int visibleCount = (int)MAX_VISIBLE;
    if (m_selected >= m_scrollOffset + visibleCount)
        m_scrollOffset = m_selected - visibleCount + 1;
}
