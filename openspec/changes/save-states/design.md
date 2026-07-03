# Save States — Design

## API

Add to RetroCore:
```cpp
bool SaveState(int slot, bool save);
bool LoadState(int slot);
```

## Save flow
1. Query `retro_serialize_size()` → buffer size
2. Allocate `std::vector<uint8_t>` buffer
3. Call `retro_serialize(data.data(), data.size())`
4. Write buffer to `LocalFolder\saves\<rom_name>.state<slot>`

## Load flow
1. Read file `LocalFolder\saves\<rom_name>.state<slot>` → buffer
2. Call `retro_unserialize(data.data(), data.size())`
3. Return success/failure

## Slot management
- Slots 0-9, current slot tracked in RetroCore
- Slot 0 = quicksave (F5)
- F6 = cycle to next slot
- F7 = load current slot

## DBPS stubs (dosbox_pure_sta.cpp)

### DBPS_RequestSaveLoad(slot, save)
- Bridge: call `g_retroCore->SaveState(slot, save)` or `LoadState(slot)`

### DBPS_HaveSaveSlot(slot)
- Check if `LocalFolder\saves\<rom_name>.state<slot>` exists
- Return 1 if exists, 0 if not

## Auto-save on Suspend
- In `App.cpp OnSuspending`: if game loaded, save to slot 99 (autosave)
- On resume: check for autosave, prompt to load or discard

## SRAM handling
- On shutdown: call `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` to get SRAM pointer
- Write SRAM to `LocalFolder\saves\<rom_name>.srm`

## Thread safety
- Save/load on main thread after `retro_run` completes
- No concurrent access
