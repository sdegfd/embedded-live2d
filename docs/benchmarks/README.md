# Benchmark workflow

The common short-v1 preset and scenario drive live in `Live2D/bench/scenarios/`. Both `l2d_host_benchmark` and the P4 application use the same STATIC, EYE_L_SWEEP and MULTI_AXIS axis input sequence. Warmup is 5 frames, measurement is 50 frames, one round, scale 1.0. Host measures software update and raster; P4 measures display, PPA and scheduling as well. Compare within one platform and the same model SHA and firmware config.

1. Run `python3 Live2D/tools/check_l2d_model_baseline.py`.
2. Run `ctest --test-dir build/host --output-on-failure` and `./build/host/l2d_host_benchmark` after building the targets.
3. Build `examples/esp32p4/` with ESP-IDF v5.5.2. Confirm `/sdcard/esp.live` SHA from the boot log matches the manifest.
4. Capture with `python3 examples/esp32p4/tools/capture_l2d_profile.py --port /dev/ttyUSB0 --out docs/benchmarks/<run>` from the repository root.
5. For correctness, use the separate correctness build and compare all ordered CRC rows with `examples/esp32p4/tools/compare_l2d_crc.py`. Restore timing config before benchmarking.

Historical Phase 0–3B raw CSV, metadata and serial logs are retained in `docs/optimization/`. The [pre-reorganization refactor baseline](refactor-regression.md) reports 459/459 matching P4 correctness rows and a 27,335.74 us MULTI_AXIS average over 50 frames. The [final reorganization capture](reorg-final.md) repeats correctness and timing on the reorganized tree and records the regression.

The current P4 maintenance capture is [045 runtime closeout](../optimization/045-runtime-closeout.md). Quote it for the firmware that compiles the shared runtime with the host flags `-fno-strict-aliasing -ffp-contract=off`. MULTI_AXIS raster averages 10957.20 µs and the frame averages 19487.44 µs, with frame p95 20086 µs, max 20147 µs, and zero deadline misses. [044](../optimization/044-fast-nearest-kernel.md) is the same kernel at `763c358` before that flag alignment (MULTI_AXIS raster 10606.80 µs, frame 19184.78 µs). Against the reorganization capture, raster moved from 19237.90 µs to the 045 figure. Kept platform settings remain L2 cache 256 KB, L2 line 128 B, internal DMA reserve 128 KB, and 128-byte framebuffer alignment. The fast-nearest path blends BGRA as one word ([043](../optimization/043-nearest-word-blend.md)) and lives in `Live2D/src/raster/l2d_raster_nearest.c`. An earlier word-plus-safe-span trial on the 128 KB L2 image was reverted; see [040](../optimization/040-raster-word-span.md). Posture and deferred work are in [status](../architecture/status.md).
