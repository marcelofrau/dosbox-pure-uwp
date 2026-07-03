# Technical Design: ROM via LocalFolder Sandbox

## Architecture

### Flow
```
App.cpp                          RetroCore.cpp                       Core (dosbox_pure_libretro.cpp)
────────                       ─────────────                       ──────────────────────────────
F1 → FileOpenPicker
  → StorageFile^
  → FileIO::ReadBufferAsync()
  → IBuffer^
       │
       ▼ LoadRom(IBuffer^, filename)
       │
       ▼ WriteAsync(LocalFolder\temp\game.zip)
       │
       ▼ info.path = LocalFolder\temp\game.zip (UTF-8)
       info.data  = NULL
       info.size  = 0
       │
       ▼ retro_load_game(&info)
                                     │
                                     ▼ retro_load_game()
                                       → info.path ← LocalFolder path
                                       → fopen_wrap(info.path, "rb")
                                       → CRT fopen() ← WORKS on LocalFolder
                                       → reads game zip
                                       → mounts internally (C:, D:, etc.)
```

### Key Insight
CRT `fopen()` works on `ApplicationData::LocalFolder` paths. No need to patch `fopen_wrap`, `open_directory`, `stat`, or `access`. The only change is where the ROM file physically lives.

## Implementation

### 1. LoadRom — copy to LocalFolder

**File**: `dosbox-uwp/Content/RetroCore.cpp`

```cpp
void RetroCore::LoadRom(IBuffer^ buffer, String^ filename)
{
    // Create temp directory
    auto localFolder = ApplicationData::Current->LocalFolder;
    auto tempFolder = await localFolder->CreateFolderAsync(
        L"temp", CreationCollisionOption::OpenIfExists);

    // Write ROM buffer to temp file
    auto tempFile = await tempFolder->CreateFileAsync(
        filename, CreationCollisionOption::ReplaceExisting);
    await FileIO::WriteBufferAsync(tempFile, buffer);

    // Get local path for info->path
    auto localPath = tempFile->Path->Data();  // Platform::String^ → const wchar_t*

    // Convert UTF-16 path to UTF-8 for retro_game_info
    int len = WideCharToMultiByte(CP_UTF8, 0, localPath, -1, NULL, 0, NULL, NULL);
    char* utf8Path = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, localPath, -1, utf8Path, len, NULL, NULL);

    // Prepare retro_game_info
    retro_game_info info = {};
    info.path = utf8Path;    // Core reads via fopen_wrap → CRT fopen ✓
    info.data = NULL;        // Not needed — core has path
    info.size = 0;

    // Store path for cleanup
    m_tempPath = utf8Path;

    // Call core
    retro_load_game(&info);

    delete[] utf8Path;
}
```

**Note**: C++/CX doesn't support `await` in the traditional sense. Must use `create_task().then()` chain to avoid `.get()` on STA:

```cpp
void RetroCore::LoadRom(IBuffer^ buffer, String^ filename)
{
    auto localFolder = ApplicationData::Current->LocalFolder;
    create_task(localFolder->CreateFolderAsync(L"temp",
        CreationCollisionOption::OpenIfExists)).then([this, buffer, filename](StorageFolder^ tempFolder)
    {
        return create_task(tempFolder->CreateFileAsync(filename,
            CreationCollisionOption::ReplaceExisting));
    }).then([this, buffer](StorageFile^ tempFile)
    {
        return create_task(FileIO::WriteBufferAsync(tempFile, buffer));
    }).then([this]()
    {
        // Continue with retro_load_game...
        // (Moved to a separate method called from .then())
        FinishLoadRom();
    });
}
```

### 2. Temp path tracking

**File**: `dosbox-uwp/Content/RetroCore.h`

```cpp
private:
    std::string m_tempPath;   // LocalFolder temp file path for cleanup
    std::string m_romFilename; // Original filename for extension detection
```

### 3. Cleanup on unload

```cpp
void RetroCore::UnloadRom()
{
    if (!m_tempPath.empty())
    {
        // Delete temp file via Win32 (works on LocalFolder)
        DeleteFileFromAppW(UTF8ToWide(m_tempPath).c_str());
        m_tempPath.clear();
    }
    retro_unload_game();
}
```

### 4. Save states directory

Core writes save files via `fopen_wrap()`. Ensure `LocalFolder\saves\` exists:

```cpp
void RetroCore::EnsureDirectories()
{
    auto localFolder = ApplicationData::Current->LocalFolder;
    create_task(localFolder->CreateFolderAsync(L"saves",
        CreationCollisionOption::OpenIfExists));
    create_task(localFolder->CreateFolderAsync(L"config",
        CreationCollisionOption::OpenIfExists));
}
```

Call at startup in `RetroCore::Initialize()`.

### 5. App.cpp changes

Pass filename from picker to LoadRom:

```cpp
// In App.cpp F1 handler
create_task(picker->PickSingleFileAsync()).then([this](StorageFile^ file)
{
    if (file)
    {
        auto filename = file->Name;  // e.g., "game.zip"
        return create_task(FileIO::ReadBufferAsync(file))
            .then([this, filename](IBuffer^ buffer)
            {
                m_retroCore->LoadRom(buffer, filename);
            });
    }
});
```

## No Changes Needed

| File | Change | Reason |
|------|--------|--------|
| `dosbox_pure_libretro.cpp` | None | `fopen_wrap()` unchanged — CRT works on LocalFolder |
| `cross.cpp` | None | `open_directory()` unchanged — CRT works on LocalFolder |
| `vfs_implementation_uwp.cpp` | None | Only needed for MIDI scan — unrelated |
| `SdlInput.cpp` | None | No file I/O |
| `RetroScreenRenderer.cpp` | None | No file I/O |

## Edge Cases

### Unicode filenames
Picker returns `StorageFile::Name` as `Platform::String^` (UTF-16). Convert to UTF-8 for `info->path`. CRT `fopen()` on Windows handles UTF-8 if using the ANSI codepage — but safer to convert to UTF-16 and use `_wfopen`... Actually, CRT `fopen()` on LocalFolder paths with ASCII-only names works fine. For Unicode names, use `_wfopen` in `fopen_wrap()`.

**Decision**: If Unicode paths are needed, patch `fopen_wrap()` to call `_wfopen()` instead of `fopen()`. Most ROM filenames are ASCII. Defer unless needed.

### Multiple ROM loads (disk swap)
Old temp file deleted before new one created. If core still has handles open, delay deletion.

### Crash during write
Partial file in LocalFolder. Next load overwrites it. Acceptable.

## Verification
1. Pick `.zip` ROM → loads in emulator
2. Puremenu file browser shows ROM contents
3. Save state creates file in `LocalFolder\saves\`
4. FRONTEND.DBP in `LocalFolder\`
5. DebugView shows `[dosbox-uwp]` temp path
6. Build: 0 errors Release|x64
