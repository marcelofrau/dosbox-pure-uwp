## ADDED Requirements

### Requirement: ANGLE binary deployed in uwp-dep
ANGLE for UWP x64 SHALL be built and placed in `extern/uwp-dep/x64/bin/` as deployable DLLs.

#### Scenario: ANGLE DLLs present
- **WHEN** build completes
- **THEN** `libEGL.dll` and `libGLESv2.dll` exist in `extern/uwp-dep/x64/bin/`

#### Scenario: DLLs deployed with app
- **WHEN** app package is built
- **THEN** ANGLE DLLs are included as `DeploymentContent`

### Requirement: uwp-dep.props references ANGLE
`uwp-dep.props` SHALL add ANGLE include directories and link libraries.

#### Scenario: Build includes ANGLE headers
- **WHEN** any UWP source includes `EGL/egl.h` or `GLES3/gl3.h`
- **THEN** the include path resolves to `extern/uwp-dep/x64/include/`

#### Scenario: Build links ANGLE
- **WHEN** linker runs
- **THEN** `libEGL.lib` and `libGLESv2.lib` are linked

### Requirement: ANGLE DLLs loaded at runtime
App SHALL load ANGLE DLLs dynamically, NOT at process start, to handle missing DLLs gracefully.

#### Scenario: ANGLE init success
- **WHEN** retro_load_game() requests HW render
- **THEN** EGL display initializes from D3D11 device, GLES context created

#### Scenario: ANGLE DLL missing
- **WHEN** `LoadPackagedLibrary` for ANGLE DLLs fails
- **THEN** core falls back to SW render path (no crash)

### Requirement: EGL display created from D3D11 device
ANGLE SHALL create EGL display and GLES context from the existing D3D11 device via `EGL_EXT_platform_device` or equivalent.

#### Scenario: EGL context creation
- **WHEN** ANGLE is initialized
- **THEN** EGL display, surface, and GLES context are created successfully

#### Scenario: EGL context failure
- **WHEN** EGL initialization fails (e.g., unsupported device)
- **THEN** core falls back to SW render path
