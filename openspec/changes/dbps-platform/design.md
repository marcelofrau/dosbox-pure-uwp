# Design — DBPS Platform Stubs

## State Storage
`dosbox_pure_sta.cpp` holds static/global state shared with DBPS calls:

```cpp
struct PlatformState {
    bool have_joy;
    SdlInput* sdl_input;                    // shared reference
    std::unordered_map<std::string, std::string> config_overrides;
    OSDQuad overlay_quad;                   // OSD frame
    int capture_joy_port, capture_joy_bind; // -1 if not capturing
    std::string captured_bind_name;
};
```

## Stub Implementations

### DBPS_HaveJoy()
Return `m_sdlInput->HasController()`. If SdlInput ref not set, return false.

### DBPS_GetJoyBind(int port, int bind)
Query `m_sdlInput->GetBindString(port, bind)`. Return empty string if no mapping.

### DBPS_StartCaptureJoyBind(int port, int bind)
Set `capture_joy_port/bind`. On next controller button press via `SdlInput`, record the button ID → resolve to bind name string → call `DBPS_SetJoyBindCallback(name)`.

### DBPS_SubmitOSDFrame(const void* frame, int w, int h)
Copy pixel data into `overlay_quad` struct. Flag as dirty. Renderer checks and submits to D2D overlay on next frame.

### DBPS_ApplyConfigOverrides(const char* json)
Parse JSON string (use simple `strstr`/`sscanf` or a lightweight parser). For each key-value pair, call `retro_environment(RETRO_ENVIRONMENT_SET_VARIABLE, ...)`.

### DBPS_IsConfigOverride(const char* key)
Check `config_overrides` map membership.

### DBPS_ToggleConfigOverride(const char* key, const char* default_val)
If key exists → erase. If not → insert with default_val. Notify core via SET_VARIABLE.

### DBPS_GetConfigOverrideJSON()
Serialize `config_overrides` map to JSON string. Return pointer to static buffer.

### DBPS_OnContentLoad(const char* path, const char* name, uint64_t size)
`OutputDebugStringA` log. Optionally store for HUD display.

## Wiring
`dosbox_pure_sta.cpp` needs access to `SdlInput*`. Pass via `DBPS_SetSdlInput(SdlInput*)` called from `RetroCore` init. Or make SdlInput a singleton.
