# File I/O — Delta Spec

## REMOVED Requirements

### Requirement: fopen_wrap replacement
**Reason**: CRT `fopen()` works on `ApplicationData::LocalFolder` paths — no need for `CreateFile2FromAppW`.
**Migration**: ROM file placed in `LocalFolder\temp\` via WinRT IO; core reads via CRT `fopen` as before.

### Requirement: open_directory implementation
**Reason**: CRT `FindFirstFile`/`FindNextFile` works on `LocalFolder` — no VFS opendir needed.
**Migration**: Puremenu file browser operates on `LocalFolder` contents.

### Requirement: stat/access compat
**Reason**: CRT `stat()`/`access()` works on `LocalFolder` — no `GetFileAttributesExFromAppW` wrappers needed.
**Migration**: Core continues to use CRT stat/access unchanged.

## MODIFIED Requirements

### Requirement: ROM delivery via LocalFolder sandbox
Old behavior: Write `info->data` to temp file via `GetTempPathFromAppW` + `CreateFile2FromAppW` as fallback.
New behavior: Copy `IBuffer^` from picker to `LocalFolder\temp\` via `FileIO::WriteBufferAsync`. Set `info->path` to LocalFolder path. Core reads via CRT `fopen` — works natively.

#### Scenario: ROM loaded from picker
- **WHEN** user presses F1, picks a `.zip` ROM file
- **THEN** `App.cpp` reads file to `IBuffer^`, calls `LoadRom(buffer, filename)`
- **THEN** `RetroCore::LoadRom` writes buffer to `LocalFolder\temp\<filename>`
- **THEN** `info->path` set to LocalFolder temp path
- **THEN** `retro_load_game()` called — core reads ROM via `fopen_wrap()` → CRT `fopen()` → succeeds

#### Scenario: ROM type variations
- **WHEN** user picks `.iso`, `.cue`, `.img`, `.dsk` (floppy), or `.zip` containing any of these
- **THEN** file is copied to `LocalFolder\temp\` with original extension preserved
- **THEN** core mounts it internally (C:, D:, A:) via its own mounting logic

#### Scenario: Disk swap during gameplay
- **WHEN** user swaps disk via Puremenu or retro_disk_control
- **THEN** new ROM file is copied to `LocalFolder\temp\newdisk.img`
- **THEN** old temp file is cleaned up
- **THEN** core reads new path seamlessly

## ADDED Requirements

### Requirement: Save states & config persistence
Writes go to `LocalFolder\saves\` and `LocalFolder\config\` — CRT `fopen` works on these paths.

#### Scenario: Save state creation
- **WHEN** user triggers save state (F2 or Puremenu)
- **THEN** core writes file to `LocalFolder\saves\`
- **THEN** file survives app restart

#### Scenario: Config file creation
- **WHEN** core starts and writes FRONTEND.DBP or other config
- **THEN** file appears in `LocalFolder\`
- **THEN** re-launch reads existing config file

### Requirement: Temp file cleanup
Previous ROM temp file deleted when new ROM is loaded, or on `retro_unload_game()`.

#### Scenario: Load new ROM after previous
- **WHEN** user loads a second ROM after playing one
- **THEN** old `LocalFolder\temp\oldgame.zip` is deleted
- **THEN** new ROM copied to `LocalFolder\temp\newgame.zip`
