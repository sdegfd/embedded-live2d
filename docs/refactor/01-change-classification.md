# Change classification

Baseline commit: `adbc38aafe3cd5b92ee040dc446f17394e1bbda8`.
Branch: `refactor/embedded-runtime`.
The worktree file `project/esp.live` was already dirty before this refactor and is not part of it.

Status words: kept means the behavior is in the shared playback path; moved means the code changed directory and still runs; platform means it stays out of `src/`.

## A. Shared runtime

| Item | Origin | State | New home | Evidence |
| --- | --- | --- | --- | --- |
| RT30 continuous parameters, neighbor interpolation, sparse bindings | Phase 0–2, `PX_LiveRealtime.c` | kept | `src/internal/pe/kernel/PX_LiveRealtime.c` | Host loads 4 axes from the git blob and 5 from the local file. |
| Incremental contribution update and full fallback | same | kept | same file | No math edit this round. Four-axis CRC matches the pre-move host oracle. |
| Discrete texture choice and hysteresis | same | kept | `PX_LiveFramework.c` | Same CRC lock. |
| Hierarchy and local visual transform | Phase 2.5 | kept | `PX_LiveFramework.c` | `GetLayerVisualTransform` calls pass the `pLive` parameter. |
| Skip unused elastic prepare in REALTIME30 | Phase 0–2 | kept | framework update path | Untouched control flow. |
| Two rotation trig caches | `3b12888` | kept | per-framework `trigCacheHit` / `trigCacheMiss` | Host STATIC reports hit 29, miss 22. A second instance's neck pose does not clear the first instance's counters. |
| Reset / reload drops the cache | Phase 2.5 | kept | framework reset and import `memset` | Counters live on the object that import clears. |
| Model lifetime | existing load/free | kept, documented | `l2d_instance_load_memory` | Failed reload leaves that instance unloaded. A second instance stays loaded in the host test. |

## B. Software renderer

| Item | State | New home | Evidence |
| --- | --- | --- | --- |
| Embedded fast-nearest shader, affine UV, 16.16 step, alpha 0/255/partial | kept | `src/engine/live2d_engine.c` | Shader body not rewritten. Host CRC locked. |
| Vertex to surface transform and submitted-triangle AABB | kept | engine plus `l2d_geometry_bounds_t` | Host ROI unit check. P4 still reads bounds when `L2D_CFG_SRM_ROI` is 1. |
| Generic clip and BGRA to RGB565 | kept | `include/l2d/l2d_image.h` | Host checks alpha ignore, padded stride, and a short buffer. S3 smoke checks opaque red `0xF800`. |

The PC editor bilinear path is still in root `PainterEngine/`. It is not this nearest profile.

## C. ROI policy versus P4 execution

| Item | Home |
| --- | --- |
| Previous/current geometry union, full-frame conditions, per-target history | `src/pipeline/l2d_roi.c` |
| Mapping a rect to PPA SRM registers, FILL, cache sync | `firmware/esp32p4/live2d_rt30_baseline/components/live2d_renderer/live2d_renderer.c` |

The P4 renderer still records its own history fields before SRM returns. That matches the firmware this refactor started from. The portable API commits only when the caller calls `l2d_output_commit`.

## D. Platform

Left in the IDF project or the ESP port: `ppa_register_client`, FILL, SRM, `esp_cache_msync`, `heap_caps_*`, `esp_timer_get_time`, `ESP_LOG*`, FreeRTOS tasks and core affinity, `esp_lcd`, the panel and touch on the current board, SDMMC, and the LVGL objects in the board package. `src/` does not include those headers.

## E. Authoring

GUI, PNG import, fonts, file dialogs, Delaunay editing, bake, export, undo, and guides stay in `project/PainterEngine_LiveEditor` and root `PainterEngine/`. Those trees have no diff in this refactor. This Linux machine has no MSVC build of the editor.

`PX_Delaunay.c` and `PX_Memory.c` moved with the firmware snapshot and are not in `L2D_RUNTIME_SOURCES`. The triangle struct header is still included. The editor continues to compile the full root `PainterEngine` via `build_msvc.bat`.

## F. Diagnostic or reverted

| Item | State |
| --- | --- |
| Per-pixel workload counters, fine/visual timers, four-way CPU/PPA benchmark | Still behind `L2D_CFG_PROFILE_*`. Default P4 `sdkconfig` leaves fine, visual, detail, and ROI diag off. |
| Reverted child-stretch precompute | Not restored. |
| Numeric approximations, LUT trig, fixed edge walker, safe span, alpha occupancy, frame reuse, dirty raster, partial FILL, async PPA | Not implemented. |

`L2D_PC_COMMIT` in the serial protocol remains the literal `78fc634`. That field is the historical editor snapshot the old parser expects. It is not the identity of this runtime. The capture also prints `esp_commit` from `git rev-parse HEAD` at CMake time, which was `adbc38aa` while the tree was dirty. The source set that was actually compiled is hashed in `docs/refactor/benchmark-regression.md`.
