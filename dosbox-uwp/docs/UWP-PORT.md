# dosbox-pure — UWP Port Status

> **⚠️ HISTORICAL (Phase 2-3).** This document describes the original port effort.
> Much has changed since: audio (XAudio2 4×12ms ring + QPC pacing), fullscreen
> FrontendMenu (8 pages), gamepad mouse emulation, OSD 640x480 render path, hotkeys
> (F10/F12/Ctrl+L/Alt), settings persistence. See **`README.md`** and
> **`docs/ARCHITECTURE.md`** for the current state.

## Build Status

**Build: ✅ 0 errors** (Release|x64, ~1500 warnings C4244 cosméticos)
**Linker: ✅ 0 unresolved**
**Deploy: ✅ Renderiza na tela via VS2022 (Windows 11, x64). ❌ Xbox Series não testado ainda.**

## Estrutura

```
dosbox-uwp/
├── dosbox-uwp.vcxproj              — projeto principal (UWP AppX)
├── dosbox-uwp.sln                  — solução (requerida para $(SolutionDir) no x64)
├── Package.appxmanifest            — identidade do pacote UWP
├── App.cpp / App.h                 — entry point UWP, picker F1, async load
├── dosbox_uwpMain.cpp / .h         — game loop, core lifecycle, input routing
├── dosbox_pure_sta.cpp             — [CRIADO] stubs DBPS_* standalone (9 funções + 2 globais)
├── uwp-dep.props                   — dependências SDL/uwp (x64 apenas)
│
├── Content/
│   ├── RetroCore.cpp / .h          — [NOVO] bridge libretro completa (init, load, run, callbacks, VFS)
│   ├── RetroScreenRenderer.cpp / .h— [NOVO] render D2D com letterbox, BeginDraw/EndDraw
│   ├── SdlInput.h/cpp              — gamepad + audio via SDL
│   ├── Sample3DSceneRenderer.*     — fallback 3D (usado quando sem ROM)
│   └── SampleFpsTextRenderer.*     — overlay debug
│
├── local/dosbox-pure/              — [CRIADO] patches locais (submodule intocado)
│   ├── dosbox_pure_libretro.cpp    — bridge libretro (copiado, build via local)
│   └── src/
│       ├── dbp_serialize.cpp       — C2051 fix (switch __LINE__ → array)
│       ├── dos/drive_cache.cpp     — C2664 guard (GetVolumeInformationA UWP)
│       ├── gui/midi.cpp            — C2664 guard (midi_win32.h UWP)
│       └── misc/cross.cpp          — C2664 guard (FindFirstFile UWP) + stubs diretório
│
├── docs/
│   └── UWP-PORT.md                 — este documento
│
├── libretro-common/
│   ├── include/
│   │   ├── compat/msvc.h           — [CRIADO] strcasecmp→_stricmp, ssize_t, etc
│   │   └── retro_inline.h          — [CRIADO] macro INLINE
│   ├── vfs/
│   │   └── vfs_implementation_uwp.cpp — [CRIADO] stub VFS (StorageFile pendente)
│   ├── uwp/
│   │   ├── std_filesystem_compat.h
│   │   ├── uwp_async.h
│   │   ├── uwp_file_handle_access.h
│   │   └── uwp_func.h
│   ├── compat/compat_strl.c        — [CRIADO] strlcpy/strlcat
│   └── encodings/encoding_utf.c    — [CRIADO] UTF-8/16 conversion
│
├── Content/                        — DirectX shaders
├── Common/                         — DirectX helpers
├── Assets/                         — ícones/logos AppX
└── pch.h / pch.cpp                 — precompiled header

extern/
├── dosbox-pure/                    — submodule (pristine, commit 65dd07d)
└── libretro-common/                — submodule
```

## Estratégia de Patches

**Decisão:** Submodule `extern/dosbox-pure/` permanece intocável (pristine).
Arquivos que precisam de modificação são **copiados** para `dosbox-uwp/local/dosbox-pure/`
com a mesma estrutura de diretórios, e o vcxproj aponta para o local.

```
$(DBPDir)src\gui\midi.cpp                    → local\dosbox-pure\src\gui\midi.cpp
$(DBPDir)src\dos\drive_cache.cpp             → local\dosbox-pure\src\dos\drive_cache.cpp
$(DBPDir)src\misc\cross.cpp                  → local\dosbox-pure\src\misc\cross.cpp
$(DBPDir)src\dbp_serialize.cpp               → local\dosbox-pure\src\dbp_serialize.cpp
$(DBPDir)dosbox_pure_libretro.cpp            → local\dosbox-pure\dosbox_pure_libretro.cpp
```

**Próximos arquivos que precisarem de patch** devem seguir o mesmo padrão.

