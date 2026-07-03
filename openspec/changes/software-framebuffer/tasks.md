# Tasks: GET_CURRENT_SOFTWARE_FRAMEBUFFER

## Task 1: Implement GET_CURRENT_SOFTWARE_FRAMEBUFFER handler
- In `retro_env()` switch, add `case 55`.
- Guard: null check `data` pointer → return 0.
- Guard: `s_lastFrame.pitch == 0` or `s_lastFrame.data == nullptr` → return 0.
- Fill `retro_framebuffer` struct fields.
- Return 1.

## Task 2: Fill retro_framebuffer struct correctly
- `data` → `s_lastFrame.data`
- `width` → `s_lastFrame.width`
- `height` → `s_lastFrame.height`
- `pitch` → `s_lastFrame.pitch`
- `access_flags` → `RETRO_MEMORY_ACCESS_WRITE`
- `memory_flags` → `RETRO_MEMORY_TYPE_CACHED_WRITEBACK`

## Task 3: Coordinate with video frame mutex
- Check if `s_lastFrame` is protected by mutex.
- If render thread reads `s_lastFrame` concurrently, acquire mutex before returning pointer.
- If single-threaded (core thread does all work), no mutex needed.
- Add `OutputDebugStringA` note confirming thread safety analysis.

## Task 4: Handle edge cases
- **No frame yet**: `s_lastFrame.data == nullptr` after boot, before first `video_cb`.
- **HW frame**: `pitch == 0` sentinel from core.
- **Size changes**: width/height change between frames — always read current fields.
- **Null fb pointer from core**: defensive return 0.

## Task 5: Build and test with Puremenu overlay
- Build with MSBuild.
- Launch. Press F12 to open Puremenu in-game.
- Verify OSD text appears composited over game framebuffer.
- If no OSD: check debug log for cmd 55 calls and return value.
