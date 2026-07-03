# Design: Register core options

## Environment call handlers

### SET_CORE_OPTIONS_V2 (cmd 71)
- `data` is `retro_core_options_v2*` containing `option_defs` pointer and `category` count.
- Store `option_defs` array pointer and count.
- Return `1` (supported).
- Optionally also parse category descriptions from `categories` field.

### Fallback: v1 SET_CORE_OPTIONS (cmd 56)
- `data` is `retro_core_option_definition*` array, terminated by NULL key.
- Store in same map structure.

### Fallback: v0 SET_VARIABLES (cmd 16)
- `data` is `retro_variable*` array, terminated by NULL key.
- Format: `"key; values"` — parse first value as default.

### GET_VARIABLE (cmd 15)
- `data` is `retro_variable*` with `key` and `value` fields.
- Lookup key in stored options map.
- If found: set `value` to stored string, return `1`.
- If not found: return `0`.

### GET_VARIABLE_UPDATE (cmd 65)
- `data` is `bool*`.
- Set to `true` only when options were changed externally.
- Default: `false`.
- Use flag `s_optionsUpdated` that gets set by SET_VARIABLE and cleared after read.

### SET_VARIABLE (cmd 70)
- `data` is `retro_variable*` with key and value.
- Update stored value in map.
- Set `s_optionsUpdated = true`.

## Data structures

```cpp
// In RetroCore.h or RetroCore.cpp
struct OptionState {
    std::unordered_map<std::string, std::string> values;  // key → current value
    bool updated;                                          // flag for GET_VARIABLE_UPDATE
    const retro_core_options_v2_definition* defs;          // pointer to core's option defs
    unsigned defCount;                                     // number of option defs
};
static OptionState s_options;
```

## Option categories (from core_options.h)

| Category | Count | Prefix |
|----------|-------|--------|
| General | 7 | `dosbox_pure_` |
| Input | 10 | `dosbox_pure_input_` |
| Performance | 5 | `dosbox_pure_perf_` |
| Video | 10 | `dosbox_pure_video_` |
| System | 6 | `dosbox_pure_system_` |
| Audio | 14 | `dosbox_pure_audio_` |
| **Total** | **52** | |

## Initialization
- Before first `retro_run()`, core calls `SET_CORE_OPTIONS_V2` during `retro_load_game()`.
- All values start at their `default_value` from the option definition.
- No hardcoded defaults needed — read from the definition structs.

## Future consideration
- `SET_CONTROLLER_INFO` (cmd 57) for pad type exposure.
- `GET_INPUT_DEVICE_CAPABILITIES` (cmd 72) for fine-grained input.
