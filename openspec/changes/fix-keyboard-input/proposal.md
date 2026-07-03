# Fix keyboard input: map VirtualKey to RETROK_ constants

## Problem
`OnKeyEvent()` in `dosbox_uwpMain.cpp` passes raw `Windows::System::VirtualKey` codes (0x0D, 0x1B, 0x26...) to `s_keyboardState[]` and `RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK`. The core ignores these because it expects `RETROK_*` constants from `libretro.h`. Only A-Z and 0-9 work by accident because their VirtualKey values happen to match ASCII which matches the `RETROK_*` values.

Secondary bug: `s_keyboardState` is declared `bool[256]` but `RETROK_LAST` is 324 — keys above 256 wrap or overflow.

## Scope
- dosbox_uwpMain.cpp (keyboard state array + lookup)
- RetroCore.cpp/.h (keyboard callback wiring)

## Spec
`openspec/specs/input/spec.md` — Keyboard section

## Solution
1. Build a static lookup table mapping each `VirtualKey` enum to its `RETROK_*` constant
2. Expand `s_keyboardState` from 256 to `RETROK_LAST` (324)
3. Resolve F1 conflict: move UWP FileOpenPicker trigger to R3/Select, pass F1 through to core
4. Leave A-Z and 0-9 as-is (correct by accident)
