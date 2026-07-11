# PUREMENU Theming System

## Overview

The PUREMENU (DOSBox Pure OSD) and FrontendMenu share a unified theme system driven by `dosbox-pure-settings.json`. Colors are loaded at startup and applied to all UI surfaces.

## Architecture

```
dosbox-pure-settings.json
  │
  ├─→ SettingsManager::Initialize()
  │     ├─ ThemeColors struct (D2D brushes: FrontendMenu, FileBrowser)
  │     └─ Core options map (dosbox_pure_menu_transparency, etc.)
  │
  ├─→ SettingsManager::ApplyThemeToPUREMENU()
  │     └─ DBPS_SetMenuColorsFromTheme() → DBP_BufferDrawing::BGCOL_*/COL_* (struct statics)
  │
  └─→ FrontendMenu::EnsureResources()
        └─ SettingsManager::GetTheme() → CreateSolidColorBrush()
```

## Settings File

**Location:**
- Xbox: `E:\dosbox\dosbox-pure-settings.json`
- Windows: `%TEMP%\dosbox-pure\dosbox-pure-settings.json`

**Format:** JSON with `//` and `/* */` comments supported (stripped before parse).

**Parser:** nlohmann/json (header-only, already in project via `uwp-xray-depot`).

## Theme Colors

All colors use `#RRGGBB` format (alpha defaults to FF/opaque). For alpha, use `#AARRGGBB`.

### Panel

| Key | Default | Role |
|-----|---------|------|
| `bg_panel` | `#0b002e` | Panel background (dark navy) |
| `bg_fullscreen` | `#000000` | Full-screen overlay |
| `frame` | `#761694` | Panel border/frame (purple-magenta) |
| `title_bg` | `#94164d` | Title bar background (maroon) |

### Text

| Key | Default | Role |
|-----|---------|------|
| `text_title` | `#fefefe` | Title text (near-white) |
| `text_normal` | `#aabbb9` | Normal item text (gray-green) |
| `text_value` | `#59caf9` | Value/accent text (cyan-blue) |
| `text_disabled` | `#74898e` | Disabled/footer text (dim gray-green) |
| `text_bios` | `#30f84c` | BIOS POST / directory names (neon green) |

### Selection

| Key | Default | Role |
|-----|---------|------|
| `selection_bg` | `#2c0087` | Selection highlight background (deep indigo) |
| `selection_text` | `#fefefe` | Selected item text (near-white) |

### FileBrowser Extras

| Key | Default | Role |
|-----|---------|------|
| `file_text` | `#d0d0d0` | File name text (light gray) |
| `overlay_alpha` | `0.55` | Overlay opacity (0.0–1.0) |

### PUREMENU Extras

| Key | Default | Role |
|-----|---------|------|
| `col_warn` | `#f8305b` | Warning text (bright pink) |
| `col_dim` | `#74898e` | Dim text |
| `col_white` | `#fefefe` | White text |

### Buttons

| Key | Default | Role |
|-----|---------|------|
| `bg_btn_off` | `#2c004a` | Button off state |
| `bg_btn_on` | `#761694` | Button on state |
| `bg_btn_hover` | `#94164d` | Button hover state |
| `col_btn_text` | `#fefefe` | Button text |

### Keyboard

| Key | Default | Role |
|-----|---------|------|
| `bg_key` | `#2c004a` | Key base color |
| `bg_key_hover` | `#761694` | Key hover |
| `bg_key_press` | `#f8305b` | Key pressed |
| `bg_key_held` | `#30f84c` | Key held |
| `bg_key_outline` | `#000000` | Key outline |
| `col_key_text` | `#fefefe` | Key text |

## Core Options

Options from `core_options` section are loaded via `SetOptionValue()` on game load.

```json
"core_options": {
    "dosbox_pure_menu_transparency": "70"
}
```

## Future Sections

```json
"shaders": {},   // Shader presets (not yet implemented)
"filters": {}    // Filter chains (not yet implemented)
```

## Files

| File | Role |
|------|------|
| `Content/SettingsManager.h` | ThemeColors struct, API declaration |
| `Content/SettingsManager.cpp` | JSON load/save, comment stripping, hex parsing |
| `Content/RetroCore.cpp` | Applies theme + options on init/load |
| `Content/FrontendMenu.cpp` | Reads ThemeColors for D2D brushes |
| `Content/FileBrowser.cpp` | Reads ThemeColors for D2D brushes |
| `local/dosbox-pure/dosbox_pure_osd.h` | Forked header — enum→statics inside struct (minimal patch) |
| `local/dosbox-pure/dosbox_pure_libretro.cpp` | Bridge: ThemeColors → PUREMENU statics |

## Submodule Fork

`dosbox_pure_osd.h` is copied from `extern/dosbox-pure/` to `local/dosbox-pure/` with a **minimal surgical patch**: the `enum EColors` block inside `struct DBP_BufferDrawing` is replaced by `static Bit32u` member declarations with the same names and default values.

