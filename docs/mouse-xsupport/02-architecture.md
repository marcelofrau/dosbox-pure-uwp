# Proposed Architecture — Dual Mouse Pipeline

## Goal

Fix two related symptoms with one architectural change:

1. USB mouse not working on Xbox.
2. Mouselook stops turning when the OS cursor hits the screen edge on
   Windows desktop (affects Blood and other DOS FPS titles).

## Design: two parallel pipelines, one for position, one for motion

```
┌─────────────────────────────┐        ┌──────────────────────────────────┐
│ CoreWindow::PointerMoved     │        │ MouseDevice::GetForCurrentView()  │
│ (absolute position, clamped) │        │   ->MouseMoved (raw HID delta)    │
└──────────────┬───────────────┘        └────────────────┬───────────────────┘
               │                                          │
               ▼                                          ▼
   normX/normY (0..1, clamped)                 args->MouseDelta.X/Y
               │                                          │
               ▼                                          ▼
   RetroCore::SetPointer(nx, ny, down)      RetroCore::SetMouseMove(dx, dy)
               │                                          │
               ▼                                          ▼
   PUREMENU visual cursor position          RETRO_DEVICE_ID_MOUSE_X/Y
   (GetPointer / DBPS_GetMouse)             (in-DOS mouse motion / mouselook)
```

- **Visual cursor position** (used by PUREMENU's own on-screen cursor
  rendering via `DBPS_GetMouse` → `GetPointer`) keeps coming from
  `CoreWindow::PointerMoved`'s absolute, normalized position. This is fine
  for a UI cursor — it should stay within screen bounds, that's expected UX
  for a menu pointer.
- **In-DOS mouse motion** (`RETRO_DEVICE_ID_MOUSE_X/Y`, what actually drives
  mouselook / cursor movement inside DOS games) switches to come from
  `MouseDevice::MouseMoved`'s `MouseDelta.X/Y` — a true relative delta with
  no screen-space concept, no clamping, works continuously in any direction
  regardless of where a system cursor is.

## Conflict resolution

Both `PointerMoved` and `MouseMoved` can fire for the same physical mouse
motion. To avoid double-counting delta:

- `dosbox_uwpMain::OnPointerMove(nx, ny, px, py)` (fed by `CoreWindow`)
  **stops computing `relX/relY` from position diff** and **stops calling
  `SetMouseMove`**. It only calls `SetPointer(nx, ny, ...)` — position for
  the visual cursor.
- New method `dosbox_uwpMain::OnRawMouseDelta(int dx, int dy)` (fed by
  `MouseDevice::MouseMoved`) becomes the **sole caller** of
  `RetroCore::SetMouseMove(dx, dy)`.
- `m_lastPointerPX/PY` diff tracking in `OnPointerMove` can be removed once
  this migration is complete (no longer needed for motion — still useful if
  a fallback path is kept for platforms where `MouseDevice` isn't available,
  TBD in Phase 1).

## Buttons — unchanged for now, contingency documented

Mouse buttons (`SetMouseButton`) keep coming from
`CoreWindow::PointerPressed/Released` initially, since click events are
discrete (not position-dependent) and there is no evidence yet that they're
broken on Xbox. `MouseDevice` does not expose button state directly — it's
a motion-only API.

**If** Phase 0 diagnostics (see `03-plan.md`) reveal that `PointerPressed`/
`Released` also fail to fire on Xbox for a physical USB mouse, this
architecture needs a second contingency branch — do not implement
speculatively; investigate alternatives at that point (e.g. checking if
`PointerPoint` state can be polled another way, or whether the failure mode
is specific to position-dependent events only).

## Visual cursor edge case

If Xbox testing (Phase 0) reveals that `PointerMoved`'s absolute position is
*also* unreliable/degraded on Xbox (e.g. frozen, quantized, or simply never
fires), the visual cursor position will need to be derived instead by
accumulating the raw `MouseDevice` delta internally into a virtual
`(virtualCursorX, virtualCursorY)` state, clamped to 0..1, and feeding that
into `SetPointer`. This is a fallback, not the default plan — implement only
if empirically justified.

## Compatibility with existing gamepad-simulated mouse mode

`dosbox_uwpMain.cpp:588-706` implements a stick-based virtual mouse
(`m_gamepadMouseMode`) that also calls `SetMouseMove`. This must keep
working unchanged — it's an independent input source feeding the same sink
(`RetroCore::SetMouseMove` accumulates `s_mouseRelX/Y` regardless of caller).
No conflict expected since gamepad-mouse-mode and raw HID mouse motion are
mutually exclusive in practice (idle-timeout heuristic already handles
suppressing virtual cursor when real pointer activity is detected — same
heuristic should also consider raw `MouseDevice` activity, not just
`CoreWindow` pointer activity, once implemented).
