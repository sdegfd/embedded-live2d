# 040 Word span trial

Status: measured, not kept. Source returned to `846b65e` for `Live2D/src/internal/pe/kernel/PX_LiveFramework.c`.

The fast-nearest span in `PX_LiveFramework_RenderAffinePixelShaderSpan` was changed to load and store BGRA as one 32-bit word, and to skip the per-pixel texture bounds check when both 16.16 endpoints stayed inside the texture. UV setup, coverage, and the blend formula were not changed. The HDR `blend != NULL` loop was left as the original byte path. No approximate edge walker was added.

Host runtime tests matched the locked pose CRCs (`STATIC/NECK_15` `0x6793bec7`, `ALL_14_5` `0x540d2664`, `ALL_29` `0x7a64b4ad`). P4 `short-v1` used the same conditions as [reorg-final](../benchmarks/reorg-final.md): warmup 5, measure 50, scale 1.0, 360 MHz CPU, 200 MHz PSRAM, 320×320, fast-nearest, PPA FILL, PPA SRM ROI, synchronous RGB565. Model SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`. Raw rows: [phase4_raster_span](phase4_raster_span/summary.csv).

| Scenario | Baseline raster avg | Trial raster avg | Baseline frame avg | Trial frame avg | Trial frame p95 | Deadline misses |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| STATIC | 19384.86 | 19512.94 | 25826.04 | 25935.00 | 26489 | 0 |
| EYE_L_SWEEP | 19315.70 | 19429.94 | 27319.54 | 27495.34 | 27807 | 0 |
| MULTI_AXIS | 19237.90 | 19395.46 | 27896.66 | 28041.72 | 28432 | 0 |

Baseline MULTI_AXIS frame p95 is 28627 µs and max is 28809 µs. The trial frame p95 is 28432 µs and max is 29169 µs. Raster did not improve. The word loop removes byte splits that already hit the same 64-byte cache line, and the int64 inside-test adds work on every span. PSRAM fill latency remains the stall.

`__builtin_prefetch` lowers to nothing with `-march=rv32imafc_zicsr_zifencei_xesppie`. A 4-wide unrolled span compiled to about 0x49c bytes with stack spills and was not built into firmware.

The following measurement is [041-l2-cache-256.md](041-l2-cache-256.md). It does not include this span trial.
