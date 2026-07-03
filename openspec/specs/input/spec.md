# Input Capability

## Scope
All input devices routed from UWP/SDL to dosbox-pure core via libretro callbacks.

## Requirements

### Keyboard (RETRO_DEVICE_KEYBOARD)
- Map `Windows::System::VirtualKey` to `RETROK_*` constants (defined in `libretro.h`)
- Buffer must hold all keys 0..RETROK_LAST (324), not just 256
- Handle: letters, numbers, Enter, Escape, Backspace, Tab, arrows, Shift/Ctrl/Alt (left+right), F1-F12, Numpad, punctuation
- F1: resolve picker vs core key conflict (e.g. F1=menu, picker on R3/Select)

### Gamepad (RETRO_DEVICE_JOYPAD)
- Map SDL_GameController buttons to `RETRO_DEVICE_ID_JOYPAD_*`:
  - A/B/X/Y, Select, Start, Guide
  - D-pad up/down/left/right
  - L1/R1 (shoulder), L2/R2 (triggers)
  - L3/R3 (thumbstick clicks)

### Analog (RETRO_DEVICE_ANALOG)
- Left stick → `RETRO_DEVICE_INDEX_ANALOG_LEFT`, axes LX/LY
- Right stick → `RETRO_DEVICE_INDEX_ANALOG_RIGHT`, axes RX/RY
- Deadzone handling from core options

### Mouse (RETRO_DEVICE_MOUSE)
- Relative X/Y movement
- Left/Right/Middle button state
- Scroll wheel
- Mouse capture toggle (for FPS games: hide cursor, relative-only)

### Pointer (RETRO_DEVICE_POINTER)
- Absolute normalized X/Y (0..0x7fff)
- Pressed state
- Pointer count (multi-touch optional)

## Affected Changes
- `fix-keyboard-input`
- `fix-gamepad-input`
- `dbps-mouse`