**What changes:**
- Lines 36-44 of the original (`enum EColors : Bit32u { ... }`) → `static Bit32u` members with identical names/values

**What stays the same:**
- ALL ~100+ usage sites like `buf.BGCOL_HEADER`, `buf.COL_LINEBOX`, etc. — zero changes needed
- The entire rest of the 2700+ line file is byte-identical to the submodule

**Why `static Bit32u` inside the struct works:**
- `buf.BGCOL_HEADER` resolves to the struct's `static Bit32u` member (same name, same lookup path as the original enum)
- Static members are mutable at runtime → SettingsManager can write to them via the bridge
- Since this header is only included from one TU (`dosbox_pure_libretro.cpp`), no ODR issues

**Why NOT file-level statics (previous approach):**
- `buf.BGCOL_HEADER` wouldn't resolve to file-level statics (enum was removed from struct but `buf.` prefix expects a member)
- Would require renaming 54+ usage sites from `buf.XXX` → bare names
- Much more invasive, much harder to rebase against upstream

### Update Procedure

When updating the `dosbox-pure` submodule:

1. **Check** if `dosbox_pure_osd.h` changed upstream: `git diff HEAD~1 -- dosbox_pure_osd.h`
2. **If unchanged** → just copy to `local/dosbox-pure/`, apply the patch below. Done.
3. **If changed** → merge carefully:
   a. Copy the new upstream file to `local/dosbox-pure/`
   b. Find the `enum EColors : Bit32u` block inside `struct DBP_BufferDrawing` (around line 36)
   c. Replace the entire enum block with the static member block below
   d. If upstream added new enum members, add corresponding `static Bit32u` lines with the same default values
   e. **No other changes needed** — all `buf.BGCOL_*` / `buf.COL_*` usage automatically resolves to the new statics

### The Patch (copy-paste)

Replace the `enum EColors : Bit32u { ... };` block with:

```cpp
// Mutable theme colors — default values match upstream enum.
// Replaces enum EColors so SettingsManager can write to them at runtime.
// All code using buf.BGCOL_* / buf.COL_* resolves here (same names, same struct).
// 'inline' required for in-class initializer in C++14 (MSVC).
// Project uses /std:c++17 in vcxproj so this compiles directly.
static inline Bit32u BGCOL_SELECTION  = 0x117EB7;
static inline Bit32u BGCOL_SCROLL     = 0x093F5B;
static inline Bit32u BGCOL_MENU       = 0x1A1E20;
static inline Bit32u BGCOL_HEADER     = 0x582204;
static inline Bit32u BGCOL_STARTMENU  = 0xFF111111;
static inline Bit32u COL_MENUTITLE    = 0xFFFBD655;
static inline Bit32u COL_CONTENT      = 0xFFFFAB91;
static inline Bit32u COL_LINEBOX      = 0xFFFF7126;
static inline Bit32u COL_HIGHLIGHT    = 0xFFBDCDFB;
static inline Bit32u COL_NORMAL       = 0xFF4DCCF5;
static inline Bit32u COL_DIM          = 0xFF4B7A93;
static inline Bit32u COL_WHITE        = 0xFFFFFFFF;
static inline Bit32u COL_WARN         = 0xFFFF7126; // was COL_LINEBOX in enum
static inline Bit32u COL_HEADER       = 0xFF9ECADE;
static inline Bit32u BGCOL_BTNOFF     = 0x5F3B27;
static inline Bit32u BGCOL_BTNON      = 0xAB6037;
static inline Bit32u BGCOL_BTNHOVER   = 0x895133;
static inline Bit32u COL_BTNTEXT      = 0xFFFBC6A3;
static inline Bit32u BGCOL_KEY        = 0x5F3B27; // was BGCOL_BTNOFF in enum
static inline Bit32u BGCOL_KEYHOVER   = 0xAB6037; // was BGCOL_BTNON in enum
static inline Bit32u BGCOL_KEYPRESS   = 0xE46E2E;
static inline Bit32u BGCOL_KEYHELD    = 0xC9CB35;
static inline Bit32u BGCOL_KEYOUTLINE = 0x000000;
static inline Bit32u COL_KEYTEXT      = 0xFFF8EEE8;
```

### Bridge Function

In `dosbox_pure_libretro.cpp` (after `#include "dosbox_pure_osd.h"`):

```cpp
#include "Content/SettingsManager.h"
void DBPS_SetMenuColorsFromTheme(const ThemeColors& t)
{
    DBP_BufferDrawing::BGCOL_SELECTION  = t.selection_bg;
    DBP_BufferDrawing::BGCOL_MENU       = t.bg_panel;
    DBP_BufferDrawing::BGCOL_HEADER     = t.title_bg;
    // ... (full mapping in source, 24 colors total)
}
```

The `DBP_BufferDrawing::` prefix is required because the statics are struct members, not file-level.

## Comment Format

The settings file supports C-style comments:

```jsonc
{
    // This is a line comment
    "theme": {
        /* Block comment */
        "bg_panel": "#0b002e"
    }
}
```
