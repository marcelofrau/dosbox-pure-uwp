# References — Research Findings

---

## Upstream Repositories

| Repo | URL | Role |
|------|-----|------|
| **dosbox-pure** (core) | https://github.com/schellingb/dosbox-pure | Emulation core (libretro). Submodule em `extern/dosbox-pure/` |
| **dosbox-pure** (docs oficial) | https://docs.libretro.com/library/dosbox_pure/ | Manual de uso, teclas, configuração, FAQ |
| **dosbox-pure-unleashed** | https://github.com/schellingb/dosbox-pure-unleashed | Standalone desktop frontend (ZillaLib + main.cpp). **Não vamos usar** — só referência |
| **RetroArch** (UWP fork) | https://github.com/XboxEmulationHub/RetroArch | Referência para VFS UWP, frontend UWP, ciclo de vida |
| **ZillaLib** | Sibling do unleashed | Platform layer (SDL + OpenGL). **Não vamos usar** — substituído pelo scaffold UWP |

---

## ZillaLib Analysis (unleashed frontend)

ZillaLib é uma camada C++ cross-platform que usa SDL + OpenGL internamente. Referências no `main.cpp`:

```
ZL_Application, ZL_Display, ZL_Input, ZL_Audio
ZL_File, ZL_Compression, ZL_Json, ZL_Network
ZL_Font, ZL_Checksum, ZL_Easing, ZL_Math, ZL_Math3D
ZL_Joystick, ZL_Mutex, ZL_Client, ZL_Peer, ZL_Packet
```

**Conclusão**: `main.cpp` (155KB) usa ZillaLib extensivamente para tudo. Nosso scaffold UWP substitui:
- `ZL_Application/Display` → DirectX UWP App
- `ZL_Input` → SdlInput (já temos)
- `ZL_Audio` → SDL audio (já temos)
- `ZL_File` → VFS UWP (copiado do RetroArch)
- `ZL_Compression` → Se o core precisar, `miniz` ou chamada VFS
- `ZL_Json` → Se precisar, `nlohmann/json` ou manual
- `ZL_Font` → DirectWrite (já temos no SampleFpsTextRenderer)
- `ZL_Network` → Netplay futuro

**O core (`dosbox_pure_libretro.cpp`) NÃO chama ZillaLib.** Só o frontend (`main.cpp`) usa. Portanto ZillaLib não precisa ser portado.

---

## RetroArch UWP VFS Implementation

Arquivo: `libretro-common/vfs/vfs_implementation_uwp.cpp` (28KB, 945 linhas)

### Como funciona
- Usa `CreateFile2FromApp()` da Win10 SDK (header `<fileapifromapp.h>`)
- Essa função permite acesso a arquivos dentro do sandbox UWP sem precisar de `FileOpenPicker`
- Compatível com caminhos relativos a `LocalFolder`, `InstalledLocation`, e `RemovableStorage`
- Suporte a SMB (network share) via `#ifdef HAVE_SMBCLIENT`

### Funções implementadas

| Função | Descrição |
|--------|-----------|
| `retro_vfs_file_open_impl` | fopen com CreateFile2FromApp |
| `retro_vfs_file_close_impl` | fclose |
| `retro_vfs_file_read_impl` | fread |
| `retro_vfs_file_write_impl` | fwrite |
| `retro_vfs_file_seek_impl` | fseek |
| `retro_vfs_file_tell_impl` | ftell |
| `retro_vfs_file_size_impl` | GetFileSizeEx |
| `retro_vfs_file_truncate_impl` | SetEndOfFile |
| `retro_vfs_file_remove_impl` | DeleteFileFromApp |
| `retro_vfs_file_rename_impl` | MoveFileFromApp |
| `retro_vfs_stat_impl` | GetFileAttributesEx |
| `retro_vfs_mkdir_impl` | CreateDirectory |
| `retro_vfs_opendir_impl` | FindFirstFileEx |
| `retro_vfs_readdir_impl` | FindNextFile |
| `retro_vfs_closedir_impl` | FindClose |
| `uwp_set_acl` | Define ACL (permissões) |

### Files auxiliares necessários

