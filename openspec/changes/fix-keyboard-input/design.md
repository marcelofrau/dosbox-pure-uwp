# Design: fix-keyboard-input

## VirtualKey → RETROK_ Mapping Table

Implement as `static const int vkToRetroK[256]` initialized at file scope, or a `switch` in the keyboard callback. Latter preferred for clarity.

### Letters (A-Z) and Numbers (0-9)
| VirtualKey | RETROK_ | Notes |
|---|---|---|
| 0x41..0x5A (A-Z) | RETROK_a..RETROK_z | ASCII match, no mapping needed |
| 0x30..0x39 (0-9) | RETROK_0..RETROK_9 | ASCII match, no mapping needed |

### Navigation & Control
| VirtualKey | value | RETROK_ | value |
|---|---|---|---|
| Enter | 0x0D | RETROK_RETURN | 13 |
| Escape | 0x1B | RETROK_ESCAPE | 27 |
| Backspace | 0x08 | RETROK_BACKSPACE | 8 |
| Tab | 0x09 | RETROK_TAB | 9 |
| Space | 0x20 | RETROK_SPACE | 32 |

### Arrow Keys
| VirtualKey | value | RETROK_ | value |
|---|---|---|---|
| Up | 0x26 | RETROK_UP | 273 |
| Down | 0x28 | RETROK_DOWN | 274 |
| Left | 0x25 | RETROK_LEFT | 276 |
| Right | 0x27 | RETROK_RIGHT | 275 |

### Modifiers
| VirtualKey | value | RETROK_ | value |
|---|---|---|---|
| LeftShift | 0xA0 | RETROK_LSHIFT | 304 |
| RightShift | 0xA1 | RETROK_RSHIFT | 303 |
| LeftControl | 0xA2 | RETROK_LCTRL | 306 |
| RightControl | 0xA3 | RETROK_RCTRL | 305 |
| LeftAlt | 0xA4 | RETROK_LALT | 308 |
| RightAlt | 0xA5 | RETROK_RALT | 307 |

### Function Keys (F1-F12)
| VirtualKey | value | RETROK_ | value |
|---|---|---|---|
| F1 | 0x70 | RETROK_F1 | 282 |
| F2 | 0x71 | RETROK_F2 | 283 |
| F3 | 0x72 | RETROK_F3 | 284 |
| F4 | 0x73 | RETROK_F4 | 285 |
| F5 | 0x74 | RETROK_F5 | 286 |
| F6 | 0x75 | RETROK_F6 | 287 |
| F7 | 0x76 | RETROK_F7 | 288 |
| F8 | 0x77 | RETROK_F8 | 289 |
| F9 | 0x78 | RETROK_F9 | 290 |
| F10 | 0x79 | RETROK_F10 | 291 |
| F11 | 0x7A | RETROK_F11 | 292 |
| F12 | 0x7B | RETROK_F12 | 293 |

### Numpad
| VirtualKey | value | RETROK_ | value |
|---|---|---|---|
| Numpad0 | 0x60 | RETROK_KP0 | 288? Verify |
| Numpad1-9 | 0x61..0x69 | RETROK_KP1..RETROK_KP9 | 257..265 |
| Decimal | 0x6E | RETROK_KP_PERIOD | 266 |
| Divide | 0x6F | RETROK_KP_DIVIDE | 267 |
| Multiply | 0x6A | RETROK_KP_MULTIPLY | 268 |
| Subtract | 0x6D | RETROK_KP_MINUS | 269 |
| Add | 0x6B | RETROK_KP_PLUS | 270 |
| Enter | 0x?? | RETROK_KP_ENTER | 271 |

### Punctuation / Symbols
Map via ASCII where VirtualKey matches printable char. For keys without direct VirtualKey (e.g. on non-US layouts), defer to platform default or skip.

## Array Size Change

```cpp
// Before:
static bool s_keyboardState[256];
static bool s_keyboardStatePrevious[256];

// After:
#include "libretro.h"  // defines RETROK_LAST = 324
static bool s_keyboardState[RETROK_LAST];
static bool s_keyboardStatePrevious[RETROK_LAST];
```

Update all loops/memset that reference `256` for keyboard state to use `RETROK_LAST`.

## F1 Conflict Resolution

Current: App.cpp F1 handler opens FileOpenPicker. Core never sees F1.

New:
- Move picker trigger from F1 to R3 (right thumbstick click) OR Select button  
- F1 passes through to core normally
- Update App.cpp OnKeyDown to not intercept F1
- Update dosbox_uwpMain.cpp to map F1 → RETROK_F1

## Keyboard Callback Wiring

In `RetroCore.cpp`, `retro_keyboard_event()` already reads from `s_keyboardState[]`. No change needed there — just ensure the state array is populated with correct RETROK_ values.

## Files Changed
- `dosbox-uwp/dosbox_uwpMain.cpp` — mapping table, array size, flow
- `dosbox-uwp/Content/RetroCore.cpp` — include `libretro.h`, RETROK_LAST usage
- `dosbox-uwp/App.cpp` — F1 handling change
