# Tasks — DBPS_GetMouse

- [ ] 1. Register CoreWindow PointerPressed/Moved/Released/WheelChanged handlers in App::SetWindow
- [ ] 2. Implement OnPointerMoved: track absolute + relative position in MouseState
- [ ] 3. Implement OnPointerPressed/Released: update button state
- [ ] 4. Implement OnPointerWheelChanged: accumulate scroll delta
- [ ] 5. Implement DBPS_GetMouse in dosbox_pure_sta.cpp — return accumulated rel/abs values
- [ ] 6. Update retro_input_state `RETRO_DEVICE_MOUSE` case to read real MouseState values
- [ ] 7. Update retro_input_state `RETRO_DEVICE_POINTER` case with normalized absolute pointer
- [ ] 8. Add mouse capture toggle (middle-click or F8) — hide cursor, relative-only mode
- [ ] 9. Thread safety: std::mutex around MouseState, lock on all reads/writes
- [ ] 10. Build and test: mouse cursor visible in Puremenu, relative mouse works in DOS game