| File | Tamanho | Função |
|------|---------|--------|
| `uwp/uwp_file_handle_access.h` | 3KB | Wrappers para acesso a arquivos UWP |
| `uwp/uwp_func.h` | 2KB | Funções auxiliares UWP |
| `uwp/std_filesystem_compat.h` | 2KB | Compatibilidade std::filesystem no UWP |
| `uwp/uwp_async.h` | 5KB | Async helpers |

### Includes que o vfs_implementation_uwp.cpp usa
```
<ppl.h>, <ppltasks.h>, <wrl.h>, <wrl/implements.h>
<robuffer.h>, <fileapifromapp.h>, <AclAPI.h>, <sddl.h>
<io.h>, <fcntl.h>
<vfs/vfs.h>, <vfs/vfs_implementation.h>
<libretro.h>, <encodings/utf.h>, <compat/strl.h>
<retro_miscellaneous.h>, <file/file_path.h>
<retro_environment.h>, <uwp/std_filesystem_compat.h>
```

Muitos desses headers (`encodings/`, `compat/`, `file/`, `retro_miscellaneous.h`) são do próprio `libretro-common`. Decisão: copiar seletivamente ou submodular `libretro-common` inteiro.

---

## Libretro Interfaces

### Video refresh callback
```c
typedef void (*retro_video_refresh_t)(const void *data, unsigned width,
                                      unsigned height, size_t pitch);
```
- `data != NULL` → framebuffer pixels em RAM (SW render — nosso path)
- `data == NULL` → HW render, core já desenhou no framebuffer OpenGL
- `data == RETRO_HW_FRAME_BUFFER_VALID` → HW render com framebuffer válido
- Formato: `XRGB8888` (32bpp) ou `RGB565` (16bpp) definido por `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`

### Audio callback
```c
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *samples,
                                             size_t frames);
```
- PCM 16-bit signed, interleaved stereo (L,R pairs)
- Frames = número de pares estéreo

### Input callbacks
```c
typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device,
                                       unsigned index, unsigned id);
```
- `port` = player index (0-4)
- `device` = `RETRO_DEVICE_JOYPAD`, `RETRO_DEVICE_ANALOG`, `RETRO_DEVICE_KEYBOARD`, etc.
- `id` = button index (`RETRO_DEVICE_ID_JOYPAD_A`, `RETRO_DEVICE_ID_JOYPAD_B`, etc.)
- Retorna 0 ou 1 (digital) ou -32768 a 32767 (analógico)

### Environment interface
```c
typedef bool (*retro_environment_t)(unsigned cmd, void *data);
```
Cmd keys que o core do DOSBox Pure usa (identificadas no `main.cpp` do unleashed):

| CMD | Nosso handler |
|-----|---------------|
| `GET_VFS_INTERFACE` | ✅ Entregar nossa VFS (v3) |
| `GET_VARIABLE` | ✅ Responder config defaults |
| `SET_VARIABLE` | ✅ Aceitar |
| `GET_SYSTEM_DIRECTORY` | ✅ `LocalFolder` |
| `GET_SAVE_DIRECTORY` | ✅ `LocalFolder` |
| `SET_HW_RENDER` | ✅ Retornar 0 (rejeitar HW, forçar SW) |
| `SET_KEYBOARD_CALLBACK` | ✅ Push held key states |
| `SET_MESSAGE_EXT` | ✅ Log via OutputDebugStringA |
| `GET_THROTTLE_STATE` | ✅ VSYNC (refresh) quando vsync on && refresh<core_fps; senão NONE (core_fps) |
| `SET_NETPACKET_INTERFACE` | ⏳ Futuro (netplay) |
| `GET_USERNAME` | ☑️ Opcional |
| `GET_LANGUAGE` | ☑️ Opcional |
| `SET_FRAME_TIME_CALLBACK` | ☑️ Opcional |
| `SHUTDOWN` | ✅ Aceitar (logar) |

