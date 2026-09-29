# 045 Runtime closeout

Status: kept. This is the maintenance image of the fast-nearest kernel, not another raster algorithm.

Two changes land together on top of `763c358`:

- `l2d_raster_fast_nearest` moves from `PX_LiveFramework.c` to `Live2D/src/raster/l2d_raster_nearest.c`. Coverage, the 16.16 UV step and the BGRA word blend are the same formulas as [044](044-fast-nearest-kernel.md). The generic span stays in the legacy file.
- The ESP-IDF component compiles the shared runtime with `-O2 -fno-strict-aliasing -ffp-contract=off`, matching `Live2D/CMakeLists.txt`. GCC `-O2` otherwise contracts floating-point expressions, so P4 and S3 were not on the host CRC contract.

Host `l2d_test_runtime` after the move reported `L2D_RUNTIME_TESTS_OK` with the locked pose CRCs (`STATIC`/`NECK_15` `0x6793bec7`, `ALL_14_5` `0x540d2664`, `ALL_29` `0x7a64b4ad`). Those CRCs do not measure the P4 flag change. The historical 459-row device RGB565 file was not recaptured; it cannot pass or fail this image.

P4 short-v1 used the timing firmware: warmup 5, measure 50, scale 1.0, 360 MHz, 200 MHz PSRAM, 320×320, fast-nearest, PPA FILL, PPA SRM ROI. Model SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`. The boot log reports `optimization=-O2`. Deadline misses are 0. Raw rows: [phase4_runtime_closeout](phase4_runtime_closeout/summary.csv). Cache settings are unchanged from 044 (256 KB L2, 128-byte line, 128 KB DMA reserve, 128-byte framebuffer alignment).

| Scenario | 763c358 raster / frame | Closeout raster / frame |
| --- | ---: | ---: |
| STATIC | 10616.72 / 17299.68 | 10966.26 / 17508.50 |
| EYE_L_SWEEP | 10532.72 / 18594.02 | 10900.02 / 18904.50 |
| MULTI_AXIS | 10606.80 / 19184.78 | 10957.20 / 19487.44 |

MULTI_AXIS raster p95 / max moves from 10723 / 10800 µs to 11053 / 11065 µs. MULTI_AXIS frame p95 / max moves from 19782 / 20014 µs to 20086 / 20147 µs. Producer average moves from 18854.64 µs to 19249.96 µs. RT30 stays 818.20 → 805.82 µs, hierarchy 1313.86 → 1346.82 µs, clear 1113.76 → 1063.98 µs, and PPA SRM 4441.64 → 4495.98 µs. The raster average rises by about 350 µs on every scenario. That is the board cost of the new translation unit together with the host float contract. The flags stay; recovering the 10.61 ms figure by compiling P4 differently would drop the CRC contract.

No PIE, no PPA async, no cache retune, and no further raster pass. Later optional work is listed in [status](../architecture/status.md).
