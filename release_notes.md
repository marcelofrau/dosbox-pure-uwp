## 🎮 DOSBox Pure Unleashed 1.0.0.213 — First Major Release

Full frontend overhaul since 0.9.5.135: fullscreen settings menu, in-game mouse emulation, 640x480 PUREMENU OSD, low-latency audio ring, and version auto-incrementing.

---

### 🖥️ Frontend Menu (fullscreen)

- **New fullscreen DOS-style menu** (D2D overlay, gamepad + mouse navigable) replacing the old in-app shell
- **8 pages**: Main (Load File / Settings / History / About / Exit), General, Input, Performance, Video, System, Audio, State
- **General**: VSync On/Off, Frame Limiter Off/60Hz/70Hz, Scaler Nearest/Bilinear, Startup Folder picker, Force Output FPS (10-360), Save States Support (on/rewind/disabled), Start Menu behavior, Debug Overlay On/Off, Reload Settings, Restart App
- **Performance**: Emulated Performance (AUTO/MAX/8086→Athlon 1.2GHz), Maximum Emulated Performance (Unlimited/315cps→1Mcps), Performance Scale 20-200%, Limit CPU Usage 50-100%, Performance Statistics (none/simple/detailed)
- **Video**: Emulated Graphics Chip (SVGA/VGA/EGA/CGA/Tandy/Hercules/PCjr), CGA Mode, Hercules Color, SVGA Mode + Memory, 3dfx Voodoo (8MB/12MB/4MB/off), Aspect Ratio Correction, Overscan Border
- **Input**: L3 Button to Show Menu (OSK), Mouse Input Mode (auto/virtual/direct/touchpad/off), Mouse Wheel key binding, Mouse Sensitivity (global + horizontal), Auto Game Pad Mappings, Keyboard Layout (26 layouts), Joystick Analog Deadzone, Action Wheel Inputs
- **Audio**: SoundBlaster Type/Settings/Adlib, MIDI Output, Gravis Ultrasound, Tandy Sound, Swap Stereo, per-channel volumes (SB/MIDI/Adlib/Speaker/CD-ROM/Other)
- **State**: save/load slot shell (menu present, wired later)

### 🎮 Gamepad Input

- **R3** — toggle PUREMENU (in-game settings) — no more Start/Back guessing
- **LB+RB+Select** (held together) — toggle **gamepad mouse mode** (default OFF): left stick → relative mouse in-game, A→Enter, B→Escape for PUREMENU
- **L-analog always drives the menu/OSD cursor** — FrontendMenu hover + PUREMENU pointer tracking, independent of mouse mode
- **R3/L3** — PUREMENU / on-screen keyboard access via core options

### ⌨️ Hotkeys

- **F10** — visual-only on the BIOS/start screen (hint "F10 = Menu" shown); toggles PUREMENU in-game
- **F12** — swallowed (reserved)
- **Ctrl+L** — open the in-app file browser at any time
- **Alt** — forwarded via accelerator key handling (menu bar / in-game Alt)

### 🖼️ PUREMENU OSD — 640x480 render path

- PUREMENU now renders on the game framebuffer: `DBP_STANDALONE` is defined **per-file for the core TU only** (vcxproj override), and the patched core composites the OSD via `DBP_ScaleNearest` + `DBP_RenderOSD` at a fixed **640x480** design buffer (`dbp_osdbuf`), scaled to the output
- Fixes menu mis-layout at arbitrary resolutions — OSD is always crisp and centered, mouse cursor tracks correctly

### 🔊 Audio

- **Ring reduced to 4 × 12ms = 48ms** (down from the mistaken 1.024s 16×64ms ring) — perceived latency ~40ms, matching RetroArch xaudio.c driver model
- QPC `PaceFrame()` remains the frame clock (GET_THROTTLE_STATE-consistent rate); ring is a pure follower absorbing drift
- 48ms is the Xbox stability floor: CPU-bound games (Screamer ~15.9ms/frame vs 16.7ms slot) tolerate occasional 2-frame spikes without underrun
- Sample rate hardcoded `48000` in the core under `DBP_STANDALONE` (matches the XAudio2 device; the option value only exists in the non-standalone option enum)

### ⚙️ Settings & Persistence

