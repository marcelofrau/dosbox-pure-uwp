# Tasks: fix-gamepad-input

- [x] Directories exist (`openspec/changes/fix-gamepad-input/`)

### Task 1: Add gamepad state arrays to SdlInput
- [ ] Add `m_joypadState[16]` for 16 libretro button ids
- [ ] Add `m_joypadStatePrevious[16]` for edge detection
- [ ] Add `m_analogState[4]` for left X/Y, right X/Y
- [ ] Add `m_deadzone` configurable property (default 8000)
- [ ] Initialize all to zero

### Task 2: Implement SDL gamepad polling in PollEvents()
- [ ] Create `PollEvents()` method in SdlInput
- [ ] Get `SDL_GameController*` from existing controller tracking
- [ ] Loop button mapping array, call `SDL_GameControllerGetButton()` for each
- [ ] Store previous state, update current state
- [ ] Call `PollEvents()` from `dosbox_uwpMain.cpp` Update() loop

### Task 3: Map A/B/X/Y, Select, Start, Guide buttons
- [ ] SDL_CONTROLLER_BUTTON_A → JOYPAD_B (id 0)
- [ ] SDL_CONTROLLER_BUTTON_B → JOYPAD_Y (id 1)
- [ ] SDL_CONTROLLER_BUTTON_X → JOYPAD_X (id 9)
- [ ] SDL_CONTROLLER_BUTTON_Y → JOYPAD_A (id 8)
- [ ] SDL_CONTROLLER_BUTTON_BACK → JOYPAD_SELECT (id 2)
- [ ] SDL_CONTROLLER_BUTTON_START → JOYPAD_START (id 3)

Note: Nintendo vs Xbox A/B/X/Y layout may differ. Use Xbox layout (A=bottom, B=right, X=left, Y=top). Core can swap via options.

### Task 4: Map D-pad
- [ ] SDL_CONTROLLER_BUTTON_DPAD_UP → JOYPAD_UP (id 4)
- [ ] SDL_CONTROLLER_BUTTON_DPAD_DOWN → JOYPAD_DOWN (id 5)
- [ ] SDL_CONTROLLER_BUTTON_DPAD_LEFT → JOYPAD_LEFT (id 6)
- [ ] SDL_CONTROLLER_BUTTON_DPAD_RIGHT → JOYPAD_RIGHT (id 7)

### Task 5: Map L1/R1/L2/R2 shoulder buttons and triggers
- [ ] SDL_CONTROLLER_BUTTON_LEFTSHOULDER → JOYPAD_L (id 10)
- [ ] SDL_CONTROLLER_BUTTON_RIGHTSHOULDER → JOYPAD_R (id 11)
- [ ] Map analog triggers as digital: L2 → JOYPAD_L2 (id 12), R2 → JOYPAD_R2 (id 13)
- [ ] Alternatively: derive digital press from axis value (>deadzone threshold)

### Task 6: Map L3/R3 thumbstick clicks
- [ ] SDL_CONTROLLER_BUTTON_LEFTSTICK → JOYPAD_L3 (id 14)
- [ ] SDL_CONTROLLER_BUTTON_RIGHTSTICK → JOYPAD_R3 (id 15)

### Task 7: Implement analog stick polling with deadzone
- [ ] Read SDL_CONTROLLER_AXIS_LEFTX, LEFT Y, RIGHTX, RIGHTY
- [ ] Implement `ApplyDeadzone(int16_t value)`:
  - `if (abs(value) < deadzone) return 0;`
  - Clamp return to ±0x7FFF
- [ ] Store in m_analogState[0..3]

### Task 8: Update retro_input_state JOYPAD case
- [ ] Replace keyboard-state fallback with real gamepad state read
- [ ] Guard by port == 0 (player 1 only for now)
- [ ] Return 0 for out-of-range id

### Task 9: Implement RETRO_DEVICE_ANALOG in retro_input_state
- [ ] Handle LEFT/RIGHT index
- [ ] Return m_analogState mapped to X/Y
- [ ] Ensure return type is `int16_t`

### Task 10: Build and test with Puremenu navigation via gamepad
- [ ] Build Release|x64 (0 errors)
- [ ] Test: D-pad and left stick navigate Puremenu
- [ ] Test: A confirms, B backs out
- [ ] Test: Start opens menu, Select toggles something
- [ ] Test: analog sticks work in DOS game (e.g. joystick calibration)
- [ ] Test: deadzone prevents drift
- [ ] Test: two controllers (if connected) — only P1 active for now
