# Dynarec (JIT) on UWP: Status and Roadmap

## Current Status: ENABLED

Dynamic recompiler is enabled and functional on UWP. Fix applied in commit
`e499454` (tag `dynarec-done`).

## Fix Applied

### Problem

`VirtualAlloc` returns NULL in AppContainer (UWP). `malloc()` fallback allocates
non-executable memory. Dynarec crashes on first jump to generated x86 code.

### Solution

`dosbox-uwp/local/dosbox-pure/src/cpu/dyn_cache.h`: replaced `VirtualAlloc` with
`VirtualAllocFromApp` (same signature, UWP-compatible). Added
`cache_make_writable()`/`cache_make_executable()` calls around code generation
sites (`gen_runcodeInit`, `gen_dh_fpu_saveInit`) using
`VirtualProtectFromApp`.

Also:
- `DISABLE_DYNAREC` removed from `PreprocessorDefinitions` in `.vcxproj`
- Local copy of `dyn_cache.h` at `dosbox-uwp/local/dosbox-pure/src/cpu/`

### Include path

`.vcxproj` `AdditionalIncludeDirectories` lists
`dosbox-uwp/local/dosbox-pure/src/cpu/` BEFORE
`extern/dosbox-pure/src/cpu/` so the patched header takes precedence.

## Performance

| Core type | Relative speed | Suitable for |
|-----------|---------------|--------------|
| Dynarec   | ~5-10x        | All DOS games, including post-1995 (GTA, Duke3D, Quake) |
| Interp.   | 1x (baseline) | Games up to ~1995 (Arkanoid, Doom, Commander Keen, etc.) |

## Known Risks

- **Xbox certification**: JIT policies on Xbox Series may be stricter than
  desktop UWP. Unknown if dynarec passes store verification.
- **VirtualProtectFromApp**: Works on Windows 11 desktop. Xbox behavior
  untested.
- **Exception handling**: If `VirtualAllocFromApp` or `VirtualProtectFromApp`
  fail, dynarec logs the error and falls back to interpreter via existing
  `DISABLE_DYNAREC` guard in `config.h`.

## Related Files

| File | Role |
|------|------|
| `dosbox-uwp/local/dosbox-pure/src/cpu/dyn_cache.h` | Patched JIT cache with VirtualAllocFromApp |
| `extern/dosbox-pure/src/cpu/core_dyn_x86.cpp` | Dynarec core (not patched) |
| `extern/dosbox-pure/include/config.h:108` | `DISABLE_DYNAREC` guard (not defined in build) |
| `dosbox-uwp/dosbox-uwp.vcxproj` | PreprocessorDefinitions (no DISABLE_DYNAREC) |
