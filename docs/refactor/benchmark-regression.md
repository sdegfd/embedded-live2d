# Benchmark regression

Short preset, unchanged: warmup 5, measure 50, rounds 1, scale 1.0, deadline 33333 us. Scenario code is `bench/scenarios/l2d_preset.h` (`short-v1`). The P4 stage-0 suite runs STATIC, EYE_L_SWEEP, and MULTI_AXIS. It does not run the stage-1 NECK and FACE rows. Missing axes are skipped by the existing handle check. This model has mouth, so MULTI_AXIS includes it.

## Identity of the timing image

| Field | Value |
| --- | --- |
| git HEAD at CMake | `adbc38aafe3cd5b92ee040dc446f17394e1bbda8` |
| worktree | dirty. `project/esp.live` plus this uncommitted refactor. The serial field `esp_commit` does not say dirty. IDF printed app version `adbc38a-dirty` on the later reconfigure. |
| `live2d_commit` serial field | `78fc634`, the historical editor snapshot, not this runtime |
| source tree SHA256 | `1209c2870da0a521ba5185ebcd0fe281f43c920d1d72462e6f68a6876cef06dc` over the runtime, port, and `live2d_renderer.c` bytes listed below |
| IDF | v5.5.2 |
| device compiler | riscv32-esp-elf-gcc 14.2.0 (`esp-14.2.0_20251107`) |
| host compiler | gcc 13.3.0, CMake 3.28.3 |
| sdkconfig | TIMING=y, STAGE=0, SRM_ROI=y, correctness/fine/visual/detail off |
| numeric profile | EMBEDDED_COMPAT_NEAREST |
| raster | embedded fast-nearest |
| clear / convert | PPA FILL / PPA SRM |
| model | `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`, 800796 bytes, 320×320, 11 layers, 227 vertices, 257 triangles, 12 textures, 5 axes |
| canvas | 320×320 BGRA intermediate, RGB565 panel, tight stride |
| CPU / PSRAM | 360 MHz, PSRAM 200 MHz, render task core 0 priority 6 |
| display | real panel path, synchronous submit |
| capture | `/dev/ttyUSB0`, 150 measured frames, `L2D_DONE` |

Raw rows: `doc/optimization/refactor_embedded_runtime_stage0/`.

The source digest covers, in path order: `src/engine/live2d_engine.c`, `live2d_engine.h`, `src/api/l2d_api.c`, `src/pipeline/l2d_roi.c`, `src/internal/l2d_config.h`, `src/internal/l2d_pe_port.h`, the compiled `PX_*.c` plus their playback headers, both port `.c` files, `cmake/l2d_sources.cmake`, and `live2d_renderer.c`.

## Historical host note, four-axis, not acceptance

The table below is an old measurement of git blob `4174304b…`. It is not the current host correctness, benchmark, CRC, memory, ROI, or consistency gate. The only formal model is the five-axis `project/esp.live`. See `MODEL_BASELINE.md`.

Model `786b18e3…`, 798660 bytes, extracted from git blob `4174304b…`. Command:

```bash
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime
./build/host/l2d_test_runtime
```

Result: `L2D_RUNTIME_TESTS_OK`.

| Pose | ARGB CRC | hit | miss |
| --- | --- | --- | --- |
| STATIC | 6793bec7 | 29 | 22 |
| ALL_14_5 | d522cbf1 | 29 | 22 |
| ALL_29 | 1033faa1 | 29 | 22 |
| NECK_15 | 6793bec7 | 29 | 22 |

ALL_14_5 and ALL_29 match the historical device rows for the four-axis model. Host STATIC and NECK_15 stay `6793bec7`. The historical device value for those two poses is `eae4bd6c`. That gap existed on the pre-move host oracle. This round did not retune trig or the shader to close it.

The same run checks ROI padding, a truncated file, loose stride, a 160×80 surface, three steady-state frames with the allocator trap, two instances, and a failed reload. `A_AFTER_B` prints hit 0 miss 0 because that update does not change parameters. Its CRC is still `d522cbf1`. The isolation check reads the counters before that second update.

## P4 timing, five-axis, one short capture

