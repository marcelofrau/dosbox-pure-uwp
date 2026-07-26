## ADDED Requirements

### Requirement: Voodoo games render correctly
Voodoo 3D games (e.g., Tomb Raider series, Final Fantasy VII PC, GLQuake) SHALL render with correct colors, geometry, and texture mapping via ANGLE path.

#### Scenario: Voodoo game launches and displays
- **WHEN** user loads a Voodoo game
- **THEN** game renders at correct resolution
- **THEN** colors match expected output (no hue shift or channel swap)

#### Scenario: Voodoo performance option
- **WHEN** "3dfx Voodoo Performance" is set to "auto" or "4" (GL)
- **THEN** core uses HW render path via ANGLE

#### Scenario: Voodoo fallback
- **WHEN** "3dfx Voodoo Performance" is set to "1" or "2"
- **THEN** core uses SW render path (existing behavior unchanged)

### Requirement: Non-Voodoo games unaffected
Games that don't use Voodoo (standard VGA, SVGA) SHALL continue to use the existing SW framebuffer path via RetroD3D11Renderer.

#### Scenario: Standard VGA game
- **WHEN** user loads a non-Voodoo DOS game
- **THEN** render path is identical to current SW pipeline
- **THEN** no ANGLE overhead incurred

### Requirement: Performance comparable to RetroArch
HW-rendered Voodoo performance SHALL be within 10% of RetroArch with ANGLE on equivalent hardware.

#### Scenario: FPS comparison
- **WHEN** running a Voodoo benchmark game
- **THEN** FPS is within 90% of RetroArch's ANGLE performance on same hardware