### VFS Interface struct
```c
struct retro_vfs_interface {
  retro_vfs_file_open_t        open;
  retro_vfs_file_close_t       close;
  retro_vfs_file_size_t        size;
  retro_vfs_file_tell_t        tell;
  retro_vfs_file_seek_t        seek;
  retro_vfs_file_read_t        read;
  retro_vfs_file_write_t       write;
  retro_vfs_file_flush_t       flush;
  retro_vfs_file_remove_t      remove;
  retro_vfs_file_rename_t      rename;
  retro_vfs_file_truncate_t    truncate;
  retro_vfs_file_stat_t        stat;
  retro_vfs_file_mkdir_t       mkdir;
  retro_vfs_file_opendir_t     opendir;
  retro_vfs_file_readdir_t     readdir;
  retro_vfs_file_dirent_get_name_t dirent_get_name;
  retro_vfs_file_dirent_is_dir_t    dirent_is_dir;
  retro_vfs_file_closedir_t    closedir;
};
```

Preenchido pelo frontend e passado ao core via `RETRO_ENVIRONMENT_GET_VFS_INTERFACE`.

---

## UWP File System Sandbox

Paths acessíveis no UWP (sem precisar de capabilities especiais):

| Path | Como obter |
|------|-----------|
| `InstalledLocation` | `Package.Current.InstalledLocation` (read-only) |
| `LocalFolder` | `ApplicationData.Current.LocalFolder` (read/write) |
| `RoamingFolder` | `ApplicationData.Current.RoamingFolder` (sync entre devices) |
| `TempFolder` | `ApplicationData.Current.TemporaryFolder` |
| `RemovableStorage` | Requer `removableStorage` capability |

A VFS UWP do RetroArch usa `CreateFile2FromApp()` que resolve paths relativos automaticamente dentro do sandbox, seguindo a mesma lógica de search order do `LocalFolder` e `InstalledLocation`.

Para Xbox Dev Mode, o deploy copia o MSIX + dependencies + cert para o console via Device Portal. A instalação segue o fluxo padrão de sideloading UWP.

---

## Notas sobre o VCXPROJ do unleashed

O projeto `DOSBoxPure-vs.vcxproj` referência:
- `$(DBPDir)` = `../dosbox-pure/` (core)
- `$(ZillaLibDir)` = `../ZillaLib/` (platform layer)
- Usa `ZillaApp-vs.props` importado de ZillaLib
- Defines: `__LIBRETRO__`, `DBP_STANDALONE`, `_CRT_SECURE_NO_WARNINGS`
- ~120 source files do core compilados como parte do mesmo projeto

**Para integrar no nosso vcxproj UWP**: replicar a lista de `ClCompile Include="$(DBPDir)..."` do unleashed, mas **sem** os includes do ZillaLib.

---

## Build Findings — Core Compilation na UWP

