# Dynarec (JIT) on UWP: Status and Roadmap

## Current Status: DISABLED

The dynamic recompiler (dynarec / JIT) is disabled via `DISABLE_DYNAREC` preprocessor
define. DOSBox Pure falls back to the interpreted CPU core (`CPU_Core_Normal_Run`).

## Why Disabled

### Crash: Access Violation in gen_runcode

Pressing Space in the DOS shell triggered:

```
Exception 0xC0000005 at CPU_Core_Dyn_X86_Run() line 307
    BlockReturn ret = gen_runcode(block->cache.start);
```

`block->cache.start` points to memory allocated in `dyn_cache.h:708`:

```cpp
cache_code_start_ptr = (Bit8u*)VirtualAlloc(0, ..., MEM_COMMIT, PAGE_EXECUTE_READWRITE);
if (!cache_code_start_ptr)
    cache_code_start_ptr = (Bit8u*)malloc(...);
```

On UWP (AppContainer):
1. `VirtualAlloc` returns NULL (blocked for AppContainer processes)
2. Falls through to `malloc()` — allocates read/write memory, NOT executable
3. Dynarec tries to jump to generated x86 code → access violation

### Additional UWP Dynarec Risks

Even with a correct `VirtualAllocFromApp` call, UWP's JIT restrictions may cause
further issues:

- **Store Verification**: UWP validates dynamically generated code. Certain
  instruction sequences may be rejected at runtime.
- **VirtualProtectFromApp**: Marking memory as executable requires this API.
  Compatibility depends on Windows version and Xbox security policy.
- **Xbox Series S|X**: JIT compilation policies on Xbox are stricter than desktop
  UWP. Unknown if dynarec would pass certification.

## Performance Impact

| Core type | Relative speed | Suitable for |
|-----------|---------------|--------------|
| Dynarec   | ~5-10x        | All DOS games, esp. post-1995 (GTA, Duke3D, Quake) |
| Interp.   | 1x (baseline) | Games up to ~1995 (Arkanoid, Doom, Commander Keen, etc.) |

Interpreted core runs most games from the DOS era at playable speeds on modern
x64 hardware. Late-90s titles using DOS4GW or protected-mode DOS extenders may
struggle.

## Re-enabling Dynarec

### Prerequisites

1. **Fix the allocation**: Replace `VirtualAlloc` with `VirtualAllocFromApp` in
   `extern/dosbox-pure/src/cpu/dyn_cache.h:708`. Both have the same signature:
   ```
   LPVOID VirtualAllocFromApp(LPVOID lpAddress, SIZE_T dwSize,
                              DWORD flAllocationType, DWORD flProtect);
   ```

2. **Patch location**: Since `extern/dosbox-pure/` is a git submodule (never commit
   directly), copy the file to `dosbox-uwp/local/dosbox-pure/src/cpu/dyn_cache.h`
   and adjust the include path in `.vcxproj`.

### Include path strategy

`core_dyn_x86.cpp` includes `dyn_cache.h` with `#include "dyn_cache.h"` (quotes).
MSVC searches the source file's directory first, then include directories. To
override, either:

- **Plan A**: Copy `core_dyn_x86.cpp` to local as well, change include path.
- **Plan B**: Use `/FI"Content/uwp_mem_fix.h"` forced include with
  `#define VirtualAlloc VirtualAllocFromApp`. Risky due to macro timing.
- **Plan C**: Put a wrapper `dyn_cache.h` at the project level and add its
  directory to `AdditionalIncludeDirectories` *before* `$(DBPDir)src`.

### Verification

After patching:
1. Remove `DISABLE_DYNAREC` from `PreprocessorDefinitions` in `.vcxproj`
2. Build Release|x64
3. Test with a .dosz game that triggered the crash (e.g., Arkanoid via Space)
4. Monitor for `gen_runcode` access violations
5. Test on Xbox Series if targeting console

## Related Files

| File | Role |
|------|------|
| `extern/dosbox-pure/include/config.h:108` | `DISABLE_DYNAREC` guard |
| `extern/dosbox-pure/src/cpu/dyn_cache.h:708` | `VirtualAlloc` → fix target |
| `extern/dosbox-pure/src/cpu/core_dyn_x86.cpp:307` | Crash site: `gen_runcode` |
| `dosbox-uwp/dosbox-uwp.vcxproj` | Define location |
