# Tasks: fix-keyboard-input

- [x] Directories exist (`openspec/changes/fix-keyboard-input/`)

### Task 1: Create full VirtualKey → RETROK_ lookup
- [ ] Implement static lookup table or switch in `dosbox_uwpMain.cpp`
- [ ] Cover all keys: letters, numbers, navigation, modifiers, function keys, numpad, punctuation
- [ ] Include `#include "libretro.h"` for RETROK_ definitions
- [ ] Verify each mapping against `libretro.h` constants (do not hardcode raw numbers)

### Task 2: Expand s_keyboardState array
- [ ] Change `s_keyboardState[256]` to `s_keyboardState[RETROK_LAST]`
- [ ] Change `s_keyboardStatePrevious[256]` similarly
- [ ] Update all memset/loop bounds from 256 to RETROK_LAST
- [ ] Verify `RETROK_LAST` is 324 in `libretro.h`

### Task 3: Map letter keys (A-Z) and number keys (0-9)
- [ ] Verify ASCII pass-through is correct (no change needed if VK matches ASCII)
- [ ] Add safety cast/clamp to ensure values are within RETROK_LAST range

### Task 4: Map navigation keys
- [ ] Enter → RETROK_RETURN
- [ ] Escape → RETROK_ESCAPE
- [ ] Backspace → RETROK_BACKSPACE
- [ ] Tab → RETROK_TAB
- [ ] Space → RETROK_SPACE

### Task 5: Map modifiers
- [ ] LeftShift → RETROK_LSHIFT
- [ ] RightShift → RETROK_RSHIFT
- [ ] LeftCtrl → RETROK_LCTRL
- [ ] RightCtrl → RETROK_RCTRL
- [ ] LeftAlt → RETROK_LALT
- [ ] RightAlt → RETROK_RALT

### Task 6: Map F1-F12 (resolve F1 conflict)
- [ ] F1..F12 → RETROK_F1..RETROK_F12
- [ ] Change F1 handling in `App.cpp`: remove FileOpenPicker on F1
- [ ] Move picker trigger to R3 or Select button
- [ ] Ensure F1 reaches `OnKeyDown` in `dosbox_uwpMain.cpp`

### Task 7: Map Numpad keys
- [ ] Numpad0..Numpad9 → RETROK_KP0..RETROK_KP9
- [ ] Decimal → RETROK_KP_PERIOD
- [ ] Divide → RETROK_KP_DIVIDE
- [ ] Multiply → RETROK_KP_MULTIPLY
- [ ] Subtract → RETROK_KP_MINUS
- [ ] Add → RETROK_KP_PLUS
- [ ] NumpadEnter → RETROK_KP_ENTER

### Task 8: Map punctuation / symbol keys
- [ ] Semicolon, colon, comma, period, slash, etc.
- [ ] Bracket keys, backslash, tilde, minus, equals
- [ ] Quote keys

### Task 9: Change F1 handling
- [ ] `App.cpp`: remove `if (key == VirtualKey::F1)` picker logic
- [ ] Add picker trigger in `dosbox_uwpMain.cpp` on R3 or Select press
- [ ] Wire new picker invocation via CoreDispatcher or similar

### Task 10: Build and test with Puremenu navigation
- [ ] Build Release|x64 (0 errors)
- [ ] Test: arrow keys navigate Puremenu
- [ ] Test: Enter selects, Escape backs out
- [ ] Test: F1 opens core menu
- [ ] Test: R3/Select opens file picker
- [ ] Test: Ctrl+F5 quick save works
- [ ] Test: Tab/Space in DOS prompts