- All menu options persist to **`dosbox-pure-settings.json`** (Xbox: `E:\dosbox\`, Windows: `%TEMP%\dosbox-pure\`), JSON with `//` and `/* */` comment support (nlohmann/json)
- **`DBPS_ApplyConfigOverrides` now implemented** — minimal JSON parser in `dosbox_pure_sta.cpp` so per-game **FRONTEND.DBP** overrides can apply (partial: `{"key":"value"}` flat objects)

### 🔧 Versioning & CI

- **Auto-increment version system**: `version.props` (major.minor.patch) + `build_counter.txt` + PreBuildEvent script — each build bumps the revision and rewrites `Package.appxmanifest` + `version.txt`
- CI creates a self-signed cert, signs the MSIX (fixes 0x800B0100 install error), and publishes hand-crafted release notes
- First release under the **1.0.0** line: `1.0.0.213`

---

## 🎮 DOSBox Pure UWP 0.9.5.135

Major audio + pacing overhaul since v0.8.2.0. Multi-run model restored, auto-cycle clamping, frame limiter, and full XAudio2 pipeline rewrite.

---

### 🔊 Audio Pipeline

- **XAudio2 48kHz restored**: full rewrite replacing SDL audio — 32-slot buffer pool with zero heap allocation per submit
- **OnBufferEnd callback**: lock-free CAS slot claim, no more race conditions on Flush
- **Queue-depth feedback**: main loop scales retro_run count based on audio queue fill level (qf>40→1 run, >20→2, >10→3, else 5)
- **UNDERRUN fix**: core disk I/O (20-100ms) no longer drains audio queue — buffer pool absorbs spikes
- **Sample rate sync**: DBP_STANDALONE no longer hardcodes "44100" — uses option value matching XAudio2 48kHz

### ⚡ Multi-Run Loop

- **Reverted to multi-run**: up to 5 retro_run calls per frame tick, clamped at 2.0× target fps (140)
- **CyclesMax ±30% clamp**: St.LastCycleMax tracks previous value, new clamped to [old×0.7, old×1.3] — prevents wild oscillation
- **DBPS_GetCyclesMax()**: new extern + RetroCore::GetCyclesMax() wrapper for diagnostics

### 🎯 Frame Limiter

- **New setting**: Off / 60Hz / 70Hz in General menu
- **Software pacing**: App::Run sleeps + spins after Present(0,0) when limiter enabled
- **VSync default changed**: now "Off" by default (180Hz display doesn't divide evenly into 70fps)

### 🖼️ UI Improvements

- **Toast redesign**: yellow text (#FFD900) on black semi-transparent background (85% alpha)
- **Toast moved**: now renders after overlay + confirm (no more occlusion)
- **Double-enter fix**: OnBack callback calls Close() on first line
- **Overlay hit-test**: offset corrected for accurate pointer interaction
- **Wheel cooldown**: prevents rapid scroll spam
- **Marquee reset**: scroll animation resets on item change

### 📊 Diagnostics

- **Config dump**: end of check_variables() logs machine, memory_size, cpu_core, cpu_type, cycles, cycles_max, sblaster_conf, aspect_correction, savestate
- **DIAG overlay**: D2D display shows CyclesMax + audio queue fraction
- **spdlog integration**: all diagnostics use structured logging

### 🔧 CI/CD & Scripts

- **MSIX signing**: CI now creates self-signed certificate and signs packages (fixes 0x800B0100 install error)
- **Version detection**: all PowerShell scripts read version.txt AFTER build (not before)
- **MSIX glob fallback**: handles version mismatch between version.txt and built MSIX
- **Release notes**: hand-crafted with categories and emojis
- **Release workflow**: PreBuildEvent handles version bump — no redundant patching

---

### 📦 Installation

1. Download `dosbox-uwp_0.9.5.135_x64.zip`
2. Extract to a folder on your Xbox/PC
3. Run `Install.ps1` (installs cert + dependencies + app)
4. Launch from Start Menu or Xbox Dev Mode

> **Note:** Side-loading requires Developer Mode enabled in Windows Settings.

---

### 🔗 Links

- [Source Code](https://github.com/marcelofrau/dosbox-pure-uwp/tree/main)
- [DOSBox Pure Core](https://github.com/schellingb/dosbox-pure)
- [Libretro](https://www.libretro.com/)

---

**Full Changelog**: https://github.com/marcelofrau/dosbox-pure-uwp/compare/v0.8.2.0...v0.9.5.135
