# FrontendMenu — RGUI-Style D2C Frontend for DOSBox Pure Unleashed UWP

## Overview

Custom D2D-rendered menu system for the UWP frontend. No external GUI libraries. Navigable by gamepad (D-pad + A/B). Visual style inspired by RetroArch RGUI and classic DOS VGA aesthetics.

## Design Goals

- **Zero dependencies** — uses already-linked D2D + DWrite
- **Gamepad-first** — all interactions work with D-pad + A/B/X/Y
- **VGA/DOS visual** — dark blue background, cyan bars, amber selection, monospace font
- **Layered over any content** — menu renders as D2D overlay on top of retro framebuffer or splash cube
- **Extensible** — menu tree defined in data, not code; easy to add items/pages

## Render Pipeline Integration

```
Render():
  ├── D3D11 ClearRenderTargetView + ClearDepthStencilView
  ├── [if menu visible && !core loaded]
  │     └── Sample3DSceneRenderer::Render()          ← spinning cube (background)
  ├── [if core loaded]
  │     └── RetroScreenRenderer::Render()            ← D2D framebuffer
  ├── [else]
  │     └── Sample3DSceneRenderer::Render()          ← spinning cube (splash)
  ├── [if menu visible]
  │     └── FrontendMenu::Render()                   ← D2D overlay [BeginDraw/EndDraw]
  ├── SampleFpsTextRenderer::Render()                ← D2D HUD
  └── [splash] D2D cursor crosshair                  ← D2D overlay
```

When menu is visible:
- If no core loaded: cube rotates in background, menu overlay on top
- If core loaded: retro framebuffer visible behind semi-transparent menu

## Visual Design

```
┌───────────────────────────────────┐
│ [TITLE BAR: DOSBox Pure Unleashed]│  ← cyan fill, white text
│                                   │
│  ▸ Continue Game                  │  ← amber highlight on selected
│  ▸ Open Game                      │
│  ────────────────────             │  ← separator line
│  ▸ Settings                       │
│  ▸ Controls                       │
│  ▸ Save/Load State                │
│  ────────────────────             │
│  ▸ About                          │
│  ▸ Close Menu                     │
│                                   │
│  [Hint: D-Pad Navigate A Select]  │  ← dim text footer
└───────────────────────────────────┘
```

### Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `COL_BG` | `#00002A` | Fullscreen background |
| `COL_BG_ALPHA` | `#AA00002A` | Semi-transparent overlay |
| `COL_TITLE_BG` | `#00AAAA` | Top bar background (cyan) |
| `COL_TITLE_TEXT` | `#FFFFFF` | Title text |
| `COL_SELECTED` | `#AA5500` | Selected item highlight (amber) |
| `COL_ITEM_TEXT` | `#00AAAA` | Item label text |
| `COL_ITEM_DISABLED` | `#555555` | Disabled item text |
| `COL_VALUE_TEXT` | `#00FF00` | Value/toggle text (green) |
| `COL_SEPARATOR` | `#555555` | Separator line |
| `COL_FOOTER` | `#555555` | Footer hint text |

### Typography

- **Font:** Consolas (monospace, built-in Windows)
- **Sizes:** Title 18px, items 16px, footer 14px
- **Fallback:** DWrite `Consolas` → system monospace

## Menu Tree

### Main Page

```
DOSBox Pure Unleashed
─────────────────────
  Continue Game                    [enabled when core loaded]
  Open Game                        [→ onOpenFile callback]
  Recent Games                     [future: dynamic list]
─────────────────────
  Settings                         [→ Settings page]
  Controls                         [→ onOpenPuremenu (mapper)]
  Save/Load State                  [→ State page, enabled when core loaded]
─────────────────────
  About                            [→ onAbout callback]
  Close Menu                       [hides menu]
```

### Settings Page

```
Settings
─────────────────────
  Video                            [→ Video page]
  Audio                            [→ Audio page]
  Core Options                     [→ onOpenPuremenu (settings)]
─────────────────────
  Back                             [→ main page]
```

### Video Page

```
Video
─────────────────────
  Fullscreen: [On/Off]             [cycle value]
  Aspect Ratio: [Auto/4:3/16:9]   [cycle value]
─────────────────────
  Back                             [→ Settings page]
```

### Audio Page

```
Audio
─────────────────────
  Volume: [0-100]                  [future: slider or cycle]
─────────────────────
  Back                             [→ Settings page]
```

### Save/Load State Page

```
Save/Load State                    [enabled when core loaded]
─────────────────────
  Save State                       [→ save to current slot]
  Load State                       [→ load from current slot]
  Slot: [1/2/3/4/5]               [cycle value]
─────────────────────
  Back                             [→ main page]
```

### About Page

```
About
─────────────────────
  DOSBox Pure Unleashed
  Version: x.y.z
  Built: date
  ────────────────────
  Based on dosbox-pure libretro core
  ────────────────────
  Back                             [→ main page]
```

## Data Model

```cpp
enum class MenuAction {
    NONE,            // label-only, no interaction
    SUBMENU,         // push child page
    BACK,            // pop page
    CONTINUE_GAME,   // hide menu, resume core
    OPEN_FILE,       // trigger FileOpenPicker
    OPEN_PUREMENU,   // open PUREMENU (mapper or settings)
    TOGGLE_VALUE,    // cycle current value
    ABOUT,           // show About page
    EXIT             // close menu
};

struct MenuItem {
    std::string label;
    MenuAction action;
    std::vector<MenuItem> children;   // used if action == SUBMENU
    std::vector<std::string> values;  // used if action == TOGGLE_VALUE
    int currentValue = 0;             // index into values
    bool enabled = true;              // false = grayed, unselectable
};
```

## Navigation

| Input | Action |
|-------|--------|
| D-pad Up | Select previous item (wrap) |
| D-pad Down | Select next item (wrap) |
| A / Button_A | Confirm selection |
| B / Button_B | Back to previous page |
| Start / Escape | Toggle menu (close if open) |
| Mouse click on item | Select + confirm |

## Scroll Behavior

When menu has > visible items (max ~20), scroll offset follows selection:
- Selection goes above `m_scrollOffset` + visibleCount – 3 → `m_scrollOffset++`
- Selection goes below `m_scrollOffset` → `m_scrollOffset--`
- Items rendered starting at `m_scrollOffset`

## Implementation Files

| File | Purpose |
|------|---------|
| `Content/FrontendMenu.h` | MenuItem struct, MenuAction enum, FrontendMenu class declaration |
| `Content/FrontendMenu.cpp` | BuildTree, Render, input handlers (OnDPad, OnConfirm, OnBack), action dispatch |
| `dosbox_uwpMain.h` | FrontendMenu member, ToggleMenu(), IsMenuVisible() |
| `dosbox_uwpMain.cpp` | Render integration, Update input routing, constructor init |
| `App.cpp` | F10/Back toggle, Wire callbacks |

## Future Work

1. **Recent games** — persist JSON list via `ApplicationData::LocalSettings`
2. **Settings persistence** — save frontend prefs (fullscreen, volume, aspect) to JSON file
3. **Core config overlay** — expose core options directly (instead of opening PUREMENU)
4. **DOS bitmap font** — replace Consolas with .FON/.FNT bitmap font for authentic look
5. **Textured cube** — replace solid-color cube faces with DOSBox logo texture
