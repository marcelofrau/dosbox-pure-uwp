# Design: fix-gamepad-input

## Button Mapping: SDL → RETRO_DEVICE_ID_JOYPAD

### Face Buttons
| SDL_GameControllerButton | RETRO_DEVICE_ID_JOYPAD_ |
|---|---|
| SDL_CONTROLLER_BUTTON_A | RETRO_DEVICE_ID_JOYPAD_A |
| SDL_CONTROLLER_BUTTON_B | RETRO_DEVICE_ID_JOYPAD_B |
| SDL_CONTROLLER_BUTTON_X | RETRO_DEVICE_ID_JOYPAD_X |
| SDL_CONTROLLER_BUTTON_Y | RETRO_DEVICE_ID_JOYPAD_Y |

### D-Pad
| SDL_GameControllerButton | RETRO_DEVICE_ID_JOYPAD_ |
|---|---|
| SDL_CONTROLLER_BUTTON_DPAD_UP | RETRO_DEVICE_ID_JOYPAD_UP |
| SDL_CONTROLLER_BUTTON_DPAD_DOWN | RETRO_DEVICE_ID_JOYPAD_DOWN |
| SDL_CONTROLLER_BUTTON_DPAD_LEFT | RETRO_DEVICE_ID_JOYPAD_LEFT |
| SDL_CONTROLLER_BUTTON_DPAD_RIGHT | RETRO_DEVICE_ID_JOYPAD_RIGHT |

### Shoulder + Triggers
| SDL_GameControllerButton | RETRO_DEVICE_ID_JOYPAD_ |
|---|---|
| SDL_CONTROLLER_BUTTON_LEFTSHOULDER | RETRO_DEVICE_ID_JOYPAD_L |
| SDL_CONTROLLER_BUTTON_RIGHTSHOULDER | RETRO_DEVICE_ID_JOYPAD_R |
| SDL_CONTROLLER_BUTTON_LEFTSTICK | RETRO_DEVICE_ID_JOYPAD_L3 |
| SDL_CONTROLLER_BUTTON_RIGHTSTICK | RETRO_DEVICE_ID_JOYPAD_R3 |

Note: L2/R2 triggers (SDL_CONTROLLER_BUTTON_LEFTTRIGGER/RIGHTTRIGGER) — in libretro these are typically `RETRO_DEVICE_ID_JOYPAD_L2`/`R2` which are analog-trigger-aware. If RETRO_DEVICE_ID_JOYPAD_L2 is not defined, map to L/R and use analog axis values for pressure.

### System Buttons
| SDL_GameControllerButton | RETRO_DEVICE_ID_JOYPAD_ |
|---|---|
| SDL_CONTROLLER_BUTTON_BACK | RETRO_DEVICE_ID_JOYPAD_SELECT |
| SDL_CONTROLLER_BUTTON_START | RETRO_DEVICE_ID_JOYPAD_START |
| SDL_CONTROLLER_BUTTON_GUIDE | Unmapped (or system menu) |

### Expected Button IDs
```cpp
#define RETRO_DEVICE_ID_JOYPAD_B     0
#define RETRO_DEVICE_ID_JOYPAD_Y     1
#define RETRO_DEVICE_ID_JOYPAD_SELECT 2
#define RETRO_DEVICE_ID_JOYPAD_START 3
#define RETRO_DEVICE_ID_JOYPAD_UP    4
#define RETRO_DEVICE_ID_JOYPAD_DOWN  5
#define RETRO_DEVICE_ID_JOYPAD_LEFT  6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT 7
#define RETRO_DEVICE_ID_JOYPAD_A     8
#define RETRO_DEVICE_ID_JOYPAD_X     9
#define RETRO_DEVICE_ID_JOYPAD_L     10
#define RETRO_DEVICE_ID_JOYPAD_R     11
#define RETRO_DEVICE_ID_JOYPAD_L2    12
#define RETRO_DEVICE_ID_JOYPAD_R2    13
#define RETRO_DEVICE_ID_JOYPAD_L3    14
#define RETRO_DEVICE_ID_JOYPAD_R3    15
```

## Analog Stick Mapping

