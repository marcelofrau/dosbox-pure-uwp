# Tasks — DBPS Platform Stubs

- [ ] 1. Implement DBPS_HaveJoy() — return SdlInput controller connection status
- [ ] 2. Implement DBPS_GetJoyBind(port, bind) — query current button mapping string
- [ ] 3. Implement DBPS_StartCaptureJoyBind(port, bind) — enter capture mode, register SdlInput callback
- [ ] 4. Implement DBPS_SubmitOSDFrame(frame, w, h) — store frame for overlay render
- [ ] 5. Implement DBPS_ApplyConfigOverrides(json) — parse JSON, call SET_VARIABLE per key
- [ ] 6. Implement DBPS_IsConfigOverride(key) / DBPS_ToggleConfigOverride(key, default) / DBPS_GetConfigOverrideJSON()
- [ ] 7. Implement DBPS_OnContentLoad(path, name, size) — logging
- [ ] 8. Wire SdlInput reference into dosbox_pure_sta.cpp (DBPS_SetSdlInput or singleton)
- [ ] 9. Build and test: all stubs respond with real data, no crashes
