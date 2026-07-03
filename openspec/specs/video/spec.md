# Video Capability

## Scope
Video rendering pipeline from core framebuffer to screen via Direct2D.

## Requirements

### Core rendering
- retro_video callback accepts XRGB8888 framebuffer (pitch = width * 4)
- Reject HW frames (pitch == 0 = `RETRO_HW_FRAME_BUFFER_VALID`)
- Thread-safe frame buffer copy with mutex
- GrabVideoFrame() consumes + invalidates for next frame

### D2D bitmap rendering
- `DXGI_FORMAT_B8G8R8A8_UNORM` + `D2D1_ALPHA_MODE_IGNORE` (raw RGB, NOT premultiplied)
- `GetDpi()` from render target (never hardcoded)
- `BeginDraw()` → `DrawBitmap()` with letterbox → `EndDraw()`
- `D2DERR_RECREATE_TARGET` handling (device lost)

### Scale modes
- **Pixel-perfect**: nearest-neighbor, 1:1 pixel mapping centered
- **Integer scale**: largest integer multiplier that fits viewport
- **Stretch**: fill entire viewport (may distort aspect)
- **Aspect-ratio**: stretch to fill while maintaining 4:3 or core-reported ratio

### Software framebuffer (OSD overlay)
- `GET_CURRENT_SOFTWARE_FRAMEBUFFER` returns pointer to current SW framebuffer
- Core OSD (Puremenu) draws overlay onto framebuffer before video_cb

### Core video options
Expose via GET_VARIABLE:
- `dosbox_pure_machine` (vgaonly, svga, etc.)
- `dosbox_pure_cga`, `dosbox_pure_hercules`, `dosbox_pure_svga`
- `dosbox_pure_aspect_correction`, `dosbox_pure_overscan`
- `dosbox_pure_voodoo`, `dosbox_pure_voodoo_scale`, `dosbox_pure_voodoo_gamma`

## Affected Changes
- `software-framebuffer`
- `video-enhancements`
