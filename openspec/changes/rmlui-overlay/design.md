# RMLUI Overlay — Design

## Dependency
- Add RMLUI headers + lib to solution
- Include in `.vcxproj` with proper UWP compat flags

## RmlOverlay class
- Init RMLUI with custom D2D render interface
- Lifecycle: CreateDeviceResources, Render, ReleaseDeviceDependentResources

## Rendering pipeline
1. RetroScreenRenderer draws DOSBox frame to D2D bitmap
2. RmlOverlay renders RMLUI documents on top (separate D2D layer)
3. Both composited via D2D device context

## Input routing
- When overlay active: keyboard/gamepad/mouse → RMLUI
- When overlay inactive: input → DOSBox emulation
- Toggle: F1 or Back/Select gamepad button

## RML documents

### mainmenu.rml
- Toolbar overlay with buttons: Scale Mode, Save/Load, Config, Keyboard
- Semi-transparent, top edge

### picker.rml
- Visual file browser
- ROM list with thumbnails
- Filters by type (DOS/Windows/ demos)
- Recent folder shortcut

### config.rml
- Categorized options UI
- Reads from core options map (GET_VARIABLE bridge)
- Writes via retro_set_variable or core options map

### keyboard.rml
- On-screen QWERTY layout
- Modifier keys (Shift, Ctrl, Alt)
- Auto-show when physical keyboard absent (Xbox)

### saves.rml
- Save slot grid (0-9 per game)
- Load / Save / Delete per slot
- Screenshot thumbnail per slot
- Autosave slot highlighted

## Config bridge
- Core options stored in `std::unordered_map<string, string>` in RetroCore
- RMLUI reads via bridge → GET_VARIABLE
- RMLUI writes via bridge → core options map + retro_set_variable
