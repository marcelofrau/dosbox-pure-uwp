# Design — DBPS_GetMouse with Real Mouse Events

## Event Capture
In `App::SetWindow`, register `CoreWindow::PointerPressed`, `PointerMoved`, `PointerReleased`, and `PointerWheelChanged` handlers.

Store mouse state in a shared struct:
```cpp
struct MouseState {
    int abs_x, abs_y;           // absolute pixel position
    int rel_dx, rel_dy;         // accumulated relative delta
    int scroll;                 // accumulated vertical scroll
    bool buttons[3];            // left, right, middle
    bool captured;              // relative capture mode
};
```

## Coordinate Conversion
- **Absolute → Pointer (0..0x7fff):** `ptr_x = (abs_x * 0x7fff) / (window_width - 1)`
- **Relative delta → MOUSE:** store delta; reset after each `retro_input_state` read
- **Scroll:** accumulate `PointerWheelChanged` delta; report button 3 (up) / 4 (down)

## DBPS_GetMouse(short& mx, short& my, bool osd)
- If `osd`: return absolute pointer position (normalized to 0..0x7fff)
- If `!osd`: return accumulated relative delta, zero counters after read
- Return button mask (bit 0=left, 1=right, 2=middle)

## retro_input_state Integration
- `RETRO_DEVICE_MOUSE` class: `RETRO_DEVICE_ID_MOUSE_X/Y` → rel_dx/rel_dy; `RETRO_DEVICE_ID_MOUSE_LEFT/RIGHT/MIDDLE` → buttons[0..2]; `RETRO_DEVICE_ID_MOUSE_WHEELUP/DOWN` → scroll (consumed on read)
- `RETRO_DEVICE_POINTER` class: `RETRO_DEVICE_ID_POINTER_X/Y` → abs normalized; `RETRO_DEVICE_ID_POINTER_PRESSED` → any button down

## Mouse Capture Toggle
- Middle-click or F8 toggles `captured` flag
- Captured: `CoreCursor` hidden, only relative motion reported
- Uncaptured: cursor visible, absolute pointer reported
- Escape key auto-releases capture

## Thread Safety
- `std::mutex` guards the `MouseState` struct
- Lock on write (event handlers) and read (retro_input_state / DBPS_GetMouse)
