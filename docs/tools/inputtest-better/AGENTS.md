# AGENTS.md — InputTest Better

## What It Is

DOSBox Pure keyboard + mouse input test utility. Shows real-time key presses on a visual keyboard layout with color-coded states. Uses VGA mode 12h (640×480, 16 colors).

## Purpose

Debug keyboard input in DOSBox Pure — verify every key generates correct scancode, distinguish left/right modifiers, detect held keys, test mouse buttons + coordinates. Development companion for `dosbox-pure-unleashed-uwp` UWP port.

## Keyboard Layout

5 rows mapped to standard 101-key layout:
- Row 0: `~ 1 2 3 4 5 6 7 8 9 0 - = BkSp
- Row 1: Tab Q W E R T Y U I O P [ ] \
- Row 2: Caps A S D F G H J K L ; ' Enter
- Row 3: LSh Z X C V B N M , . / RSh
- Row 4: LCtrl LAlt Space(6w) RAlt RCtrl
- Row 5: F1-F12 Esc

Row 5 line also shows modifier status bar at screen bottom (row 29). "Mnu" key placeholder.

## Visual States

Keys color-coded by state machine `kstate[128]`:
| State | Value | Visual | Meaning |
|-------|-------|--------|---------|
| KS_UP | 0 | Dark gray | Idle |
| KS_GRN | 1 | Light green (fill=10, bor=2) | First press, ~40 frames |
| KS_YLW | 2 | Yellow (fill=14, bor=12) | Held/repeat |
| KS_RED | 3 | Light red (fill=12, bor=4) | Released, ~30 frames |
| — | — | Light gray (fill=7, bor=15) | Was pressed once since boot (`pressed[]`) |

No frame delay — transitions happen at emulator speed.

## Modifier Detection

`poll_mods()` calls **INT 16h AH=12h** (Get Extended Shift States):

```
DOSBox handler: reg_al = BIOS_KEYBOARD_FLAGS1 (basic)
                 reg_ah = BIOS_KEYBOARD_FLAGS2 + FLAGS3 (extended)
```

| Bit | Basic (AL) | Extended (AH) |
|-----|-----------|---------------|
| 0 | RShift | LCtrl |
| 1 | LShift | LAlt |
| 2 | Ctrl(any) | RCtrl |
| 3 | Alt(any) | RAlt |
| 4 | ScrollLock | ScrollLock |
| 5 | NumLock | NumLock |
| 6 | CapsLock | CapsLock |
| 7 | Insert | SysReq |

**INPUT REGISTERS MUST BE ZEROED** before call — DOSBox only sets AL/AH, leaves BX unchanged. Uninitialized BX gives garbage results.

Modifier state transitions set `kstate[sc]`, `log_event`, and `last_str`. Held modifiers keep `hl` above 20 (never expires). Release sets `hl=0, kstate=KS_RED, rel=REL_FRAMES` for proper RED→UP transition.

## Event Log

- 11-line circular buffer below keyboard (`LOG_N=11`)
- Shows per-event: `'A' asc=65 sc=0x1E Down|Hold|Up`
- Named keys: `F1`, `Enter`, `BkSp`, `Tab`, `Esc`, `LShift`, `LCtrl`, etc.
- Logged from: INT 16h reads (regular keys), decrement loop transitions (HLD/UP), `poll_mods` (modifiers)

## Mouse

- INT 33h AX=0 detect, AX=1 show cursor, AX=3 poll
- Right panel (X≥406): diagram with 3 buttons + X,Y coordinates + button states
- Button highlights: red when pressed, dark gray when idle
- Static parts drawn once in `draw_bg`; only numbers + button colors update on movement (no flicker)

## Top Status Bar

Row 1: `'A' asc=65 sc=0x1E DN` or `LShift sc=0x2A UP` — character, ASCII decimal, scancode, event. Updated via `format_last()` helper used by `handle_key` (DN), decrement loop (HLD/UP), and `poll_mods` (modifier DN/UP).

## Key Flow

1. `_bios_keybrd(_KEYBRD_READY/READ)` → INT 16h AH=00h → `handle_key()`
2. `poll_mods()` → INT 16h AH=12h → modifier state transitions
3. Draw loop: redraw keys where `kstate[sc] != prev_ks[sc]`
4. `update_status()` → redraw top line if `last_str` changed
5. Decrement loop: `hl[i]--`, transition GRN→YLW (hl<140) → RED (hl=0) → UP (rel=0)
6. `draw_mouse()` → INT 33h AX=3 → update right panel if X/Y/btns changed

## Build

```powershell
cd docs/tools/inputtest-better
$env:WATCOM='C:\Apps\OW'; $env:PATH='C:\Apps\OW\binnt;'+$env:PATH
pwsh -NoProfile ./build.ps1
```

Script: wcc (compile) → wlink (link) → copy `inputtest.com` from sibling dir → zip .dosz → copy to `E:\PC\DOSBoxPure\`.

Produces `dist/` with `hello.exe`, `inputtest.exe`, `inputtest.com`.

## Files

| File | Purpose |
|------|---------|
| `inputtest.c` | Main program (560 lines) |
| `hello.c` | Minimal Open Watcom graphics sanity test |
| `build.ps1` | One-command build + deploy |
| `dist/` | Build output (.exe, .com, .dosz) |
| `../inputtest/inputtest.asm` | Sibling ASM version (NASM, basic, no keyboard viz) |

## Constraints

- **C89 only** — Open Watcom C16 beta. No declarations after statements inside blocks.
- **Open Watcom 2.0beta1** — `graph.h`, `bios.h`, `i86.h`, `conio.h`. Linker warning `math87s.lib` missing is harmless.
- **VGA 640×480, 16 colors** — `_setvideomode(_VRES16COLOR)`. Text overlay via `_settextposition`/`_outtext`.
- **No VGA retrace sync** (`inp(0x3DA)`) — hangs DOSBox Pure (1000× slowdown).
- **DOSBox INT 16h AH=12h** puts extended flags in AH, NOT BH/BL. ASM sibling does `mov bx, ax` to swap.
- **No frame delay** — loop runs at emulator speed.

## Known Caveats

- F11/F12 intercepted by DOSBox Pure UI before reaching BIOS. May never show press.
- Modifiers only detectable via INT 16h AH=12h polling (no keystroke generated for modifier-only presses in most BIOSes).
- `_bios_keybrd(_KEYBRD_READY)` maps to INT 16h AH=01h — some BIOS/DOSBox implementations may not handle this correctly for extended keys.

## History

Built iteratively during dosbox-pure-unleashed-uwp development. Started from `hello.c` test, evolved into full keyboard visualization with mouse support. Replaced earlier `inputtest.asm` when richer visual feedback needed.
