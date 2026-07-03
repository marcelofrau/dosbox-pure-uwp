# Video enhancements — Design

## ScaleMode enum

Add to `RetroScreenRenderer.h`:

```cpp
enum class ScaleMode {
    PixelPerfect,  // 1:1 pixel mapping, centered
    Integer,       // Largest N where N*w ≤ vp_w && N*h ≤ vp_h, centered
    Stretch,       // Fill entire viewport (current behavior)
    AspectRatio    // Stretch fill maintaining source aspect ratio
};
```

## Modes

### PixelPerfect
- Scale factor = 1.0
- Center in viewport: offset = (vp_w - w)/2, (vp_h - h)/2
- No interpolation (point sampling)

### Integer
- Find max N ≥ 1 where N*w ≤ vp_w AND N*h ≤ vp_h
- Render N*w × N*h, centered
- No interpolation

### Stretch (default)
- Scale to vp_w × vp_h
- Bilinear interpolation

### AspectRatio
- Scale = min(vp_w / w, vp_h / h)
- Render w*scale × h*scale, centered
- Bilinear interpolation

## Selection
- Keyboard shortcut (e.g. F2) to cycle modes
- Eventually RMLUI config dropdown

## Core video options
- machine, cga, svga, aspect_correction, overscan already handled by register-core-options change
- Renderer reads these from the core options map

## FPS overlay
- Toggle debug overlay showing current FPS
- Measured from frame timestamps in Update/Render loop