### Include Paths
No `dosbox-uwp.vcxproj`, adicionar no `ClCompile.AdditionalIncludeDirectories`:
- `$(DBPDir)` (root do core: `..\extern\dosbox-pure\`)
- `$(DBPDir)include` (headers do core: `dosbox.h`, `cross.h`, etc.)
- `$(LrCommonDir)include` (`..\extern\libretro-common\include`)

### Preprocessor Defines (ambos Debug e Release)
```
__LIBRETRO__;DBP_STANDALONE;_CRT_SECURE_NO_WARNINGS;_CRT_NONSTDC_NO_DEPRECATE
```

### Per-File Overrides Obrigatórios (para todos os .cpp do core)
```xml
<PrecompiledHeader>NotUsing</PrecompiledHeader>     <!-- core files não têm pch.h -->
<CompileAsWinRT>false</CompileAsWinRT>               <!-- sem /ZW, core é C++ puro -->
<DisableSpecificWarnings>4703</DisableSpecificWarnings>  <!-- /sdl trata C4703 como erro -->
```

### Problemas Conhecidos
1. **C4996 (POSIX names)**: resolvido com `_CRT_NONSTDC_NO_DEPRECATE`
2. **C2664 (Platform::String^)**: resolvido com `CompileAsWinRT=false` (desliga C++/CX por arquivo)
3. **C4703 (uninitialized pointer)**: resolvido com `DisableSpecificWarnings=4703` (código do core inicializa condicionalmente, MSVC /sdl é agressivo)
4. **SDL.h**: resolvido buildando via `.sln` (não `.vcxproj` direto) para `$(SolutionDir)` resolver corretamente no `uwp-dep.props`

### Estratégia para Adicionar Core Files
- Arquivos problemáticos (ex: `cross.cpp`, `drive_local.cpp`, `midi.cpp`) podem ser **excluídos do build** temporariamente com `<ExcludedFromBuild>true</ExcludedFromBuild>`
- Foco inicial: adicionar todos os ~110 .cpp, resolver link errors incrementalmente
- O unleash usa `<WarningLevel>Level2</WarningLevel>` para quase todos os core files — no nosso caso `DisableSpecificWarnings` + `CompileAsWinRT=false` é suficiente

---

## Erros Encontrados & Correções (Phase 2-3)

### 1. D2D: BeginDraw/EndDraw obrigatório
**Sintoma:** Crash no debug layer: `"An attempt to render a primitive outside of BeginDraw/EndDraw"`
**Causa:** `ID2D1RenderTarget::DrawBitmap()` chamado sem `BeginDraw()` antes.
**Solução:** Envolver toda renderização em `BeginDraw()`/`EndDraw()`. `BeginDraw()` retorna `void`.
**Referência:** `dosbox-uwp/Content/RetroScreenRenderer.cpp:82`

### 2. Framebuffer vazio (pitch = 0) = HW frame
**Sintoma:** `memcpy` com tamanho 0 crasha (access violation)
**Causa:** Core envia `retro_video_cb(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)` quando usa HW path.
  `RETRO_HW_FRAME_BUFFER_VALID` é `(void*)~0` ≈ 0xFFFFFFFFFFFFFFFF; `pitch = 0` sinaliza "frame HW".
**Solução:** Guard `if (pitch == 0) return` — loga e descarta frames HW.
**Referência:** `dosbox-uwp/Content/RetroCore.cpp:354-368`

### 3. Cores distorcidas no D2D bitmap
**Sintoma:** Cores erradas (ex: vermelho vira laranja, azul vira roxo)
**Causa:** `D2D1_ALPHA_MODE_PREMULTIPLIED` assume alpha premultiplicado. Framebuffer XRGB8888 é raw.
**Solução:** Usar `D2D1_ALPHA_MODE_IGNORE` ao criar bitmap.
**Referência:** `dosbox-uwp/Content/RetroScreenRenderer.cpp:130-138`

### 4. D2DERR_BITMAP_BOUND_AS_TARGET em high-DPI
**Sintoma:** Erro D2D ao criar bitmap, ou render artifacts
**Causa:** DPI do bitmap (hardcoded 96.0f) != DPI real do render target
**Solução:** Usar `d2dContext->GetDpi(&dpiX, &dpiY)` e passar valores no `D2D1_BITMAP_PROPERTIES1`.
**Referência:** `dosbox-uwp/Content/RetroScreenRenderer.cpp:148`

### 5. STA thread: .get() em WinRT async
**Sintoma:** `Concurrency::invalid_operation`: "Illegal to wait on a task in a Windows Runtime STA"
**Causa:** `concurrency::create_task(Windows::Storage::FileIO::ReadBufferAsync(file)).get()` da STA thread.
  UWP STA não permite bloquear (`.get()`) em async operations WinRT.
**Solução:** Usar continuation chaining (`.then()`) sem `.get()`. Ou fire-and-forget.
**Referência:** `dosbox-uwp/App.cpp:215-236`

### 6. AccessDenied ao reabrir arquivo por path
**Sintoma:** `Platform::AccessDeniedException` (HRESULT 0x80070005)
**Causa:** `StorageFile::GetFileFromPathAsync(arbitraryPath)` falha — UWP não pode abrir
  arquivo por caminho arbitrário (sandbox).
**Solução:** Ler o arquivo na continuation do `FileOpenPicker` usando `FileIO::ReadBufferAsync(file)`,
  onde `file` é o `StorageFile^` retornado pelo picker (carrega direitos de acesso).
  Passar dados como `std::vector<uint8_t>` para `LoadGame`.
**Referência:** `dosbox-uwp/App.cpp:215-236`, `dosbox-uwp/Content/RetroCore.cpp:80-120`

### 7. SET_HW_RENDER retornar 1 causa GL crash
**Sintoma:** Core tenta usar OpenGL paths e crasha (sem contexto GL)
**Causa:** `DBP_STANDALONE` faz core tentar HW render primeiro. Se `SET_HW_RENDER` retorna 1,
  core assume que frontend suporta HW e usa GL framebuffer.
**Solução:** Retornar 0 em `retro_env` para `RETRO_ENVIRONMENT_SET_HW_RENDER`. Core faz fallback
  automático para SW path.
**Referência:** `dosbox-uwp/Content/RetroCore.cpp:308-311`

### 8. DPI mismatch entre C++/CX e D2D
**Sintoma:** `ID2D1DeviceContext` reporta DPI diferente do esperado
**Causa:** O DPI pode ser 96 (100%) em um monitor, 144 (150%) em outro, ou valor diferente
  do DisplayInformation::LogicalDpi.
**Solução:** Sempre consultar `d2dContext->GetDpi()` ao criar bitmaps. Nunca hardcodar.
**Referência:** `dosbox-uwp/Content/RetroScreenRenderer.cpp:148`

---

## D2D Findings

### Direct2D vs Direct3D para video output
**Decisão:** Usamos Direct2D (não Direct3D) para renderizar o framebuffer do core.

**Motivos:**
- `ID2D1Bitmap1::CopyFromMemory()` aceita XRGB8888 raw diretamente
- `DrawBitmap()` com `D2D1_INTERPOLATION_MODE_LINEAR` escala automaticamente
- Não precisa de vertex/pixel shaders, não precisa de `D3D11_USAGE_DYNAMIC`/`Map()`/`Unmap()`
- Letterbox via `D2D1_RECT_F` source/dest rectangles
- O scaffold UWP já cria `ID2D1DeviceContext` (via `CreateD2DDeviceContext()`)

**Pipeline D2D:**
1. Criar `ID2D1Bitmap1` com `D2D1_BITMAP_PROPERTIES1`:
   - `pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM`
   - `pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE`
   - `dpiX = dpiY` do `GetDpi()` do context
   - `bitmapOptions = D2D1_BITMAP_OPTIONS_NONE`
2. `UpdateVideoFrame()`: `bitmap->CopyFromMemory(nullptr, data, pitch)`
3. `Render()`: `BeginDraw()` → `DrawBitmap()` com letterbox → `EndDraw()`

**Cuidados:**
- `BeginDraw()` retorna `void`, `EndDraw()` retorna `HRESULT`
- DPI do bitmap DEVE bater com DPI do render target
- Pixel format `IGNORE` (não `PREMULTIPLIED`) para XRGB8888

---

## UWP Async Findings

### WinRT STA não permite .get()
**Regra:** `concurrency::create_task(WinRT IAsyncOperation^).get()` da STA thread (UI thread)
lança `Concurrency::invalid_operation` com mensagem "Illegal to wait on a task in a Windows Runtime STA".

**Por quê:** UWP STA usa `CoreDispatcher` para processar mensagens. `.get()` bloquearia a thread
impedindo o dispatcher de processar a conclusão do async operation. Deadlock potencial.

**Como contornar:**
1. **Continuation chaining:**
   ```cpp
   create_task(picker->PickSingleFileAsync())
       .then([](StorageFile^ file) {
           if (file != nullptr) {
               create_task(FileIO::ReadBufferAsync(file))
                   .then([](IBuffer^ buffer) {
                       // process buffer
                   });
           }
       });
   ```
2. **Fire-and-forget:** retornar `void` do lambda, não esperar resultado
3. **NUNCA** chamar `.get()` em `IAsyncOperation<T>` da STA

### UWP file access limitations
**Acesso direto:** UWP NÃO pode acessar paths arbitrários via `fopen` ou `GetFileFromPathAsync`.
**Solução:** Sempre usar o `StorageFile^` de um `FileOpenPicker` (que carrega direitos).
**Leitura:** `FileIO::ReadBufferAsync(file)` → `DataReader::FromBuffer(buffer)` → `ReadBytes()`.
**Dados:** Copiados para `std::vector<uint8_t>` e passados para `retro_game_info.data`/`size`.
