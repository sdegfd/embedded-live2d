# Handoff: embedded runtime refactor

Date: 2026-09-29. Branch: `refactor/embedded-runtime`. Parent: `adbc38aafe3cd5b92ee040dc446f17394e1bbda8`.

Runtime commit: `42b8aa5979d30a01d3f8e946e88388e74b4218b0`. Docs commit: `2555b0d`. Both are on `origin/refactor/embedded-runtime` at `github.com:sdegfd/live2d.git`. `main` was not updated.

## What the branch contains

One playback implementation, compiled from `cmake/l2d_sources.cmake`:

- Host static library `l2d_runtime` and test `l2d_test_runtime`.
- ESP-IDF component `live2d_engine` at `ports/esp_idf/components/live2d_engine`, used by the existing P4 project and by `examples/esp32s3`.
- PPA, the panel, SD, and tasks stay in `firmware/esp32p4/live2d_rt30_baseline`.
- The Windows editor still builds root `PainterEngine/` and was not edited.

Public headers under `include/l2d/` do not include ESP-IDF, LVGL, Windows, or PainterEngine. Numeric profile in this round: `L2D_NUMERIC_PROFILE_EMBEDDED_COMPAT_NEAREST`. Trig cache, RT30, and the nearest raster moved with that profile. Their comparison and pixel formulas were not replaced.

## Commands that were run

Host:

```bash
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime
./build/host/l2d_test_runtime
```

The test printed `L2D_RUNTIME_TESTS_OK` with the four-axis CRCs in `benchmark-regression.md`.

P4, from `firmware/esp32p4/live2d_rt30_baseline`, after `source /home/ubuntu/esp/esp-idf-v5.5.2/export.sh`:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash
python3 tools/capture_l2d_profile.py --port /dev/ttyUSB0 --timeout 180 \
  --out /tmp/l2d_p4_capture
```

`idf.py build` finished (`P4_EXIT:0`). The flash and capture finished (`FLASH_CAPTURE_EXIT:0`). The capture files were then copied to `doc/optimization/refactor_embedded_runtime_stage0/`. Boot log model SHA256 is `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`. Stage-0 MULTI_AXIS frame average 27335.74 us against the Phase 3B stage-1 average 27332.66 us. Deadline misses 0/50. Details are in `benchmark-regression.md`.

S3:

```bash
cd examples/esp32s3
idf.py -B build set-target esp32s3
idf.py -B build build
```

`S3_EXIT:0`. Status: build-verified, not hardware-verified.

Editor: `project/PainterEngine_LiveEditor/build_msvc.bat` was not run. No MSVC on this machine. The editor sources and root `PainterEngine/` have an empty diff.

## P4 status

The shared sources built into the timing firmware and ran on the connected board. The SD model is the five-axis file. Performance of the stage-0 scenes is inside the 3% band of the Phase 3B short test for raster, SRM, producer, and frame time.

The committed `sdkconfig` remains timing stage 0 with SRM ROI. A second, temporary correctness image (that flag on, then the file restored) produced 459 rows. They match `doc/optimization/phase31_correctness_roi/correctness.csv` and the full-frame Phase 3B CRCs on every compared column. Host four-axis CRC is re-signed against the pre-move oracle. The four-axis host STATIC value `6793bec7` and the device STATIC value `eae4bd6c` are both still true. The device row was reproduced.

Clear is still the full canvas. Raster is still full. Conversion is ROI SRM. Submit is the full RGB565 buffer. No partial FILL, dirty raster, extra framebuffer, or async PPA was added.

## S3 status

Shared core and software RGB565 are in the image. The example creates a 64 KiB instance, rejects a null model, and checks one red pixel. No panel, pins, or PSRAM capacity are specified. Do not describe this as on-device display or a frame rate.

## Limits that stay

- Load is not transactional. A failed reload drops the old model in that instance.
- Import can return before `PX_LiveFrameworkFree` if vector setup fails before the framework exists. Later failures still free.
- Embedded-nearest requires a tight BGRA stride.
- `L2D_PC_COMMIT` in the UART protocol is still the literal `78fc634`.
- Host STATIC CRC `6793bec7` and historical device STATIC `eae4bd6c` both stand. Math was not edited to force them together.
- `PX_LiveFramework.c` still contains editor export helpers. The P4 image does not link their string callees.
- PainterEngine sources in-tree have no license banner. None was added.
- `project/esp.live` is the user's five-axis file. It is not part of the commit.

## Next round, not this one

Software raster: fixed UV step, a proved safe span, incremental or fixed edges, alpha occupancy, partial dirty raster.

Runtime: frame reuse with an explicit visual-change rule, more numeric profiles, a smaller instance layout.

Platform: partial FILL, a smaller cache sync, an async pipeline with a written ownership rule, S3 on a real panel.

Tools: PSD helper, group and transform mapping, alpha-contour meshing that respects holes, pose-combination checks, workload reports. A convex Delaunay fill is not that mesher.

Also still open: the host/device STATIC CRC gap, and the partial vector-init leak on early import failure.
