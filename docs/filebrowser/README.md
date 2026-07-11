# File Browser — In-App Explorer

Replace UWP `FileOpenPicker` with a custom D2D file browser overlay,
styled like a DOS dialog, rendered on top of the BIOS boot screen.

## Status

| Task | Status |
|------|--------|
| Documentation (this file) | DONE |
| `Package.appxmanifest` capabilities | TODO |
| `FileBrowser.h` | TODO |
| `FileBrowser.cpp` | TODO |
| `FrontendMenu` integration | TODO |
| `App.cpp` — Ctrl+L fallback | TODO |
| `dosbox_uwpMain.cpp` input routing | TODO |
| Build test | TODO |

---

## Architecture

### Loading Pipeline (unchanged)

```
FileBrowser::SelectFile(fullPath)
  -> dosbox_uwpMain::QueueLoadRom(fullPath, {})
  -> ProcessPendingLoad()
  -> RetroCore::LoadGame(path, {})
  -> retro_load_game() via UWP VFS (CreateFile2FromAppW)
  -> Core initializes DOSBox engine
  -> retro_run() starts producing frames
```

No changes to the core. FileBrowser only provides a path.

### File Access

With `broadFileSystemAccess` + `runFullTrust` capabilities:

- `GetLogicalDrives()` returns real drives (Desktop + Xbox)
- `FindFirstFileExFromAppW("C:\\*", ...)` accesses any directory
- Same mechanism as RetroArch UWP (`platform_uwp.c:188`)

### Rendering Pipeline

```
d2dContext->BeginDraw()
  |-> Loading screen (if loadingActive)
  |-> FrontendMenu::RenderFullScreen() (BIOS text + menu panel)
  |-> FileBrowser::Render() (overlay modal, if visible)
  |-> Cursor overlay
d2dContext->EndDraw()
```

FileBrowser draws AFTER FrontendMenu, on top of everything (except cursor).

---

## Layout

### Panel Dimensions

- Width: 60% of screen, max 700px, min 400px
- Height: 55% of screen, max 500px, min 300px
- Centered horizontally, anchored to bottom-third vertically

### ASCII Layout

```
+------------------------------------------------------+
| FILE BROWSER                    C:\Games\dos\>       |
+------------------------------------------------------+
|                                                      |
| >[DIR] ..                                            |
|  [DIR] DOOM                                          |
|  [DIR] DUKE3D                                        |
|  [DIR] QUAKE                                         |
|  [   ] DOOM.ZIP          <- marquee if long          |
|  [   ] DUKE3D.COM                                    |
|  [   ] SETUP.EXE                                     |
|  [   ] GAME.ISO                                      |
|  [   ] VERY_LONG_FILENAME_HERE.EXE <- marquee ->     |
|                                                      |
+------------------------------------------------------+
| A:Enter  B:Back  Y:Home  LB/RB:Page                  |
+------------------------------------------------------+
```

### Colors (matching FrontendMenu palette)

| Element | Color | Brush |
|---------|-------|-------|
| Panel background | `#0B002E` (dark navy) | `m_brushBg` |
| Outer frame | `#761694` (purple) | `m_brushFrame` |
| Inner frame | `#761694` 1px | `m_brushFrame` |
| Title bar bg | `#94164D` (magenta) | `m_brushTitleBg` |
| Title text | `#FFFFFF` white | `m_brushWhite` |
| Path text | `#59CAF9` cyan | `m_brushPath` |
| Selected item bg | `#2C0087` (deep purple) | `m_brushSelectedBg` |
| Selected item text | `#FEFEFE` white | `m_brushSelected` |
| Directory text | `#30F84C` green | `m_brushDir` |
| File text | `#AABBB9` gray | `m_brushFile` |
| Footer text | `#74898E` dim gray | `m_brushFooter` |
| `[DIR]` prefix | `#30F84C` green | `m_brushDir` |
| `[   ]` prefix | `#74898E` dim | `m_brushFooter` |

### Typography

All fonts: VCR OSD Mono (custom, loaded from `Assets/Fonts/VCR_OSD_MONO_1.001.ttf`)

| Element | Size | Weight |
|---------|------|--------|
| Title bar | 33px (27px small screen) | Bold |
| File items | 27px (22px small screen) | Normal |
| Path bar | 22px (18px small screen) | Normal |
| Footer | 22px (18px small screen) | Normal |

### Item Height

- Item row: 30px
- Title bar: 36px
- Footer: 22px
- Panel padding: 8px all sides

---

## Marquee Effect

When file name text width > available width:

1. `PushAxisAlignedClip` to clip text to item bounds
2. Text starts left-aligned, static for 1.0s pause
3. Scrolls right-to-left at 60px/sec
4. Pauses 1.0s at end before resetting
5. Cycle: pause -> scroll -> pause -> reset

When text fits: no animation, static left-aligned text.

```cpp
// Pseudo-code
float availableW = panelW - ITEM_INDENT * 3 - iconWidth;
float textWidth = measureTextWidth(name);
if (textWidth > availableW) {
    float scrollDist = textWidth - availableW + 20.0f; // 20px padding
    double elapsed = (now - m_marqueeStart) / 1000.0;
    float cycle = 1.0f + scrollDist / 60.0f + 1.0f; // pause + scroll + pause
    float phase = fmod(elapsed, cycle);
    float offset = 0.0f;
    if (phase < 1.0f) offset = 0.0f;                          // pause start
    else if (phase < 1.0f + scrollDist/60.0f)
        offset = (phase - 1.0f) * 60.0f;                      // scrolling
    else offset = scrollDist;                                  // pause end

    d2d->PushAxisAlignedClip(clipRect);
    d2d->DrawTextLayout(Point2F(x - offset, y), layout, brush);
    d2d->PopAxisAlignedClip();
}
```

