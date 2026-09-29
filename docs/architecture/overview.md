# Architecture

`Live2D/` is the shared playback library. Its public C headers are in `Live2D/include/l2d/`; Host and ESP-IDF compile the same explicit source list in `Live2D/cmake/l2d_sources.cmake`. The runtime has no ESP-IDF, LVGL or Windows dependency. The trimmed internal `PX_*` code implements the legacy `.live` reader, Timeline VM, RT30 evaluator, hierarchy, deformation and software raster.

The immutable `l2d_model_t` owns textures, mesh topology, animation frames and baked RT30 samples. Each `l2d_instance_t` owns playback cursor, parameter state, mutable vertices and trig cache. Instances can share one model. Creation and loading allocate; steady update and render do not call the system allocator.

| Layer | Contents |
| --- | --- |
| `Live2D/src/` | Loader, Timeline and RT30 playback, hierarchy, transform, software raster, ROI, allocator |
| `Live2D/ports/host/` | Host allocation, clock and logging |
| `Live2D/ports/esp_idf/` | ESP-IDF allocation, clock and logging, reusable component |
| `examples/esp32p4/` | PPA FILL and SRM ROI, PSRAM/cache, SD, LCD, FreeRTOS, benchmark capture |
| `examples/esp32s3/` | Headless ESP-IDF integration; no P4 PPA dependency |
| `Live2D_Editor/` | Model authoring, Timeline editing, RT30 bake and export, Windows build |
| `PainterEngine/` | Original PC engine reference, used by the editor, outside embedded builds |

Timeline and RT30 are per-instance exclusive pose writers. `l2d_instance_play_animation()` starts an imported Timeline animation. `l2d_instance_enter_rt30()` switches to realtime evaluation. The mode query reports NEUTRAL, TIMELINE or RT30. `l2d_instance_reset_rt30()` resets RT30 parameters but does not switch modes.

The P4 example uses the public model and instance API. Its render path clears the 320×320 ARGB buffer, evaluates one pose and rasterizes, converts only the geometry ROI through PPA SRM, then submits the full RGB565 panel buffer. Clear, raster and submit remain full-frame. PPA and cache policy are outside the shared runtime.

The nearest renderer uses 16.16 UV stepping, integer alpha blend and per-instance trig cache. The fast-nearest kernel is `Live2D/src/raster/l2d_raster_nearest.c`. The generic span, including the HDR blend path, stays in the legacy framework file. Host and the ESP-IDF component both compile the runtime with `-O2 -fno-strict-aliasing -ffp-contract=off`. No per-pixel platform callback or C++ runtime is introduced into the core.

Current posture, the board baseline and deferred work are in [status](status.md).
