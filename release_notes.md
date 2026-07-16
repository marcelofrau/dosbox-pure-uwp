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
