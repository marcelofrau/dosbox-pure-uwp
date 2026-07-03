# UX Capability

## Scope
User-facing graphical interface: scaling controls, overlay UI, ROM picker, virtual keyboard.

## Requirements

### Video scaling controls
- Toggle between pixel-perfect, integer, stretch, aspect-ratio
- Configurable via UI (initially hardcoded, eventually via RMLUI overlay)
- Option to show/HUD fps counter

### RMLUI overlay
- Integrate RMLUI (libRocket) as D2D/D3D overlay rendered on top of DOSBox framebuffer
- Overlay is semi-transparent, drawn after DOSBox frame, before Present()

### Visual ROM picker
- Browse file system via UWP StorageFolder APIs
- Show file list with icons per type (.zip/.dosz/.exe/.iso/.chd)
- Recent files list (persisted to LocalFolder)
- Search/filter

### Config UI
- Expose all ~52 core options in categorized UI
- Categories: General, Input, Performance, Video, System, Audio
- Toggle, slider, dropdown controls mapped to GET_VARIABLE/SET_VARIABLE
- Config override save/load

### Virtual keyboard
- Touch-friendly on-screen keyboard for DOS text input
- QWERTY layout + modifier keys (Ctrl, Alt, Shift)
- Triggers RETROK_* events in core
- Dismissable overlay

### Save state manager
- Visual grid of save slots per game
- Slot preview (screenshot from save state if available)
- Load/save/delete operations

## Affected Changes
- `video-enhancements`
- `rmlui-overlay`
