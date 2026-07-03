# Tasks: Register core options

## Task 1: Define option storage
- Add static `std::unordered_map<std::string, std::string>` for key→value in RetroCore.
- Add `bool s_optionsUpdated` flag.
- Add pointer/count for `retro_core_options_v2_definition` array.

## Task 2: Handle RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 (cmd 71)
- In `retro_env()` `case 71`:
  - Cast `data` to `retro_core_options_v2*`.
  - Iterate `option_defs[]` up to `count`.
  - Store key → `default_value` in map.
  - Return `1`.
- Add debug log: `[dosbox-uwp] SET_CORE_OPTIONS_V2: %u options\n`

## Task 3: Handle RETRO_ENVIRONMENT_GET_VARIABLE (cmd 15)
- In `retro_env()` `case 15`:
  - Cast `data` to `retro_variable*`.
  - Lookup `data->key` in options map.
  - If found: `data->value` = map value, return `1`.
  - If not found: return `0`.

## Task 4: Handle RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE (cmd 65)
- In `retro_env()` `case 65`:
  - Cast `data` to `bool*`.
  - `*data = s_optionsUpdated`.
  - `s_optionsUpdated = false`.
  - Return `1`.

## Task 5: Handle v1/v0 fallback
- `case 56` (SET_CORE_OPTIONS): iterate `retro_core_option_definition*` array, store key→default.
- `case 16` (SET_VARIABLES): iterate `retro_variable*` array, parse `"key; values|separated"`, store first value.
- `case 70` (SET_VARIABLE): update single key→value, set `s_optionsUpdated = true`.

## Task 6: Hardcode initial values from core_options.h defaults
- Parse `core_options.h` for all ~52 option keys and defaults.
- Alternative: read defaults from `retro_core_options_v2_definition` at runtime (preferred).
- Validate all 6 categories covered.

## Task 7: Build and test
- Build with MSBuild: `MSBuild.exe dosbox-pure-unleashed-uwp.sln /p:Configuration=Release /p:Platform=x64`
- Launch. Open Puremenu in-game (F12).
- Verify options menu shows categories and settings.

## Task 8: Debug log GET_VARIABLE timing
- Add `OutputDebugStringA` log on every GET_VARIABLE call showing key and returned value.
- Verify core queries options during boot and runtime.
- Confirm no crash on unknown keys (return 0 gracefully).
