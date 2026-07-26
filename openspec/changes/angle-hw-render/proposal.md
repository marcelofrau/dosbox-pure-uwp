## Why

dosbox-pure core's Voodoo (3dfx) HW render path uses OpenGL, which is unavailable in UWP. This forces SW fallback, losing GPU acceleration for Voodoo 3D games. ANGLE (Almost Native Graphics Layer Engine) translates OpenGL ES calls to D3D11 — available on UWP — enabling HW render without porting the core's GL code.

## What Changes

- Integrate ANGLE DLL into uwp-dep as a deployable binary
- Accept `RETRO_HW_CONTEXT_OPENGLES2`/`OPENGLES3` in `SET_HW_RENDER`
- Route GL proc addresses through ANGLE's `eglGetProcAddress`
- Voodoo OGL path now runs on D3D11 via ANGLE translation
- Remove no-op `DBP_STANDALONE` block in `GFX_EndUpdate` (already done)
- uwp-dep.props updated with ANGLE include/libs/dll

## Capabilities

### New Capabilities
- `angle-integration`: Build/download ANGLE for UWP x64, deploy as DLL, link headers/libs in props
- `hw-render-acceptance`: Accept SET_HW_RENDER with GLES context types, route proc addresses through ANGLE
- `voodoo-angle-validation`: Verify Voodoo 3D games render correctly via ANGLE path

### Modified Capabilities
- _(none — no existing specs are changing)_

## Impact

- **New dependency**: ANGLE DLL (~5MB) deployed alongside app
- **RetroCore.cpp**: SET_HW_RENDER accepts GLES, stores ANGLE proc address resolver
- **uwp-dep.props**: Adds ANGLE include dirs, libs, DLL deployment
- **dosbox_pure_libretro.cpp**: No changes needed (already supports `OPENGLES2`/`OPENGLES3` in `testhwcontexts`)
- **dosbox_pure_sta.cpp**: `DBPS_GLGetProcAddress` needs real impl wired to ANGLE
