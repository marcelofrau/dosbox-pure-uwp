# Tasks: patch-fopen-vfs

Estimated: ~15-30 min per task. Order matters.

---

- [x] **Task 1: LoadRom — copy buffer to LocalFolder temp file**
  - In `RetroCore::LoadRom` (App.cpp → RetroCore.cpp): after picker reads ROM to `IBuffer^`
  - Create `LocalFolder\temp\` dir if missing via `CreateDirectoryFromAppW` or `ApplicationData::LocalFolder`
  - Write buffer to `LocalFolder\temp\rom_filename` using `FileIO::WriteBufferAsync` (`.then()` chain, never `.get()`)
  - Determine filename from `StorageFile^` name (picker provides it)
  - Set `info->path` to the LocalFolder temp path (UTF-8)
  - Set `info->data` to NULL, `info->size` to 0 (core reads via path)
  - Keep original `info->data` as fallback if write fails
  - `OutputDebugStringA` logging `[dosbox-uwp]` with temp path

- [x] **Task 2: Handle ROM filename from picker**
  - `App.cpp` `filePicker->PickSingleFileAsync().then()` already has `StorageFile^`
  - Pass filename (e.g., `game.zip`) along with `IBuffer^` to `LoadRom`
  - Ensure extension preserved (`.zip`, `.iso`, `.cue`, `.img`, `.dsk`)
  - Handle Unicode filenames correctly (UTF-8 conversion for `info->path`)

- [x] **Task 3: Temp file cleanup on unload**
  - In `retro_unload_game()` or `LoadRom` (when loading new ROM): delete old temp file
  - Use `DeleteFileFromAppW` or `File::DeleteAsync`
  - Track current temp path in member variable `m_tempPath`
  - Handle case: no previous temp file (first load)
  - `OutputDebugStringA` logging cleanup

- [x] **Task 4: Save states directory**
  - Ensure `LocalFolder\saves\` exists (create if missing)
  - Core's `fopen_wrap()` → CRT `fopen()` → works on LocalFolder
  - Test: save state creates file in `LocalFolder\saves\`
  - Test: load state reads from same location

- [x] **Task 5: Config & persistent files directory**
  - Ensure `LocalFolder\config\` exists
  - Core writes FRONTEND.DBP, conf files etc. to `LocalFolder\`
  - Test: launch game, verify FRONTEND.DBP appears in LocalFolder
  - Test: delete FRONTEND.DBP, re-launch, verify it's recreated

- [ ] **Task 6: Multi-disk / disk swap flow**
  - When user swaps disk (Puremenu mount, or retro_disk_control): new picker call
  - New ROM gets copied to `LocalFolder\temp\newdisk.img`
  - Core reads new path seamlessly
  - Old temp file cleaned up (or kept if needed for multi-disk mount)
  - Test: mount .iso as D: while game on C: runs

- [x] **Task 7: Build and test full cycle**
  - `MSBuild.exe "dosbox-pure-unleashed-uwp.sln" /p:Configuration=Release /p:Platform=x64`
  - Run: pick `.zip` ROM → game loads → Puremenu navigable
  - Run: pick `.iso` ROM → mounts correctly
  - Run: pick `.dsk` floppy → boots
  - Check DebugView `[dosbox-uwp]` logs
  - Verify: no CRT file-access crashes
