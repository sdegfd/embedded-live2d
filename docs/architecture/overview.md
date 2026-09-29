# Live2D Embedded Runtime

ESP32-P4 is the first board that runs the full display path. ESP32-S3 is the second target and has a build entry. The runtime itself is the shared tree below. Another MCU should add a port, not a second copy of PainterEngine.

## Boundaries

| Area | Owns | Does not own |
| --- | --- | --- |
| M, tools | Windows editor, mesh editing, bake, export, PNG import. Still built from root `PainterEngine/` and `project/PainterEngine_LiveEditor/`. | MCU playback. |
| R, runtime | `.live` load, RT30 pose, hierarchy, trig cache, embedded-nearest raster, geometry bounds, software RGB565 fallback. Sources under `src/` and `include/l2d/`. | PPA, FreeRTOS, the panel, the editor GUI. |
| P, platform | Allocation placement, time, log, PPA FILL/SRM, cache visibility, display submit. | Model evaluation and triangle raster. |

## Where the one runtime lives

Production playback sources are the explicit list in `cmake/l2d_sources.cmake`. Host, the P4 component `ports/esp_idf/components/live2d_engine`, and the S3 example all compile that list plus one port file:

- Host: `ports/host/l2d_port_host.c`
- ESP-IDF: `ports/esp_idf/common/l2d_port_esp.c`

Nothing copies those `.c` files into an example. The old firmware directory `components/live2d_engine/painterengine` was moved into `src/internal/pe`.

Root `PainterEngine/` stays for the editor. It is not linked into the P4 or S3 firmware. `PainterEngine_LiveEditor_source/` and `live2d_pc_preview/` are historical trees and are not on this runtime's build.

## Data flow on P4

The P4 frame order is unchanged:

1. Full-canvas clear (PPA FILL, software memset if FILL is unavailable).
2. One `live2d_engine_render`: pose update, then embedded-nearest raster into a tight BGRA buffer.
3. Geometry bounds become a conversion rectangle. Padding is `floor(min) - 2` and exclusive `ceil(max) + 3`.
4. PPA SRM converts that rectangle from BGRA to RGB565. A failed SRM falls back to a full-frame software convert.
5. The panel submit is still the full RGB565 buffer.

Clear, raster, and panel submit are full-frame. Only the color conversion uses the ROI. That is not dirty rasterization.

Public `l2d_instance_update` and `l2d_instance_render_current` split those steps for host tests. The P4 example still calls `live2d_engine_render`, which does update and draw together, once per frame.

## Module sketch

```text
include/l2d/          public C ABI (no ESP, LVGL, Windows, PainterEngine)
src/api/              l2d_instance over the engine
src/engine/           load, RT30, raster entry
src/pipeline/         conversion ROI history
src/internal/pe/      trimmed PainterEngine playback sources
ports/host            posix allocator, clock, stderr log
ports/esp_idf/common  heap_caps, esp_timer, ESP_LOG
firmware/.../live2d_renderer   P4 PPA and display
examples/esp32s3      headless smoke, software RGB565 check
project/PainterEngine_LiveEditor   Windows editor, separate tree
```

## Numeric profile

The implemented profile id is `L2D_NUMERIC_PROFILE_EMBEDDED_COMPAT_NEAREST` (1). Geometry is float. The original `PX_sin_angle` / `PX_cos_angle` / `PX_sind` path remains, with the Phase 2.5 per-instance angle cache. Cache comparison is floating `==`. This profile is not a claim that every trig helper is pure float32.

## What a later port must supply

A port implements `l2d_port_alloc`, `l2d_port_free`, `l2d_port_log`, and `l2d_pe_time_us`. Display, cache maintenance, and hardware color conversion stay outside the core. Software RGB565 is `l2d_convert_bgra_to_rgb565` in the public header.