---

## Includes no vcxproj

```
$(ProjectDir)                          — dosbox-uwp\
$(IntermediateOutputPath)              — x64\Release\ (precompiled headers)
$(DBPDir)                              — extern\dosbox-pure\
$(DBPDir)include                       — dosbox.h, etc
$(DBPDir)libretro-common\include       — libretro.h (versão do dosbox-pure)
$(DBPDir)src                           — headers raiz de src/
$(DBPDir)src\gui                       — midi_oss.h, midi_alsa.h (para local\midi.cpp)
$(DBPDir)src\misc                      — cross.h (para local\cross.cpp)
$(DBPDir)src\dos                       — drives.h, etc (para local\drive_cache.cpp)
$(LrCommonDir)                         — uwp/*.h
$(LrCommonDir)include                  — retro_miscellaneous.h, retro_environment.h, etc
```

**Nota:** `$(DBPDir)src\gui;$(DBPDir)src\misc;$(DBPDir)src\dos` adicionados para que
arquivos locais (`local\dosbox-pure\src\gui\midi.cpp` etc.) consigam incluir headers
irmãos via `#include "midi_oss.h"` — que antes resolviam pelo diretório do source original.

---

## Modificações nos Arquivos Locais

### 1. MIDI — guard UWP

**Arquivo:** `local/dosbox-pure/src/gui/midi.cpp:93-95`

```cpp
// Original:
#elif defined(WIN32)
// Patch:
#elif defined(WIN32) && (!defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_APP)
```

**Motivo:** UWP não tem `winmm.lib`/`mmeapi.h` (API Win32 MIDI).
**Impacto:** MIDI Win32 desligado no UWP. Compila `midi_oss.h` no `#else` (não funcional).

---

### 2. C4703 — suprimido globalmente

**Decisão:** Ao invés de patchear 5 headers do submodule, C4703 é suprimido globalmente
no vcxproj via `DisableSpecificWarnings`:

```xml
<DisableSpecificWarnings>4453;28204;4703</DisableSpecificWarnings>
```

**Afetados (não precisam mais de patch):**
- `src/cpu/core_dyn_x86/string.h` — `tmp_reg`, `rep_ecx_jmp`
- `src/cpu/core_dyn_x86/decoder.h` — `segbase`
- `src/cpu/core_dyn_x86/risc_x64.h` — `func`
- `src/dbp_network.cpp` — `str`, `code`

**Motivo:** MSVC UWP usa análise de fluxo mais agressiva. Variáveis são sempre setadas
antes do uso; warnings são falsos positivos.

---

### 3. C2051 — case expression not constant

**Arquivo:** `local/dosbox-pure/src/dbp_serialize.cpp:302-360`

**Problema:** `case __LINE__:` não aceito como constante pelo MSVC no toolchain UWP.

**Solução:** `switch(__LINE__)` substituído por array de structs:

```cpp
struct { void (*setup)(DBPArchive&); bool (*check)(DBPArchive&); } entries[] = {
    DBPSERIALIZE_ENTRY(DBPSerialize_DOS),
    DBPSERIALIZE_ENTRY_FVER(DBPSerialize_Mounts, >=8),
    // ... todas as 36 funções serialize
};
```

**Forward declarations** via macro `DBPSERIALIZE_DECL`.
**Impacto:** Comportamento idêntico — preserva ordem, version checks, layout.

---

### 4. C2664 — Win32 APIs ANSI no UWP

#### drive_cache.cpp

**Arquivo:** `local/dosbox-pure/src/dos/drive_cache.cpp:140-148`

```cpp
// Original:
#if defined(WIN32) || defined(OS2)
// Patch:
#if (defined(WIN32) || defined(OS2)) && (!defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_APP)
```

**APIs removidas:** `GetVolumeInformationA`, `GetDriveTypeA`
**Impacto:** Volume label e CD-ROM detection desligados no UWP.

#### cross.cpp — diretórios

**Arquivo:** `local/dosbox-pure/src/misc/cross.cpp:188`

```cpp
// Original:
#if defined (WIN32)
// Patch:
#if defined (WIN32) && (!defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_APP)
```

**APIs removidas:** `FindFirstFile`, `FindNextFile`, `FindClose`
**Stubs UWP adicionados** (`#elif defined(WIN32)`):

| Função | Stub |
|---|---|
| `open_directory` | `return NULL` |
| `read_directory_first` | `return false` |
| `read_directory_next` | `return false` |
| `close_directory` | `no-op` |

**Impacto:** Directory enumeration não funciona no UWP via Win32.
Usar VFS (`retro_vfs_file_*`, `retro_vfs_dir_*`).

---

### 5. Libretro-common compat — headers criados