| SDL_GameControllerAxis | RETRO index | RETRO axis ID | 
|---|---|---|
| SDL_CONTROLLER_AXIS_LEFTX | RETRO_DEVICE_INDEX_ANALOG_LEFT | RETRO_DEVICE_ID_ANALOG_X |
| SDL_CONTROLLER_AXIS_LEFTY | RETRO_DEVICE_INDEX_ANALOG_LEFT | RETRO_DEVICE_ID_ANALOG_Y |
| SDL_CONTROLLER_AXIS_RIGHTX | RETRO_DEVICE_INDEX_ANALOG_RIGHT | RETRO_DEVICE_ID_ANALOG_X |
| SDL_CONTROLLER_AXIS_RIGHTY | RETRO_DEVICE_INDEX_ANALOG_RIGHT | RETRO_DEVICE_ID_ANALOG_Y |
| SDL_CONTROLLER_AXIS_TRIGGERLEFT | RETRO_DEVICE_INDEX_ANALOG_LEFT | RETRO_DEVICE_ID_ANALOG_Y (or treat as L2 press) |
| SDL_CONTROLLER_AXIS_TRIGGERRIGHT | RETRO_DEVICE_INDEX_ANALOG_RIGHT | RETRO_DEVICE_ID_ANALOG_Y (or treat as R2 press) |

### Deadzone
- Default deadzone: 8000 (SDL default is 8192 for Xbox controllers)
- Configurable via core options (deadzone 0..32767)
- Apply before converting to libretro range:
```
if (abs(value) < deadzone) value = 0;
// Scale remaining to ±0x7FFF
```

### Libretro analog range
- libretro expects `int16_t` with range −0x7FFF to +0x7FFF
- SDL `Sint16` is already −32768..32767 → clamp to −0x7FFF

## State Arrays

Add to `SdlInput` class:

```cpp
// Joystick buttons (16 standard libretro buttons)
static const int JOYPAD_BUTTONS = 16;
static const int ANALOG_AXES = 4; // LEFT_X, LEFT_Y, RIGHT_X, RIGHT_Y

bool m_joypadState[JOYPAD_BUTTONS];  // per-frame state
bool m_joypadStatePrevious[JOYPAD_BUTTONS];  // for edge detection
int16_t m_analogState[ANALOG_AXES];  // per-frame analog values
```

## PollEvents Implementation

```cpp
void SdlInput::PollEvents()
{
    SDL_GameController* controller = GetGameController(); // existing
    if (!controller) return;

    // Poll buttons
    for (int b = 0; b < JOYPAD_BUTTONS; b++)
    {
        m_joypadStatePrevious[b] = m_joypadState[b];
        m_joypadState[b] = SDL_GameControllerGetButton(controller, s_buttonMap[b]) != 0;
    }

    // Poll analog axes
    m_analogState[0] = ApplyDeadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
    m_analogState[1] = ApplyDeadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));
    m_analogState[2] = ApplyDeadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
    m_analogState[3] = ApplyDeadzone(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));
}
```

Where `s_buttonMap` is a static array mapping libretro button index → `SDL_GameControllerButton`.

## retro_input_state Changes

In `RetroCore.cpp`:

```cpp
case RETRO_DEVICE_JOYPAD:
    // Read from SdlInput gamepad state, not keyboard
    {
        auto& input = SdlInput::Get();
        if (port == 0 && id < 16)
            return input.m_joypadState[id] ? 1 : 0;
    }
    return 0;

case RETRO_DEVICE_ANALOG:
    {
        auto& input = SdlInput::Get();
        int analogIdx = (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) ? 0 : 2;
        if (id == RETRO_DEVICE_ID_ANALOG_X)
            return input.m_analogState[analogIdx];
        else if (id == RETRO_DEVICE_ID_ANALOG_Y)
            return input.m_analogState[analogIdx + 1];
    }
    return 0;
```

Also update existing `RETRO_DEVICE_KEYBOARD` case if it was incorrectly handling JOYPAD queries (current code reads keyboard state for all device types — must be scoped).

## Files Changed
- `dosbox-uwp/SdlInput.h` — add m_joypadState, m_analogState, deadzone config, ApplyDeadzone()
- `dosbox-uwp/SdlInput.cpp` — implement PollEvents(), ApplyDeadzone(), mapping arrays
- `dosbox-uwp/Content/RetroCore.cpp` — update retro_input_state for JOYPAD + ANALOG
