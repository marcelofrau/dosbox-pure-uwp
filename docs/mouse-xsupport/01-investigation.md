# Investigation — Current Mouse Input Code Map

## 1. Event wiring — `dosbox-uwp/App.cpp` / `App.h`

- `App.cpp:80-93` — `SetWindow()`: registers `PointerMoved` / `PointerPressed`
  / `PointerReleased` / `PointerWheelChanged` on `CoreWindow`, all guarded by
  `#ifdef MOUSE_SUPPORT`. Line 91: `window->PointerCursor = nullptr;` — hides
  the native OS cursor from app start (DOS/PUREMENU render their own cursor).
- `App.cpp:370-384` — `OnPointerMoved`: converts `CurrentPoint->Position` to
  normalized 0..1 coords **clamped to window bounds**:
  ```cpp
  float normX = pt.X / sender->Bounds.Width;
  if (normX < 0) normX = 0;
  if (normX > 1) normX = 1;
  if (normY < 0) normY = 0;
  if (normY > 1) normY = 1;
  ```
  This clamping is the root cause of the "mouse stops at screen edge" bug.
  Calls `m_main->SetMousePointerId()` + `m_main->OnPointerMove(normX, normY,
  pt.X, pt.Y)`.
- `App.cpp:386-408` — `OnPointerPressed`: reads
  `Properties->IsLeftButtonPressed/IsRightButtonPressed/IsMiddleButtonPressed`,
  calls `OnPointerDown(...)`; line 406 re-hides cursor.
- `App.cpp:410-428` — `OnPointerReleased`: reads `PointerUpdateKind` enum to
  determine which button released, calls `OnPointerUp(btn)` /
  `OnPointerRelease()`.
- `App.cpp:430-434` — `OnPointerWheelChanged`: reads
  `Properties->MouseWheelDelta`, calls `OnPointerWheel(delta)`.
- No `PointerEntered` / `PointerExited` handling anywhere.
- No `Windows::Devices::Input` or `Windows::Gaming::Input` namespace used in
  `App.cpp` (checked `using namespace` list at top of file). No `MouseDevice`
  reference anywhere.

## 2. Main loop / state routing — `dosbox_uwpMain.cpp` / `.h`

- `dosbox_uwpMain.cpp:1121-1146` — `OnPointerMove(nx, ny, px, py)`: if
  FrontendMenu visible, routes to menu and returns. Otherwise computes
  relative delta as `(int)(px - m_lastPointerPX)` / `(int)(py -
  m_lastPointerPY)` — **manual diff of absolute position**, not a raw HID
  delta. Calls `m_retroCore->SetMouseMove(relX, relY)` +
  `SetPointer(nx, ny, m_pointerDown)`.
- `dosbox_uwpMain.cpp:1148-1168` — `OnPointerDown(nx, ny, btn)`: routes to
  menu if visible, else calls `SetPointer(nx, ny, true)` +
  `SetMouseButton(btn, true)`.
- `dosbox_uwpMain.cpp:1170-1174` — `OnPointerUp(btn)`: calls
  `SetMouseButton(btn, false)`.
- `dosbox_uwpMain.cpp:1176-1181` — `OnPointerRelease()`: called when no
  buttons remain pressed; resets `m_pointerDown` and calls
  `SetPointer(m_pointerX, m_pointerY, false)`.
- `dosbox_uwpMain.cpp:1183-1192` — `OnPointerWheel(delta)`: routes to menu
  or calls `SetMouseWheel(delta)`.
- `dosbox_uwpMain.cpp:1194-1197` — `SetMousePointerId(id)`.
- `dosbox_uwpMain.cpp:588-706` — gamepad-simulated mouse mode
  (`m_gamepadMouseMode`, LB+RB+Select toggle) — separate feature, stick
  emulates relative mouse via `SetMouseMove`. Has idle-timeout heuristic
  keyed on `m_lastPointerTime` (real pointer activity suppresses virtual
  cursor for 500ms). Unrelated to the Xbox bug but shares the
  `SetMouseMove` entry point — needs to remain compatible with any change.

## 3. Bridge to core — `Content/RetroCore.cpp` / `.h`

- `RetroCore.cpp:285-289` — `SetMouseMove(int relX, int relY)` — **not**
  gated by `MOUSE_SUPPORT`, always compiled:
  ```cpp
  void RetroCore::SetMouseMove(int relX, int relY)
  {
      s_mouseRelX += relX;
      s_mouseRelY += relY;
  }
  ```