---

## Gamepad Input

Same scheme as FrontendMenu:

| Button | Action |
|--------|--------|
| DPad Up/Down | Move selection (wrap around) |
| A (Confirm) | Enter directory OR select file |
| B (Back) | Go to parent directory OR close browser |
| Y (Home) | Jump to LocalFolder |
| LB/RB | Page up/down through list |
| Left Stick Click | Same as A |
| Menu/Start | Close browser |

### Navigation Rules

- **Root level**: shows drives + LocalFolder
  - Select drive -> enters `C:\`, `D:\`, etc.
  - Select LocalFolder -> enters app data dir
- **Inside directory**: shows `..` + subdirs + files (filtered)
  - Select `..` -> parent directory
  - Select directory -> enters it
  - Select file -> `onFileSelected(path)` callback
- **Going to parent of drive root** (e.g. `C:\` -> `..`): stays at root drive list

---

## File Filtering

Only show files matching supported extensions:

```
.zip .dosz .exe .com .bat .iso .chd .cue .img .ima .vhd .conf
```

Directories are always shown (no filter).

---

## Directory Scanning

Uses Win32 `FindFirstFileExFromAppW` + `FindNextFileW` (same as RetroArch VFS):

```cpp
// Pattern: "<path>\\*"
WIN32_FIND_DATAW findData;
HANDLE hFind = FindFirstFileExFromAppW(pattern, FindExInfoStandard,
    &findData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
```

Sort order: directories first (alphabetical), then files (alphabetical).

---

## Fallback: Ctrl+L -> FileOpenPicker

`Ctrl+L` opens the original UWP `FileOpenPicker` as a forced fallback.
This bypasses the in-app browser entirely.

Implementation:
- `App.cpp`: `OnKeyDown` / `OnAcceleratorKeyActivated` checks for Ctrl+L
- Calls existing `OpenFilePicker()` directly
- Available always, even when menu is not visible

---

## Files

| File | Action | Purpose |
|------|--------|---------|
| `Package.appxmanifest` | Modify | Add `broadFileSystemAccess`, `runFullTrust`, `expandedResources` |
| `Content/FileBrowser.h` | **New** | Class declaration |
| `Content/FileBrowser.cpp` | **New** | Implementation: scan, render, input, marquee |
| `Content/FrontendMenu.h` | Modify | Add `#include`, `FileBrowser m_fileBrowser` member |
| `Content/FrontendMenu.cpp` | Modify | Overlay rendering, input routing, OPEN_FILE -> browser |
| `App.cpp` | Modify | Ctrl+L fallback, disable default picker on OPEN_FILE |
| `App.h` | Modify | Add `OpenFilePickerFallback()` if needed |
| `dosbox_uwpMain.cpp` | Modify | Input routing when filebrowser visible |

---

## API Reference

### FileBrowser Public Interface

```cpp
class FileBrowser {
public:
    // Lifecycle
    void Open();                           // Show browser at root
    void Close();                          // Hide browser
    bool IsVisible() const;                // Check if shown

    // Rendering (called from FrontendMenu::RenderFullScreen)
    void Render(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite,
                float screenW, float screenH);

    // Input (routed from dosbox_uwpMain::OnKeyEvent via FrontendMenu)
    void OnDPad(bool up);                  // Navigate list
    void OnConfirm();                      // Select item
    void OnBack();                         // Parent dir or close
    void OnPageUp();                       // LB - page up
    void OnPageDown();                     // RB - page down
    void OnHome();                         // Y - jump to LocalFolder
    void OnPointerMove(float sx, float sy);// Mouse hover
    int  HitTest(float sx, float sy);      // Mouse click target (-1 = miss)

    // Callbacks
    std::function<void(const std::wstring&)> onFileSelected;
    std::function<void()> onBeep;          // Navigation beep

private:
    void ScanDirectory(const std::wstring& path);
    bool PassesExtensionFilter(const std::wstring& name);
    void EnsureResources(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite,
                         float screenW, float screenH);
    void DrawMarqueeText(ID2D1DeviceContext* d2d, IDWriteFactory* dwrite,
                         const wchar_t* text, UINT32 len,
                         float x, float y, float maxW, float h,
                         ID2D1Brush* brush, IDWriteFontCollection* fc);
};
```

### FileEntry

```cpp
struct FileEntry {
    std::wstring name;    // Display name (filename or dir name)
    bool isDir;           // true = directory, false = file
};
```

---

## Notes

- `expandedResources` is optional for file browser but needed for
  Game Mode on Xbox (exclusive CPU cores + >=4GB memory).
  Without it, emulator runs in "app mode" with fewer resources.
- On Xbox with `broadFileSystemAccess`, `GetLogicalDrives()` returns
  real drive letters (confirmed by user testing RetroArch on Xbox).
- The browser does NOT use WinRT Storage APIs — pure Win32 FindFirstFile
  matching the RetroArch VFS approach.
- Extension filter is case-insensitive.
- `..` entry is always first when not at root drive level.
