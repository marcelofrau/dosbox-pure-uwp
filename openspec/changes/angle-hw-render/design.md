## Context

dosbox-pure core supports HW render (3dfx Voodoo) exclusively through OpenGL/GLES. UWP has no native OpenGL — only D3D11. ANGLE translates GLES calls to D3D11 by providing `eglGetProcAddress` and runtime GLES libraries. The core's `testhwcontexts[]` array includes `OPENGLES_VERSION`, `OPENGLES3`, and `OPENGLES2` — if any succeeds, Voodoo runs accelerated.

## Goals / Non-Goals

**Goals:**
- Voodoo 3D games render via ANGLE → D3D11 on UWP
- Zero changes to core's Voodoo GL code (voodoo.cpp, dbp_opengl.h)
- Fallback to SW render if ANGLE fails to init
- ANGLE DLL deployed alongside app (~5MB)

**Non-Goals:**
- Desktop OpenGL compatibility (WGL) — not available on UWP
- Vulkan backend — ANGLE doesn't support it
- Performance optimization of the Voodoo emulation itself

## Decisions

1. **ANGLE over direct GLES→D3D11 port** — Core's Voodoo code has ~200 GL calls + dynamic shader generation. Porting would be months of work. ANGLE handles translation transparently via EGL.

2. **GLES 3.0 over GLES 2.0** — ANGLE supports both. Core tries GLES3 first; if ANGLE doesn't expose it, falls back to GLES2. No change needed in negotiation order.

3. **ANGLE DLLs in uwp-dep** — Same pattern as SDL2.dll: bundled in `extern/uwp-dep/x64/bin/`, deployed via `.props` `DeploymentContent`. No NuGet dependency.

4. **EGL init in RetroCore.cpp** — `EGLDisplay` created from `ID3D11Device` using `eglCreatePbufferFromClientBuffer` or EGL D3D11 extensions. Stored in `dbp_hw_render.egl_display`/`context` if API allows, or in a static.

5. **Proc address routing** — `DBPS_GLGetProcAddress` (currently stub) returns `eglGetProcAddress(name)` for GLES functions that ANGLE can resolve, or falls back to core's SW path.

## Risks / Trade-offs

- [ANGLE DLL size] ~5MB → acceptable, same class as SDL2.dll
- [ANGLE crashes on unsupported GLES features] → Voodoo uses subset that maps well; test with 10-20 Voodoo games
- [EGL surface creation fails] → fallback to SW path (core already handles this)
- [Xbox Series S ANGLE compatibility] → ANGLE targets UWP; needs testing on Xbox GDK (may differ from Windows UWP)
- [Maintenance burden] → ANGLE upstream maintained by Google/Chromium team. Rarely breaks.
