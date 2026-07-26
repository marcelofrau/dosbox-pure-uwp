# Code Reference: Key Files and Line Numbers

This document catalogs every relevant code location discussed in the investigation. All paths relative to `F:\workspace\vs2022\dosbox-pure-unleashed-uwp\`.

## UWP Project Files

### `dosbox-uwp/dosbox_uwpMain.cpp`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 73 | `m_retroScreen` | RetroScreenRenderer instance (D2D, legacy) |
| 334 | (comment) | "DoPacingSleep removed" — note that DoPacingSleep was removed previously? |
| 335-398 | `DoPacingSleep()` | QPC Sleep pacing function. Uses `WaitForSingleObject(hTimer, ms)` with HIGH_RESOLUTION timer |
| 456-472 | FPS tracking | Rolling 60-sample average FPS from QPC |
| 478 | `m_timer.Tick(lambda)` | StepTimer entry point |
| 485-486 | `PollEvents()`, `_t0` | Input poll, start of timing measurement |
| 720-737 | Audio time accumulator | `m_audioTimeAccumulator += dt * targetFps` — wall-clock to frame-count conversion |
| 739-751 | `maxRetroRuns` | Audio queue feedback: qf>40→1, qf>20→2, qf>10→3, else→5 |
| 753-782 | Inner retro_run loop | `while (accumulator >= 1 && maxRetroRuns > 0)` guard with 25ms tick budget |
| 770 | `m_retroCore->RunFrame()` | The actual emulation call |
| 773-774 | SLOW FRAME detection | `rfMs > 20.0` → `spdlog::warn("[dosbox-uwp] SLOW FRAME ...")` |
| 827-830 | Auto-stop XAudio2 | Stop voice when retro_runs == 0 and queue empty |
| 867-876 | DIAG log | Every 3000 ticks: fps, frame_ms, runs, accumulator, audio_q, cpu, mem, cycles, video res |
| 878-908 | Drop detection | 5+ consecutive windows below 80% target FPS |
| 956-968 | Skip detection | Single tick >20ms with retro_runs > 0 |

### `dosbox-uwp/Content/XAudio2Output.h`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 12 | `s_queuedFrames` | Atomic counter: total frames queued in XAudio2 (sum of all submitted buffers) |
| ~15 | `s_totalConsumed` | Atomic counter: total frames consumed by XAudio2 |
| ~18 | `TARGET_QUEUE` | 2736 frames (~57ms at 48kHz) before voice starts |
| ~20 | `MAX_QUEUE` | 16000 frames (~333ms) cap that triggers flush |
| ~30 | `MAX_FRAME_SIZE` | 8192 frames per slot (~170ms) |
| ~35 | `POOL_SIZE` | 32 slots |
| ~40 | `BufferSlot` | Struct: inUse, xa2buf, data[], frames, flushGen |
| ~55 | `GetQueuedFrames()` | Returns `(uint32_t)s_queuedFrames` |
| ~60 | `GetSamplesPlayed()` | Returns `s_totalConsumed` (via GetState) |
| ~65 | `OnBufferEnd()` | Releases slot, tracks consumption |
| ~70 | `OnEngineError()` | Voice loss callback |

### `dosbox-uwp/Content/XAudio2Output.cpp`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 12 | `s_queuedFrames` definition | `volatile long s_queuedFrames = 0` |
| 84-98 | `OnBufferEnd` | Decrements s_queuedFrames, sets drain event, releases slot |
| 100-110 | `OnEngineError` | Voice loss: Stop, reset state |
| 115-135 | Constructor | Create source voice, set callbacks |
| 237-383 | `Submit()` | Lock-free CAS claim slot, memcpy, SubmitSourceBuffer, auto-start, FLUSH CAP |
| 267-274 | Slot claim | `InterlockedCompareExchange` loop |
| 291 | memcpy | Copy audio into slot |
| 298 | SubmitSourceBuffer | Non-blocking XAudio2 call |
| 299 | `InterlockedExchangeAdd(&s_queuedFrames, frames)` | Queue accounting |
| 302-309 | Auto-start | `if (qf >= TARGET_QUEUE && !s_started) Start(0)` |
| 312-321 | **FLUSH CAP** | `if (qf > MAX_QUEUE) Stop() + FlushSourceBuffers() + resubmit + Start()` |
| 390-420 | Cleanup | Destroy voice |

### `dosbox-uwp/Content/RetroCore.cpp`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 170-202 | `RunFrame()` | Calls `retro_run()`, logs timing every 600 frames |
| 398-401 | `SET_HW_RENDER` rejection | Returns 0 — forces SW render path |
| 541-577 | `retro_video()` | Stores frame data/pitch (rejects pitch==0 for HW_FRAME_BUFFER_VALID) |
| 584-591 | `retro_audio()` | Calls `s_audioOutput->Submit(data, frames)` — non-blocking |
| 595-597 | `retro_input_poll` | Currently empty — input polled separately |
| 600+ | retro_env callbacks | Various environment callbacks |

### `dosbox-uwp/Content/RetroD3D11Renderer.cpp`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 120-131 | Staging texture | Pre-allocated 1024x768 D3D11_USAGE_STAGING |
| 155-266 | `UpdateVideoFrame()` | Map→memcpy row-by-row→Unmap→CopySubresourceRegion |
| 268-291 | `Render()` | Set pipeline state, DrawIndexed(6) |

---

## RetroArch Reference Files

All paths relative to `F:\workspace\vs2022\RetroArch\`.

### `runloop.c`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 7495 | `runloop_iterate()` | Main loop entry point |
| 7547-7572 | Frame time callback | `runloop_st->frame_time.callback(delta)` before core_run |
| 7577-7612 | Audio buffer status callback | Tells core buffer occupancy % |
| 7682-7688 | **Critical comment** | Explains why QPC sleep is NOT the pacer — audio backpressure is |
| 7760 | `core_run()` | Calls `retro_run()` |
| 7837-7879 | Frame limit sleep | QPC sleep — only active for fast-forward/VRR |
| 2401-2414 | `SET_FRAME_TIME_CALLBACK` | Stores callback from core |
| 4579 | `frame_limit_minimum_time` | Computed as `1000000 / (fps * fastforward_ratio)` |

### `audio/drivers/xaudio.c`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 57-58 | `MAX_BUFFERS = 16` | Ring buffer size |
| 137-141 | `OnBufferEnd` | `InterlockedDecrement(&buffers); SetEvent(hEvent);` |
| 503-568 | `xa_write()` | Blocking write: `while (buffers == MAX_BUFFERS - 1) WaitForSingleObject(hEvent, XAUDIO_TIMEOUT)` |
| ~570 | `xa_write_avail()` | Returns available buffer space |
| ~590 | `xa_start()` | Create source voice, event |
| ~610 | `xa_stop()` | Destroy voice, close event |
| ~620 | `xa_free()` | Cleanup |

### `audio_driver.c`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 89-92 | `AUDIO_CHUNK_SIZE_BLOCKING = 512` | Samples per blocking write chunk |
| 548-575 | `audio_driver_control()` | DRC: samples buffer occupancy, adjusts resampler ratio ±rate_control_delta |
| 1190-1191 | `audio->write()` | Calls driver's write (xa_write for XAudio2) |
| 1542 | `audio_driver_sample_batch()` | Called by core's audio callback, flushes to driver |

### `gfx/drivers/d3d11.c`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 4137-4236 | `d3d11_gfx_frame()` | Handles both SW and HW frame paths |
| 4140-4160 | HW path | `PSGetShaderResources` → GetResource → CopySubresourceRegion (GPU→GPU) |
| 4216-4232 | SW path | `d3d11_update_texture` → Map/Unmap staging → CopySubresourceRegion |
| 4917-4926 | Present | `DXGIPresent(swapChain, swap_interval, flags)` — vsync optional |
| 5621-5624 | Thread safety | Comment: ID3D11DeviceContext is NOT thread-safe |

---

## ZillaLib Reference (for comparison)

File: `F:\workspace\vs2022\dosbox-pure-unleashed\main.cpp`

| Lines | Symbol | Purpose |
|-------|--------|---------|
| 954-963 | `retro_hw_get_current_framebuffer()` | Returns GL FBO handle |
| 1094-1120 | HW render negotiation | GET/SET_HW_RENDER — accepts OpenGL |
| 1162-1172 | `retro_video_refresh_cb` | `data == RETRO_HW_FRAME_BUFFER_VALID` — zero-copy HW path |
| 1174-1181 | `DBPS_SubmitOSDFrame()` | OSD pixel upload (GL texture) |
| 1825-1834 | `retro_run()` | Called inside draw loop |
| 1886-1898 | FBO→screen blit | `ZL_Surface::DrawBox()` textured quad |
| 2022 | `ZL_Application(70)` | 70fps target (ZillaLib manages timing) |
| 2146-2150 | `AfterFrame()` → `OnDraw()` | Entry point (ZillaLib's own RunLoop) |
