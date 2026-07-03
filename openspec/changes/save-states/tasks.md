# Save States — Tasks

1. Add `SaveState(slot, save)` and `LoadState(slot)` to RetroCore
2. Query `retro_serialize_size()` and allocate buffer
3. Implement `retro_serialize` call, write to file in saves directory
4. Implement `retro_unserialize`: read file, call unserialize
5. Implement `DBPS_RequestSaveLoad` as bridge to SaveState/LoadState
6. Implement `DBPS_HaveSaveSlot` checking for `.state` files
7. Add save state slot tracking (current slot, 0-9)
8. Wire F5/F6/F7 hotkeys in `dosbox_uwpMain::OnKeyEvent`
9. Implement auto-save on app Suspend
10. Implement auto-load prompt on resume (optional)
11. Handle SRAM: `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` on shutdown
12. Build and test: save state in game, reload, verify state restored
