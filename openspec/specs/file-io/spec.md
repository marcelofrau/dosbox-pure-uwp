# File I/O Capability

## Scope
Provide ROM content to dosbox-pure core inside UWP sandbox.

## Constraints
- UWP blocks CRT file APIs (`fopen`, `stat`, `access`, `FindFirstFile`) on **arbitrary user paths** (picker-granted paths)
- File picker grants access via `StorageFile^` — NOT via Win32 paths
- CRT file APIs **DO work** on `ApplicationData::LocalFolder` paths (app's private sandbox)
- Core uses `fopen_wrap()` (CRT `fopen`) for all content loading
- Core uses `need_fullpath=true`, ignores `info->data`
- Core handles mounting internally: ZIP, ISO, CUE/BIN, floppy, HDD images — no extra filesystem access needed

## Requirements

### ROM delivery via LocalFolder sandbox
- On `retro_load_game`: copy `info->data` buffer to a temp file in `LocalFolder\temp\`
- Set `info->path` to the LocalFolder temp path
- Core's `fopen_wrap()` → CRT `fopen()` → works (LocalFolder is accessible)
- Support all ROM types: ZIP, ISO, CUE/BIN, floppy images, HDD images

### Nothing else needed
- `fopen_wrap()`: **no patches** — CRT works on LocalFolder
- `open_directory()`: **no patches** — CRT works on LocalFolder
- `stat`/`access`: **no patches** — CRT works on LocalFolder
- VFS wrapper (`vfs_implementation_uwp.cpp`): only needed for MIDI scan (unchanged)

### Save states & config
- Writes go to `LocalFolder\saves\`, `LocalFolder\config\` — CRT fopen works

## Affected Changes
- `patch-fopen-vfs`