- `RetroCore.cpp:291-296` — `SetPointer(float x, float y, bool down)` — also
  not gated, stores absolute normalized position for the visual cursor:
  ```cpp
  void RetroCore::SetPointer(float x, float y, bool down)
  {
      s_ptrX = x;
      s_ptrY = y;
      s_ptrDown = down;
  }
  ```
- `RetroCore.cpp:298-319` — `#ifdef MOUSE_SUPPORT` block:
  `SetMouseButton(btn, down)`, `SetMouseWheel(delta)`, `GetPointer(short& mx,
  short& my)` (converts normalized `s_ptrX/Y` to signed 16-bit range for
  libretro's pointer device, used by `DBPS_GetMouse` for PUREMENU's cursor).
- `RetroCore.cpp:594-634` — `retro_input_state`, `RETRO_DEVICE_MOUSE` branch:
  ```cpp
  case RETRO_DEVICE_ID_MOUSE_X: return s_mouseRelX;
  case RETRO_DEVICE_ID_MOUSE_Y: return s_mouseRelY;
  #ifdef MOUSE_SUPPORT
  case RETRO_DEVICE_ID_MOUSE_LEFT: return s_mouseBtnLeft ? 1 : 0;
  case RETRO_DEVICE_ID_MOUSE_RIGHT: return s_mouseBtnRight ? 1 : 0;
  case RETRO_DEVICE_ID_MOUSE_MIDDLE: return s_mouseBtnMiddle ? 1 : 0;
  case RETRO_DEVICE_ID_MOUSE_WHEELUP: return (s_mouseWheel > 0) ? 1 : 0;
  case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: return (s_mouseWheel < 0) ? 1 : 0;
  #else
  case RETRO_DEVICE_ID_MOUSE_LEFT: return 0;
  case RETRO_DEVICE_ID_MOUSE_RIGHT: return 0;
  #endif
  ```
  Note: `s_mouseRelX/Y` reset-after-read behavior needs re-checking when
  implementing Phase 1 (must confirm core consumes/resets deltas correctly
  once fed from a different source).

## 4. Native bridge — `dosbox_pure_sta.cpp:17-34`

- `DBPS_GetMouse(short& mx, short& my, bool)` — calls
  `RetroCore::GetPointer(mx, my)` under `MOUSE_SUPPORT`, else zeros. Called
  from `dosbox_pure_libretro.cpp:1888` (core mouse dispatch) and
  `dosbox_pure_osd.h:220` (PUREMENU cursor rendering).

## 5. What was ruled out

- **`MOUSE_SUPPORT` macro**: defined unconditionally in `dosbox-uwp.vcxproj`
  for both Debug and Release, with no platform/configuration split — same
  binary code on Windows and Xbox. **Not** a compile-time guard disabling
  mouse on Xbox.
- **No Xbox-specific code path exists anywhere** for mouse. Grep for
  `Xbox`, `DeviceFamily`, `GameBar`, `GameInput`, `IsXbox`, `AnalyticsInfo`
  found nothing mouse-related (only unrelated gamepad/back-button/audio
  pacing notes).
- **Manifest** (`Package.appxmanifest`): declares `Windows.Xbox` +
  `Windows.Universal` target device families; capabilities list
  (`internetClient`, `internetClientServer`, `privateNetworkClientServer`,
  `codeGeneration`, `broadFileSystemAccess`, `runFullTrust`,
  `expandedResources`) has nothing HID/mouse-related, but also nothing that
  would explicitly block it. No `<Extensions>` block.
- **`MouseDevice`, `RawGameController`, `GameInput`,
  `Windows::Gaming::Input`, `PointerDeviceType`, `MouseCapabilities`**: zero
  usage anywhere in the app code (only `Windows::Gaming::Input::Gamepad^`
  for controller input in `SdlInput.cpp`, unrelated).
- **Docs / git history**: no prior note about Xbox mouse specifically in
  `docs/discoveries.md`, `DYNAREC_UWP.md`, or commit messages. Original
  mouse implementation commit (`cf7974e` "xaudio2 takeover + frame pacing +
  mouse input") is platform-agnostic.

## 6. What still needs empirical confirmation (Xbox hardware required)

- Does `CoreWindow::PointerMoved` fire at all on Xbox with a physical USB
  mouse connected?
- If it fires, is the position absolute-and-free, or does it get
  clamped/degraded compared to desktop?
- Does `CoreWindow::PointerPressed`/`Released` fire for button clicks?
- Does `Windows::Devices::Input::MouseCapabilities().MousePresent` return
  `true` on Xbox when a USB mouse is attached?

See `03-plan.md` Phase 0 for the exact logging/test steps.
