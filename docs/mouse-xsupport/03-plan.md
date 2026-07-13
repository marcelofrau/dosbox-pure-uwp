# Execution Plan — Xbox Mouse Support

Status legend: `[ ]` not started · `[~]` in progress · `[x]` done

## Phase 0 — Diagnostic instrumentation (requires Xbox hardware)

Goal: confirm exactly where the input chain breaks on Xbox before changing
any architecture. Do not skip this — avoids implementing speculative fixes.

- [ ] Add `spdlog::info` logging (per `AGENTS.md` logging convention — no
      `OutputDebugStringA` for new code) at:
  - `App::OnPointerMoved` — log position + delta once per N calls (avoid log
    spam)
  - `App::OnPointerPressed` / `OnPointerReleased` — log button + state
  - `App::OnPointerWheelChanged` — log delta
  - App init (near `SetWindow()`): log
    `Windows::Devices::Input::MouseCapabilities().MousePresent`
- [ ] Add temporary `PointerEntered` / `PointerExited` handlers (log-only,
      not wired to any game logic) to see if the system recognizes the
      pointer entering the window area at all on Xbox.
- [ ] Build, deploy to Xbox (dev mode), connect via xb-xray (`nc <ip> 9000`)
      to observe logs in real time without an attached debugger.
- [ ] With a physical USB mouse connected to the Xbox:
  1. Confirm `MousePresent` is `true`.
  2. Move the mouse — does `PointerMoved` fire? How many times? Does the
     reported position vary continuously, get clamped, or stay frozen?
  3. Click left/right/middle buttons — does `PointerPressed`/`Released`
     fire?
  4. Scroll wheel — does `PointerWheelChanged` fire?
- [ ] Record results in this file (append a "Phase 0 results" section below)
      before proceeding to Phase 1.

## Phase 1 — Implement `MouseDevice::MouseMoved` (raw delta) in parallel

Only proceed once Phase 0 results are known (results may change scope,
e.g. if `PointerMoved` doesn't fire at all vs. fires-but-clamped are very
different situations, though the same fix likely applies to both).

- [ ] `App.cpp` `SetWindow()`: register
      `Windows::Devices::Input::MouseDevice::GetForCurrentView()->MouseMoved
      += ref new TypedEventHandler<MouseDevice^, MouseEventArgs^>(this,
      &App::OnRawMouseMoved);`
- [ ] New handler `App::OnRawMouseMoved(MouseDevice^ sender, MouseEventArgs^
      args)`: reads `args->MouseDelta.X/Y`, calls
      `m_main->OnRawMouseDelta(args->MouseDelta.X, args->MouseDelta.Y)`.
- [ ] New method `dosbox_uwpMain::OnRawMouseDelta(int dx, int dy)`: routes to
      menu if visible (TBD: does FrontendMenu need raw delta too, or only
      absolute position from `PointerMoved`? Likely only absolute — menu
      navigation is cursor-based), otherwise calls
      `m_retroCore->SetMouseMove(dx, dy)`.
- [ ] Modify `dosbox_uwpMain::OnPointerMove` to **stop** calling
      `SetMouseMove` (remove the `relX/relY` diff calculation and its call);
      keep only the `SetPointer(nx, ny, ...)` call for visual cursor
      position.
- [ ] Verify `RetroCore::SetMouseMove` / the `RETRO_DEVICE_ID_MOUSE_X/Y`
      read-and-reset semantics still behave correctly with the new caller
      (check if `s_mouseRelX/Y` needs resetting after `retro_input_state`
      reads it — re-verify current behavior first, don't assume).

## Phase 1.5 — Validate on Windows desktop (no Xbox required)

This phase can be executed and validated **today**, without Xbox hardware,
and should be done before deploying to Xbox — it validates the core
architecture change with a much faster iteration loop.

- [ ] Build and run on Windows desktop (VS2022 debug).
- [ ] Load a DOS FPS game with mouselook (e.g. Blood).
- [ ] Test: move the mouse continuously toward one screen edge and keep
      pushing past it (as you would in any FPS to keep turning). Confirm the
      in-game view **keeps turning** instead of stopping when the OS cursor
      hits the window/screen border.
- [ ] Confirm PUREMENU's own cursor (when menu is open) still tracks
      correctly using the unchanged absolute-position path.
- [ ] Confirm mouse buttons (shoot, etc.) still work normally.
- [ ] If this works: strong signal the architecture is correct, proceed
      confidently to Xbox deployment. If it doesn't fully fix the edge-clamp
      issue on desktop, the architecture needs revisiting **before**
      spending an Xbox test cycle on it.

## Phase 2 — Button contingency (conditional on Phase 0 findings)

Only execute if Phase 0 showed `PointerPressed`/`Released` failing on Xbox.

- [ ] Investigate whether `PointerPoint::GetCurrentPoint(pointerId)` polled
      from another entry point (e.g. per-frame in the render loop) gives
      more reliable button state than the event-based approach.
- [ ] If that also fails, research `GameInput` API (newer Microsoft API,
      requires GDK) as a last resort — larger scope, needs separate
      evaluation (SDK availability, licensing/build impact) before committing.
- [ ] Do not implement this phase speculatively — only if empirically
      justified by Phase 0.

## Phase 3 — Visual cursor edge case (conditional)

Only execute if Phase 0 showed `PointerMoved` absolute position is
unreliable/frozen/quantized on Xbox (not just motion-tracking-for-DOS, but
the visual cursor position itself).

- [ ] Implement virtual cursor position by accumulating raw
      `MouseDevice` deltas internally (clamped 0..1) as a fallback source for
      `SetPointer`, used only on the platform/condition where `PointerMoved`
      position proves unreliable.

## Phase 4 — Final Xbox validation & wrap-up

- [ ] Deploy to Xbox physical hardware with the Phase 1 (+ 2/3 if executed)
      changes.
- [ ] Test: mouse motion, left/right click, wheel, PUREMENU cursor, DOS
      in-game mouse.
- [ ] Test: gamepad-simulated mouse mode still works and doesn't conflict
      with real mouse input.
- [ ] Update `AGENTS.md` — add a new numbered "Known Bugs & Pitfalls" entry
      summarizing the fix (CoreWindow absolute position vs MouseDevice raw
      delta) so future sessions don't rediscover this from scratch.
- [ ] Update `README.md` in this folder — mark investigation/implementation
      as complete, link to the final commit(s).

---

## Phase 0 results

_(To be filled in once Xbox hardware is available and Phase 0 diagnostics
have been run.)_
