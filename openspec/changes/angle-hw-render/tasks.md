## 1. ANGLE Binary Integration

- [ ] 1.1 Build or download ANGLE for UWP x64 (libEGL.dll, libGLESv2.dll, libEGL.lib, libGLESv2.lib)
- [ ] 1.2 Place binaries in `extern/uwp-dep/x64/bin/` and libs in `extern/uwp-dep/x64/lib/`
- [ ] 1.3 Add ANGLE include headers to `extern/uwp-dep/x64/include/`
- [ ] 1.4 Update `uwp-dep.props` with ANGLE include dirs, libs, and DLL DeploymentContent

## 2. RetroCore.cpp: Accept GLES HW Render

- [ ] 2.1 In `SET_HW_RENDER`, accept `RETRO_HW_CONTEXT_OPENGLES2`, `OPENGLES3`, `OPENGLES_VERSION`
- [ ] 2.2 Store the `retro_hw_render_callback` struct (context_reset, context_destroy, get_proc_address, get_current_framebuffer)
- [ ] 2.3 Implement `context_reset`: create EGL display from D3D11 device, create GLES context and surface
- [ ] 2.4 Implement `context_destroy`: destroy EGL context/surface/display
- [ ] 2.5 Route `get_proc_address` through `eglGetProcAddress` for GLES functions

## 3. DBPS_GLGetProcAddress Implementation

- [ ] 3.1 In `dosbox_pure_sta.cpp`, implement `DBPS_GLGetProcAddress` to return ANGLE-resolved function pointers when HW render active
- [ ] 3.2 Return NULL for unsupported functions (core handles gracefully)

## 4. EGL Surface from D3D11 RTV

- [ ] 4.1 In `context_reset`, create EGL surface from the D3D11 device via `EGL_EXT_platform_device`
- [ ] 4.2 Ensure `get_current_framebuffer()` returns 0 (default framebuffer) since ANGLE renders to bound D3D11 RTV

## 5. Build and Validate

- [ ] 5.1 Build project with ANGLE linked
- [ ] 5.2 Verify non-Voodoo games still use SW path (existing RetroD3D11Renderer unchanged)
- [ ] 5.3 Verify Voodoo games load ANGLE path and render correctly
- [ ] 5.4 Verify graceful fallback to SW when ANGLE DLLs missing