| Header | Conteúdo |
|---|---|
| `compat/msvc.h` | `strcasecmp→_stricmp`, `strncasecmp→_strnicmp`, `ssize_t`, `PATH_MAX`, `SIZE_MAX`, `mkdir→_mkdir` |
| `retro_inline.h` | `#define INLINE __inline` (Win32) |
| `compat/compat_strl.c` | `strlcpy`, `strlcat` |
| `encodings/encoding_utf.c` | UTF-8/16 conversion |

Ausentes no checkout atual do libretro-common.

---

## Linker: DBPS_* — resolvido (16 → 0)

Adicionados 2 arquivos ao build:

| Arquivo | Símbolos |
|---|---|
| `extern/dosbox-pure/keyb2joypad.cpp` | `map_keys`, `map_buckets` |
| `dosbox-uwp/dosbox_pure_sta.cpp` | `DBPS_SaveSlotIndex`, `DBPS_BrowsePath`, `DBPS_OnContentLoad`, `DBPS_SubmitOSDFrame`, `DBPS_GetMouse`, `DBPS_StartCaptureJoyBind`, `DBPS_HaveJoy`, `DBPS_GetJoyBind`, `DBPS_RequestSaveLoad`, `DBPS_HaveSaveSlot`, `DBPS_ApplyConfigOverrides`, `DBPS_IsConfigOverride`, `DBPS_ToggleConfigOverride`, `DBPS_GetConfigOverrideJSON` |

**7 DBPS_* fornecidos por `dosbox_pure_osd.h`** (incluído por `dosbox_pure_libretro.cpp` quando `DBP_STANDALONE`):
`DBPS_AddDisc`, `DBPS_OpenContent`, `DBPS_IsGameRunning`, `DBPS_IsShowingOSD`,
`DBPS_GetContentName`, `DBPS_ToggleOSD`, `DBPS_InitLEDs`

**Stubs** (`dosbox_pure_sta.cpp`) compilam mas não implementam — retornam valores default
ou no-op. Implementação real (SDL2 input, DirectX OSD, WinRT file picker) pendente.

---

## Próximos Passos

### Prioridade 1 — áudio + estabilidade

- [ ] **Áudio**: Roteir `retro_audio_sample_batch` → XAudio2 (atualmente SDL QueueAudio, mas sem fone)
- [ ] **Async loading UX**: Mostrar status de carregamento (spinner/texto) enquanto ROM é lida e
      processada (demora ~0.5-2s)
- [ ] **Keyboard mapping completo**: Mapear todas as teclas UWP → scancodes libretro (atualmente só
      alfanumérico + ENTER/ESC/BACK/TAB/SHIFT/CTRL/ALT/SETAS)

### Prioridade 2 — features

- [ ] **Input mapping**: Mapear botões SDL gamepad → libretro `RETRO_DEVICE_JOYPAD`
- [ ] **DBPS_* reais**: Implementar `OpenContent`, `AddDisc`, `ToggleOSD`, `SaveSlot` etc.
- [ ] **MIDI OSS**: Stub ou implementar `midi_oss.h` compilável no UWP
- [ ] **Directory enum**: Implementar via `StorageFolder::GetFilesAsync`

### Prioridade 3 — deploy & polish

- [ ] **Deploy**: Instalar em device/emulador Windows 10/11, testar performance
- [ ] **Assinar AppX**: Certificado de teste renovado para deploy local
- [ ] **ARM64**: Testar build ARM64 (não configurado)

---

## Change Log

### 2026-07-03 — Phase 2+3: Libretro frontend + D2D video rendering

**O que foi feito:**

1. **RetroCore.cpp/.h** (novo) — bridge libretro completa:
   - `Init()` → `retro_set_environment`, `retro_set_video_refresh`, `retro_set_audio_sample_batch`,
     `retro_set_input_poll`, `retro_set_input_state`, `retro_init`
   - `LoadGame(path, romData)` → passa dados pré-carregados (não tenta reabrir arquivo via UWP),
     chama `retro_load_game`
   - `RunFrame()` → `retro_run()`
   - `retro_env` handler: VFS (v3), SYSTEM/SAVE_DIR (LocalFolder), SET_PIXEL_FORMAT,
     SET_HW_RENDER (rejeitado), SET_MESSAGE_EXT, SET_KEYBOARD_CALLBACK, GET_THROTTLE_STATE
   - Thread-safe `retro_video` callback com lock mutex + buffer
   - `retro_audio` callback com lock mutex
   - `retro_input_poll` + `retro_input_state` (keyboard/joypad/mouse/pointer)

