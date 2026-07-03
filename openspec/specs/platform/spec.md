# Platform Capability

## Scope
Platform-level integration between dosbox-pure core and UWP frontend: DBPS stubs, config overrides, disk control, joypad binding.

## Requirements

### DBPS stubs
All functions in `dosbox_pure_sta.cpp` must have real implementations:

| Function | Behavior |
|----------|----------|
| `DBPS_GetMouse(short& mx, short& my, bool osd)` | Return accumulated relative mouse movement, reset each call |
| `DBPS_HaveJoy()` | Return true when SDL or UWP gamepad connected |
| `DBPS_GetJoyBind(int port, int bind)` | Return current joypad binding string |
| `DBPS_StartCaptureJoyBind(int port, int bind)` | Enter joypad binding capture mode |
| `DBPS_SubmitOSDFrame(const void* frame, unsigned w, unsigned h)` | Route OSD frame to D2D renderer if not using software framebuffer |
| `DBPS_OnContentLoad(const char* path, const char* name, size_t size)` | Log content load event |
| `DBPS_ApplyConfigOverrides(const std::string& json)` | Parse JSON config overrides from FRONTEND.DBP, apply to core options |
| `DBPS_IsConfigOverride(const char* key)` | Return true if key has active override |
| `DBPS_ToggleConfigOverride(const char* key, const char* default_value)` | Toggle override on/off for key |
| `DBPS_GetConfigOverrideJSON()` | Return all overrides as JSON string |
| `DBPS_RequestSaveLoad(int slot, bool save)` | Trigger save/load on given slot |
| `DBPS_HaveSaveSlot(int slot)` | Check if save slot has data |

### Config overrides
- Core reads/writes `FRONTEND.DBP` on C: drive via DOS file APIs
- Frontend must parse this JSON and apply to core options via `SET_VARIABLE`

### Disk control
- `SET_DISK_CONTROL_EXT_INTERFACE` with all 10 callbacks:
  - `set_eject_state(bool ejected)` → eject/insert disc
  - `get_eject_state()` → return ejected state
  - `get_image_index()` → return current disc index
  - `set_image_index(unsigned index)` → switch to disc
  - `get_num_images()` → return total disc count
  - `replace_image_index(unsigned index, const retro_game_info* info)` → replace disc
  - `add_image_index()` → increment disc count
  - `set_initial_image(unsigned index, const char* path)` → set boot disc
  - `get_image_path(unsigned index)` → return disc path
  - `get_image_label(unsigned index)` → return disc label

## Affected Changes
- `dbps-platform`
- `disk-control`
- `dbps-mouse`
