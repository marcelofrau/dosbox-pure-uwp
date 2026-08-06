## 🎮 DOSBox Pure UWP 1.0.0.213 — First 1.0.0 Release

First release under the **1.0.0** line and the biggest performance + stability jump since 0.9.5.135. Emulation now runs on its own thread, audio latency dropped to 48ms, the JIT cache is faster on UWP, and Xbox behaves better (screen stays on, clean suspend/resume).

---

### ⚡ Smoother, Faster, More Stable

- **Emulation runs on its own thread** — the UI never blocks on game work. Frames reach the screen as fast as the game produces them, fixing the "background moves then returns" glitch
- **Lower audio latency** — ring cut to **48ms** (was a much larger buffer) for a snappier feel, still rock-solid on Xbox
- **Faster JIT recompiler** — the dynamic recompiler now uses a dual-view memory cache (RW to write, RX to run), replacing the old page-flipping scheme. No more security-flip stalls mid-game
- **New default: VSync On** — smoother, tear-free output out of the box (was Off)

### 🖥️ In-Game Menu (PUREMENU) — now pixel-perfect

- The in-game settings menu renders at its native **640x480** resolution and scales cleanly to any output — no more mis-layout or blurry text at odd resolutions
- Menu is crisp and centered, and the mouse cursor tracks correctly at any resolution

### 🎮 Xbox Experience

- **Screen stays awake while playing** — the Xbox dim/idle timer no longer kicks in mid-game
- **Clean suspend/resume** — emulation pauses instantly when you switch away and resumes on return, without stutter or crashes
- **Faster remote debugging** — Visual Studio is pre-configured to deploy straight to your Xbox (Remote Debugger)
- Debug builds include extended input logging (`INPUT_DEBUG_ENABLED`) to help troubleshoot controller issues

### 🎛️ Settings & Persistence

- All menu options persist to **`dosbox-pure-settings.json`** (Xbox: `E:\dosbox\`, Windows: `%TEMP%\dosbox-pure\`)
- Per-game **FRONTEND.DBP** overrides can apply (partial: flat `{"key":"value"}` objects)
- **Debug Overlay is Off by default** — only shows what you ask for

### 🔧 Under the Hood (technical)

- **Threaded emulation**: emulation thread owns `retro_run` / `retro_load_game` / `retro_deinit`, paced by the blocking audio `Write()`. UI thread only reads the newest frame from a **3-slot frame ring** (per-slot monotonic sequence counters → always presents the newest frame) and presents it
- **Audio-master pacing** replaces the QPC `PaceFrame()` timer + multi-run model. The ring is a pure follower (4 × 12ms = 48ms sub-buffers, `MAX_BUFFERS=4`, `SUBBUF_FRAMES=576`); blocking `Write()` is a safety valve ≤256ms; `OnBufferEnd` = dec + event
- **Dynarec**: `dyn_cache.h` maps a single `PAGE_EXECUTE_READWRITE` file-mapping section with two views — `FILE_MAP_ALL_ACCESS` (write) + `FILE_MAP_EXECUTE|FILE_MAP_READ` (run). AppContainer refuses one WRITE+EXECUTE view (err=87); two views of the same section work. Removes `cache_make_writable/executable` and all W^X flips
- **OSD**: per-file `DBP_STANDALONE` (core TU only, vcxproj override) keeps libretro audio/OSD/SW-render alive; `DBP_RenderOSD` composites PUREMENU at fixed 640x480 onto the game framebuffer
- **Suspend/resume**: `DisplayRequest` keeps the screen awake; `OnSuspending` pauses emulation immediately, `OnResuming` resumes, `OnVisibilityChanged` joins the emulation thread so no retro callbacks fire after the CoreWindow is gone

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