2. **RetroScreenRenderer.cpp/.h** (novo) — render D2D:
   - Cria `ID2D1Bitmap1` do framebuffer XRGB8888
   - `UpdateVideoFrame(data, w, h, pitch)` → copia pixels
   - `Render()` → `BeginDraw()` → `DrawBitmap()` com letterbox → `EndDraw()`

3. **App.cpp** — picker de ROM via FileOpenPicker (F1):
   - Filtros: .zip, .dosz, .exe, .com, .bat, .iso, .chd, .cue, .img, .ima, .vhd, .conf
   - Leitura do arquivo via `FileIO::ReadBufferAsync` dentro da continuation do picker
   - Dados passados como `std::vector<uint8_t>` para `LoadRom` (evita `.get()`)

4. **dosbox_uwpMain.cpp/.h** — loop principal integrado com core:
   - `BootCore()` → `retro_init` no startup
   - `Update()` → `retro_run()` quando core carregado
   - `Render()` → busca frame de `GrabVideoFrame`, desenha com `RetroScreenRenderer`
   - Input routing: CoreWindow KeyDown/Up → `RetroCore::SetKeyState`

**Lições Aprendidas (Erros Encontrados & Correções):**

| Erro | Causa | Solução |
|------|-------|---------|
| D2D crash: "render outside BeginDraw/EndDraw" | `DrawBitmap` sem `BeginDraw()`/`EndDraw()` | Envolver chamadas no par BeginDraw/EndDraw. `BeginDraw()` retorna `void`. |
| AccessViolation em `retro_video` com `pitch=0` | Core envia `RETRO_HW_FRAME_BUFFER_VALID` (pitch=0) quando usa HW path | Guard `if (pitch == 0) return` — loga frames rejeitados |
| Cor distorcida no D2D bitmap | Pixel format `PREMULTIPLIED` assumia alpha premultiplicado | Usar `D2D1_ALPHA_MODE_IGNORE` para XRGB8888 |
| D2DERR_BITMAP_BOUND_AS_TARGET | DPI do bitmap (96.0f hardcoded) != DPI do render target | Usar `d2dContext->GetDpi()` ao criar bitmap |
| `Concurrency::invalid_operation` no LoadGame | `.get()` em `create_task(IAsyncOperation^)` da STA thread | Mover leitura do arquivo para continuation (`.then()`). Sem `.get()` |
| `AccessDeniedException` no `GetFileFromPathAsync` | UWP não pode abrir arquivo por path arbitrário | Usar `StorageFile^` do picker, ler dentro da continuation |
| Core não inicializava | `RETRO_ENVIRONMENT_SET_HW_RENDER` retornava 1 | Retornar 0 para forçar SW rendering path |

**Plataformas Alvo:**
- ✅ **x64** — única plataforma suportada. Testado no VS2022 (Windows 11).
- ❌ **ARM64** — NÃO suportado. Xbox Series é x64, não requer ARM64.
- ❌ **ARM (32-bit)** — NÃO suportado. Obsoleto, sem hardware alvo.
- ❌ **x86 (32-bit)** — NÃO suportado. Sem benefício para Xbox Series.

**Comportamento Atual:**
- ✅ Build 0 errors (Release|x64)
- ✅ Core inicializa (`retro_init` → `retro_load_game`)
- ✅ ROM carregada via picker → dados lidos na continuation → passados para core
- ✅ Frame buffer capturado (`retro_video` → mutex → `GrabVideoFrame`)
- ✅ D2D bitmap criado com DPI correto, pixel format IGNORE
- ✅ Render: BeginDraw → DrawBitmap with letterbox → EndDraw
- ✅ RetroScreenRenderer::Render chamado todo frame
- ✅ Input keyboard mapeado (alfanumérico + ENTER/ESC/BACK/TAB/SHIFT/CTRL/ALT/SETAS)
- ❌ Áudio não emitido (SDL QueueAudio alocado mas sem fone UWP)
- ❌ DBPS_* stubs no-op (OSD, savestates não funcionam)
- ❌ Xbox Series não testado ainda (deploy pendente)

## Notas Técnicas

- **UWP SDK** `10.0.26100.0` (VS2022 v143 toolchain)
- **Plataformas:** somente x64. ARM64, ARM, x86 NÃO suportados (Xbox Series é x64).
- `CompileAsWinRT=false` para código legado (dosbox-pure) — evita conflitos com WinRT
- `AppxPackageSigningEnabled=true` — requer certificado para deploy
- `$(SolutionDir)` necessário no x64 para resolver `uwp-dep.props` (SDL include path)
- Submodule `extern/dosbox-pure/` NÃO recebe commits — patches em `local/dosbox-pure/`
- Qualquer novo arquivo do submodule que precise de modificação: copiar para
  `local/dosbox-pure/` com mesma estrutura, apontar vcxproj
