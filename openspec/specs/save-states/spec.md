# Save States Capability

## Scope
Serialization and restoration of full DOSBox emulator state, plus battery-backed SRAM.

## Requirements

### Core serialization API
- `retro_serialize_size()` — return size needed for full state
- `retro_serialize(void* data, size_t size)` — write full state to buffer (uses `DBPArchiveWriter`)
- `retro_unserialize(const void* data, size_t size)` — restore full state from buffer (uses `DBPArchiveReader`)
- Core supports 3 modes: `on` (normal), `rewind` (lax), `disabled` (size=0)

### Frontend save/load
- Save to `LocalFolder\saves\<rom-name>.state<slot>` (or core's save dir)
- F5 = save to current slot, F7 = load from current slot
- Slot cycling: F6 = next slot
- Auto-save on app suspend/shutdown
- Auto-load on game start (optional, configurable)

### DBPS stubs for saves
- `DBPS_RequestSaveLoad(int slot, bool save)` → call retro_serialize/unserialize, write/read file
- `DBPS_HaveSaveSlot(int slot)` → check if .state<slot> file exists

### SRAM
- `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` — return pointer to battery-backed RAM
- `retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)` — return SRAM size
- Auto-save SRAM on shutdown (`.srm` file)
- Auto-load SRAM on game load

### Save format
- Core uses custom `DBPArchive` binary format (`.pure.zip` or `.sav`)
- Frontend should NOT interpret format — just pass bytes
- Save directory: `ApplicationData::Current->LocalFolder\saves\`

## Affected Changes
- `save-states`
