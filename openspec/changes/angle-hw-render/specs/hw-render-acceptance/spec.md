## ADDED Requirements

### Requirement: SET_HW_RENDER accepts GLES context types
RetroCore SHALL accept `RETRO_HW_CONTEXT_OPENGLES2`, `RETRO_HW_CONTEXT_OPENGLES3`, and `RETRO_HW_CONTEXT_OPENGLES_VERSION` in `SET_HW_RENDER` environment callback.

#### Scenario: GLES context accepted
- **WHEN** core calls `RETRO_ENVIRONMENT_SET_HW_RENDER` with `context_type = RETRO_HW_CONTEXT_OPENGLES3`
- **THEN** callback returns `true`
- **THEN** `dbp_hw_render` callbacks (context_reset, context_destroy, get_proc_address, get_current_framebuffer) are stored

#### Scenario: GLES context rejected
- **WHEN** ANGLE fails to initialize
- **THEN** callback returns `false` (core falls back to SW)

### Requirement: get_proc_address routes through ANGLE
`DBPS_GLGetProcAddress` SHALL return function pointers resolved via `eglGetProcAddress` when ANGLE is active.

#### Scenario: GLES function resolved
- **WHEN** core requests `glTexSubImage2D` via get_proc_address
- **THEN** function pointer from ANGLE's `eglGetProcAddress` is returned

#### Scenario: GLES function not found
- **WHEN** core requests a function ANGLE doesn't support
- **THEN** NULL returned (core handles gracefully)

### Requirement: context_reset sets up GLES state
When HW context is created, `context_reset` SHALL notify RetroD3D11Renderer to store the current render target view for ANGLE to render into.

#### Scenario: Context reset
- **WHEN** `context_reset()` is called
- **THEN** current D3D11 render target view is made available to ANGLE as EGL surface

### Requirement: get_current_framebuffer returns 0
Since ANGLE renders to the EGL surface bound to D3D11 RTV, the FBO callback SHALL return 0 (default framebuffer).

#### Scenario: FBO callback
- **WHEN** core calls `get_current_framebuffer()`
- **THEN** returns 0 (default framebuffer in GLES, which ANGLE maps to the bound D3D11 RTV)
