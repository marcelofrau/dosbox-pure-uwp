# Patch file I/O: ROM via LocalFolder sandbox

## Problem
UWP blocks CRT `fopen()` on picker-granted paths. Core uses `need_fullpath=true`, ignores `info->data`. `fopen_wrap(path)` → CRT `fopen()` → fails.

## Solution
Copy ROM buffer to `ApplicationData::LocalFolder\temp\` — CRT `fopen()` works on LocalFolder. Core reads normally. Zero patches to `fopen_wrap`, `open_directory`, `stat`, `access`.

## Flow
```
Picker → IBuffer → WriteAsync → LocalFolder\temp\game.zip → info->path = LocalFolder path → retro_load_game → fopen_wrap() → CRT fopen() → ✓
```

## Scope
- `RetroCore::LoadRom` — copy `IBuffer^` to LocalFolder temp file, set `info->path`
- `App.cpp` — pass `StorageFile^` filename to `LoadRom`
- Temp file cleanup on unload/reload
- `LocalFolder\saves\` and `LocalFolder\config\` creation

## No Changes
- `fopen_wrap()` — untouched, CRT works on LocalFolder
- `open_directory()` — untouched
- `stat()`/`access()` — untouched
- `vfs_implementation_uwp.cpp` — untouched

## Specs Affected
- `openspec/specs/file-io/spec.md`

## Risks
- `.then()` chain required for async write (no `.get()` on STA)
- Unicode filename conversion (picker returns `Platform::String^`)
- Temp file cleanup on crash (minor — LocalFolder is per-app, OS may clean)
