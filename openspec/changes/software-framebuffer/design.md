# Design: GET_CURRENT_SOFTWARE_FRAMEBUFFER

## Environment call

### GET_CURRENT_SOFTWARE_FRAMEBUFFER (cmd 55)
- `data` is `retro_framebuffer*` struct pointer.
- The frontend fills this struct before calling back to core.

#### Struct fields to fill

```cpp
struct retro_framebuffer {
    void *data;          // → s_lastFrame.data (current frame pixels)
    unsigned width;      // → s_lastFrame.width
    unsigned height;     // → s_lastFrame.height  
    unsigned pitch;      // → s_lastFrame.pitch
    unsigned access_flags;      // → RETRO_MEMORY_ACCESS_WRITE
    unsigned memory_flags;      // → RETRO_MEMORY_TYPE_CACHED_WRITEBACK
};
```

#### Access flags
- `RETRO_MEMORY_ACCESS_WRITE` — core needs to write OSD pixels into framebuffer.
- Core reads existing pixels, composites overlay, writes back.
- No `READ` flag needed (core uses its own pointer), but harmless to include.

#### Memory flags
- `RETRO_MEMORY_TYPE_CACHED_WRITEBACK` — standard CPU writeback cache.
- Framebuffer is regular heap memory (`std::vector<uint8_t>`), not GPU-mapped.

## Guard logic

```cpp
case 55: { // GET_CURRENT_SOFTWARE_FRAMEBUFFER
    auto* fb = static_cast<retro_framebuffer*>(data);
    if (!fb) return 0;
    if (!s_lastFrame.data || s_lastFrame.pitch == 0) return 0; // HW frame or no frame yet
    fb->data = s_lastFrame.data;
    fb->width = s_lastFrame.width;
    fb->height = s_lastFrame.height;
    fb->pitch = s_lastFrame.pitch;
    fb->access_flags = RETRO_MEMORY_ACCESS_WRITE;
    fb->memory_flags = RETRO_MEMORY_TYPE_CACHED_WRITEBACK;
    return 1;
}
```

## Thread safety
- `retro_env` calls happen from core thread (same as `retro_run` / `video_cb`).
- No concurrent access to `s_lastFrame` from render thread during `retro_run`.
- If `RetroScreenRenderer` accesses `s_lastFrame` on D2D render thread, add mutex.
- Current design: renderer reads `s_lastFrame` in `Render()` which is called from `Update()` → main thread, serialized after `retro_run`.

## Core usage flow
1. Core calls `env_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb)`.
2. Frontend returns pointer to current framebuffer pixels.
3. Core blits OSD/menu text onto framebuffer using software rasterizer.
4. Core calls `video_cb(fb.data, fb.width, fb.height, fb.pitch)` with modified buffer.
5. Frontend renders the composited frame.

## Edge cases
- **No frame yet**: return 0 (s_lastFrame.data == nullptr).
- **HW frame** (pitch == 0): return 0, core falls back to no-OSD.
- **Size changed**: core calls GET_CURRENT_SOFTWARE_FRAMEBUFFER each frame, so size always matches current.
- **Null fb pointer**: defensive return 0.
