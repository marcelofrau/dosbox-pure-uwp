# Fix gamepad input: map SDL buttons to RETRO_DEVICE_JOYPAD/ANALOG

## Problem
SDL gamepad buttons are tracked in `SdlInput` but never mapped to libretro JOYPAD device IDs. `retro_input_state` for JOYPAD reads keyboard state instead of gamepad state. Analog sticks return 0 (stub implementation). D-pad, shoulder buttons, triggers, thumbstick clicks all unmapped.

## Scope
- SdlInput.cpp/.h (gamepad state, polling, mapping)
- RetroCore.cpp (retro_input_state JOYPAD + ANALOG cases)

## Spec
`openspec/specs/input/spec.md` — Gamepad + Analog sections

## Solution
1. Map each `SDL_GameControllerButton` to `RETRO_DEVICE_ID_JOYPAD_*` 
2. Map `SDL_GameControllerAxis` to `RETRO_DEVICE_ANALOG` (left/right stick)
3. Build `m_joypadState[16]` and `m_analogState[4]` in `PollEvents()`
4. Update `retro_input_state` to read real gamepad state
5. Apply deadzone for analog axes
