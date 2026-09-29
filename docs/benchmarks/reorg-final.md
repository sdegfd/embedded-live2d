# Embedded Runtime reorganization validation

## Scope and identity

Branch `refactor/embedded-runtime`, source commit `43e9d01ec87456190c0191826fb192f4ab41cb0c` for both device captures. ESP-IDF v5.5.2. The sole formal model is `models/esp.live` on Host and `/sdcard/esp.live` on P4: SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`, 800796 bytes, five RT30 axes. The capture tool checked the model identity before accepting the device output.

The P4 correctness suite's `RELOAD_STATIC` and `RELOAD_POSE` steps temporarily load the **same** `/sdcard/esp.live` a second time while the previous model and instance still exist. This tests transactional reload; it does not introduce a second formal model. The previous 16 MiB model pool could not coexist with a second 16 MiB pool in 32 MiB PSRAM after display allocations. Its two reload rows failed in the exploratory capture at `reorg-p4-correctness-before-pool-fix/`. The public model loader now reserves `2 * wire_bytes + 1 MiB` (2650168 bytes here), with an overflow guard. The formal model actually uses 837184 bytes of that arena. Host exercises two simultaneous loads and the P4 reload now completes.

## Verification

| Target | Result |
| --- | --- |
| Host CMake/CTest | `l2d_runtime` passed; five-axis model, Timeline visible frame and Timeline to RT30 switch, loader, renderer, multi-instance sharing, reload CRC. |
| Host ASan + UBSan | `L2D_RUNTIME_TESTS_OK`, leak detection enabled. |
| PC RT30 editor-side test | Linux build against retained PainterEngine: all 2305 checks passed. Windows MSVC GUI was not built here. |
| ESP32-S3 | ESP-IDF v5.5.2 build passed; `l2d_s3_smoke.bin` 219696 bytes; no S3 hardware run. |
| ESP32-P4 | Timing and correctness images built and flashed to `/dev/ttyUSB0`; model SHA matched. |
| P4 correctness | 459/459 ordered ARGB8888 and RGB565 CRC rows match `docs/optimization/phase31_correctness_roi/correctness.csv`; every render and submit succeeded, including both reload rows. Raw data: `reorg-p4-correctness/`. |
| P4 timing | 150 measured frames across STATIC, EYE_L_SWEEP and MULTI_AXIS; zero deadline misses, no panic or Backtrace. Raw data: `reorg-p4-timing/`. |

P4 correctness used `CONFIG_L2D_PROFILE_CORRECTNESS=y`. After capture, `sdkconfig` was restored byte for byte to timing mode. P4 timing used profile stage 0, `short-v1` (5 warmup, 50 measured frames per scenario), scale 1.0, 360 MHz CPU, 200 MHz PSRAM, 320×320 ARGB intermediate, fast-nearest raster, PPA FILL clear, PPA SRM ROI conversion and synchronous RGB565 panel submission. These conditions match the [pre-reorganization refactor baseline](refactor-regression.md). Comparison is between short captures, not a long-run or interleaved experiment.

## P4 timing against the pre-reorganization baseline

| Scenario / metric | Previous µs | Final µs | Change |
| --- | ---: | ---: | ---: |
| STATIC frame average | 25363.20 | 25826.04 | +1.82% |
| EYE_L_SWEEP frame average | 26951.38 | 27319.54 | +1.37% |
| MULTI_AXIS frame average | 27335.74 | 27896.66 | +2.05% |
| MULTI_AXIS frame p95 | 27893 | 28627 | +2.63% |
| MULTI_AXIS raster average | 18830.36 | 19237.90 | +2.16% |
| MULTI_AXIS PPA SRM average | 4360.56 | 4397.54 | +0.85% |
| MULTI_AXIS producer average | 27096.92 | 27627.90 | +1.96% |

No deadline misses occurred in either capture. The average frame and raster increases remain below the existing 3% investigation line; they are still a measured regression, so retain these CSVs for the next optimization pass. Host benchmark timings are separate machine measurements and cannot be compared numerically with P4.

## Code boundary and remaining work

The Embedded Runtime compiles 10 retained `PX_*.c` files under `Live2D/src/internal/pe/`, for memory pool, vector, surface/texture, device format, LiveFramework playback and RT30. It does not compile the full `PainterEngine/` tree. Editor authoring, Delaunay generation, UI, audio, network and scripts are outside this embedded source list. Timeline VM and RT30 both remain in playback. The active Windows editor is in `Live2D_Editor/` and still builds against the preserved `PainterEngine/` reference tree.

Remaining validation before a public release: test the Windows editor build/GUI on MSVC, run S3 on a board, perform a P4 long soak, evaluate the arena sizing heuristic with other legitimate models, and resolve redistribution terms for the retained PainterEngine sources. These are not covered by the short test above.
