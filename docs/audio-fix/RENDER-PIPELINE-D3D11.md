# Render Pipeline — D2D → D3D11 Migration (keep separate from audio)

> This is a **side quest** relative to the audio work. It is documented here only
> because a past local attempt changed the renderer **and** the audio at the same
> time, which confounded both. **Do them in separate branches.**

---

## 1. Why this note exists

A local experiment (not necessarily pushed to GitHub) replaced the **Direct2D**
bitmap-blit render path with a **Direct3D 11** textured-quad path. The maintainer
reports it "improves some other things" but that attempt was entangled with an
audio change, so it was unclear which change caused which regression.

**Rule:** never change the renderer and the audio engine in the same branch/commit.
A rendering change alters frame timing (Present cadence, GPU sync), which *looks*
like an audio regression but is a different variable. Isolate them.

---

## 2. Current renderer (baseline on `main`)

- **Direct2D** + DirectWrite over a DXGI swap chain (C++/CX UWP scaffold).
- Core framebuffer (XRGB8888) → `ID2D1Bitmap1`
  (`DXGI_FORMAT_B8G8R8A8_UNORM`, `ALPHA_MODE_IGNORE`) → `DrawBitmap()` with
  letterboxing.
- Files: `dosbox-uwp/Content/RetroScreenRenderer.cpp/.h`; loop in `App.cpp:143`
  (`Render()` → `Present(GetSyncInterval(), 0)`).
- Known D2D pitfalls (from `AGENTS.md` — verify against code, docs may be stale):
  - `BeginDraw()`/`EndDraw()` mandatory or `DrawBitmap()` crashes.
  - `RETRO_HW_FRAME_BUFFER_VALID` sends `pitch == 0`; guard or `memcpy(0)` crashes.
  - Use `ALPHA_MODE_IGNORE`, not `PREMULTIPLIED` (color distortion).
  - DPI must come from `GetDpi()` on the render target, not hardcoded 96.0.

---

## 3. Why move to D3D11 (rationale to validate)

Potential benefits (confirm empirically, don't assume):

- **Explicit control of present timing** — `IDXGISwapChain::Present`,
  `SetMaximumFrameLatency(1)`, waitable swap-chain object → tighter frame pacing,
  which *indirectly* helps audio if audio pacing ever couples to video.
- **Shader-based scaling/CRT filters** — nearest/linear/scanline in a pixel shader
  instead of D2D interpolation modes.
- **Lower overhead** on Xbox — a textured quad is closer to the metal than the D2D
  device context; avoids the D2D-on-D3D interop layer.
- **Alignment with ZillaLib reference** — the `ZillaLib` WP8/UWP path uses **D3D11
  pure** (no D2D): `Present(1,0)`, `BufferCount=1`,
  `DXGI_SWAP_EFFECT_DISCARD`, `SetMaximumFrameLatency(1)`
  (`Source/ZL_PlatformWP.cpp`). Good reference for the pure-D3D approach.

Costs:
- OSD/menu/file-browser currently render via **D2D/DirectWrite**. Moving the game
  framebuffer to D3D11 means either (a) keep a D2D interop surface for UI on top of
  the D3D11 back buffer, or (b) reimplement UI text/shapes in D3D11 (large effort).
  Option (a) is the pragmatic path: D3D11 for the game quad, D2D interop for UI.

---

## 4. Interaction with audio (the important part)

- The renderer change **must not** be used as the audio pacing mechanism. Audio
  timing is solved by DRC on the DAC clock (`AUDIO-PIPELINE.md`), **not** by Present
  cadence. Keep them decoupled: video presents at display refresh; audio steers
  itself.
- A waitable swap chain (`DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` +
  `SetMaximumFrameLatency(1)`) can *reduce* input/display latency without touching
  audio. That is the only legitimate audio-adjacent benefit, and it is passive.
- If you migrate the renderer first, **re-run the full audio soak test**
  (`IMPLEMENTATION-PLAN.md` §6) afterward to confirm the new Present timing didn't
  change queue dynamics.

---

## 5. Suggested order of operations

1. Land the **audio DRC fix** on its own branch, verified green (soak test passes).
2. Merge audio to `main`.
3. **Then** start the D3D11 renderer on a fresh branch:
   - Game framebuffer → D3D11 texture + fullscreen quad + sampler (nearest/linear).
   - Keep OSD/menu/file-browser via D2D interop on the D3D11 back buffer.
   - Add waitable swap chain + `SetMaximumFrameLatency(1)`.
4. Re-run the audio soak test on the D3D11 branch. Only merge if audio is still
   green.

---

## 6. Open questions for the next agent

- Does the current local D3D11 attempt exist in git anywhere (branch/tag/stash), or
  only on the other PC? If recoverable, diff it to salvage the working parts —
  **but split out any audio changes** before reusing.
- Confirm the OSD path: does `DBPS_SubmitOSDFrame` / the PUREMENU overlay still land
  on the framebuffer if the framebuffer becomes a D3D11 texture? (See `AGENTS.md`
  pitfall #8 about `DBP_STANDALONE` and OSD compositing.)
- Xbox: verify `SetMaximumFrameLatency`/waitable swap chain behavior on Series
  hardware (compositor already enforces vsync).