Historical column is `doc/optimization/phase31_srm_roi/summary.csv`, stage 1, same preset and the same model SHA. This capture is stage 0. STATIC and MULTI_AXIS use the same scene generator in both stages. One pair of runs is not an interleaved A/B and is not a soak.

Times are microseconds, average / p95 / max unless noted. Delta is this capture minus Phase 3B, divided by Phase 3B.

| MULTI_AXIS | Phase 3B stage 1 | This stage 0 | Delta |
| --- | --- | --- | --- |
| raster avg | 18928.96 | 18830.36 | -0.52% |
| PPA SRM avg | 4345.72 | 4360.56 | +0.34% |
| PPA total avg | 5418.94 | 5434.32 | +0.28% |
| producer avg | 27111.76 | 27096.92 | -0.05% |
| frame avg | 27332.66 | 27335.74 | +0.01% |
| frame p95 | 27817 | 27893 | +0.27% |
| frame max | 28085 | 27948 | -0.49% |
| deadline misses | 0/50 | 0/50 | 0 |
| rt30 avg | 928.96 | 937.50 | +0.92% |
| display lock avg (p95) | 25.10 (77) | 54.98 (211) | lock wait moved, not the raster |

STATIC frame average 25363.20 versus historical 25379.98 (-0.07%). STATIC raster 19044.24 versus 19112.96 (-0.36%). Deadline misses 0/50 on STATIC, EYE_L_SWEEP, and MULTI_AXIS.

The 3% investigation line is not crossed for raster, SRM, producer, or frame average/p95. Display-lock wait is higher on this single capture (max 510 us versus historical 227 us) and is not attributed to the runtime split. No extra framebuffer was added. The frame still does one full clear, one update+raster, ROI SRM, and a full submit.

## Code size

Recorded from the timing ELF before a later correctness rebuild replaced the build directory:

- P4 app binary 1703328 bytes. `.flash.text` 1172566, `.flash.rodata` 413856.
- Runtime objects together are about 82 KB of `.text` before GC. See `03-pruning-manifest.md`.
- Host `l2d_test_runtime` text 72552, data 808, bss 2120.
- S3 `l2d_s3_smoke.bin` 216832 bytes. Build verified with xtensa-esp32s3-elf-gcc 14.2.0. Not flashed.

Pool occupancy after load is in `docs/formats/compatibility.md`. Steady state does not call the port allocator. The 16 MiB pool is one allocation at create.

## Device CRC, five-axis, separate image

The timing `sdkconfig` was left unchanged in git. A temporary build set `CONFIG_L2D_PROFILE_CORRECTNESS=y`, flashed `/dev/ttyUSB0`, captured 459 rows, then restored the timing `sdkconfig` from a byte copy. `git diff` on `sdkconfig` is empty. The ignored `build/` directory can still contain that correctness ELF until the next `idf.py build`.

Capture metadata: same model SHA and size, `mode=correctness`, `srm_roi=1`, `scale_q100=100`, `esp_commit=adbc38aafe3cd5b92ee040dc446f17394e1bbda8` (HEAD at configure time, dirty tree). Rows are in `doc/optimization/refactor_embedded_runtime_correctness/`.

Compared with `doc/optimization/phase31_correctness_roi/correctness.csv` on pose, scale, frame_id, ARGB CRC, RGB565 CRC, render_ok, submit_ok, and backend: 459/459 equal. The same ARGB and RGB565 CRCs also equal `phase31_correctness_full`. Every row has `render_ok=1` and `submit_ok=1`. First row STATIC is ARGB `eae4bd6c`, RGB565 `8694d6e4`. Last row is `DYN_MULTI_119`, frame_id 459.

That signs the five-axis device pixels for this ROI SRM path against the Phase 3B captures. Host pixels for the same five-axis file are recorded separately in `MODEL_BASELINE.md`. The retired four-axis host CRC is not an acceptance result.

## Not signed

- S3 frame rate, panel, and long run. No board.
- Editor preview pixels. Sources are untouched and were not built on this host.
- Interleaved before/after timing on the same afternoon. The Phase 3B numbers are the previous short test, not a same-day pair.
- A long soak. Both the timing and CRC captures are one short pass.
