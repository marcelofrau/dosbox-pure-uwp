# Mouse Input on Xbox — Investigation & Plan

## Problem statement

Native USB mouse input is not working when the app runs on Xbox (console),
even though the mouse pipeline (`App.cpp` → `dosbox_uwpMain.cpp` →
`RetroCore.cpp` → `DBPS_GetMouse` → dosbox-pure core) works fine on Windows
desktop via VS2022.

A **related bug** was also identified during investigation, observable today
on Windows desktop without needing an Xbox: in mouselook-style DOS games
(e.g. Blood and other FPS titles), when the OS mouse cursor reaches the edge
of the screen, the in-game view stops turning — the classic "cursor clamped
at screen border" symptom.

## Root cause (shared by both problems)

The app currently derives mouse motion exclusively from
`Windows::UI::Core::CoreWindow::PointerMoved`, which reports **absolute
cursor position** (clamped to the window bounds). Relative motion deltas fed
into `RETRO_DEVICE_ID_MOUSE_X/Y` are computed manually as a frame-to-frame
diff of that absolute position (`px - m_lastPointerPX`).

This is the **desktop pointer/cursor model** — fine for touch/pen/mouse used
as a screen-space cursor, but:

- When the physical OS cursor hits the screen edge, the absolute position
  stops changing, so the computed diff becomes `0` even though the user is
  still physically moving the mouse. This breaks continuous mouselook.
- On Xbox, physical USB mouse HID events may not be routed through
  `CoreWindow::PointerMoved` at all in the same way as on desktop — Xbox
  console shell/navigation may intercept or degrade pointer routing for
  fullscreen exclusive UWP apps. This has **not yet been empirically
  confirmed** — see `01-investigation.md` and `03-plan.md` Phase 0.

The standard UWP API for **raw relative mouse input** (delta-based, no
cursor position, no clamping, no dependency on a visible OS pointer) is
`Windows::Devices::Input::MouseDevice::GetForCurrentView()->MouseMoved`,
which exposes `args->MouseDelta.X/Y`. This is the API used by FPS-style
games for "mouselook" and is expected to fix both problems simultaneously.

## Docs in this folder

| File | Purpose |
|------|---------|
| `01-investigation.md` | Full code map (file:line) of current mouse handling, what was ruled out, what needs empirical confirmation on Xbox hardware |
| `02-architecture.md` | Proposed dual-pipeline architecture: absolute position (visual cursor) + raw delta (motion) |
| `03-plan.md` | Phased execution plan with checkboxes, including a Windows-desktop-testable phase (1.5) before requiring Xbox hardware |

## Status

Investigation complete. Implementation not started. Waiting for access to
Xbox hardware to execute Phase 0 (diagnostic logging) of `03-plan.md`.
Phase 1.5 (Windows desktop MouseDevice validation) can be done without Xbox
hardware and should be attempted first.
