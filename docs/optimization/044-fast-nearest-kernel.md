# 044 Dedicated fast-nearest raster kernel

Status: kept as the kernel step. The maintenance board numbers, after this kernel was moved to `Live2D/src/raster/l2d_raster_nearest.c` and the ESP component matched the host float flags, are in [045](045-runtime-closeout.md). The table below is the `763c358` capture. Baseline for that capture is `de409f8` (word blend on the 256 KB L2, 128-byte line, 128-byte framebuffer alignment).

The fast-nearest scanline lived inside `PX_LiveFrameworkRenderCurrent`. That function is large enough that the edge slopes, intercepts, and affine UV state spill to a multi-kilobyte stack frame. Every scanline reloaded them and then marshalled the full generic span arguments, including vertex UVs, position, and normal that the fast path does not read. The P4 object for that call site is about a hundred loads and stores before the span prologue.

`l2d_raster_fast_nearest` is a separate function (`noinline`, so it is not inlined back into `PX_LiveFrameworkRenderCurrent`). It is used only when fast-nearest sampling, a pixel shader, a texture, and a null blend are all set. That is the embedded runtime path. Timeline and RT30 both call it. The generic perspective span and the HDR `blend != NULL` loop are unchanged.

Coverage is the same scanline formula: `x = (y - b) / k`, with the same vertex sort, the same half-open Y ranges, and the same clip. UV at the clipped span start is still `(s * texture_width) * 65536` in 16.16, and the channel blend is still the word formula from 043. The only arithmetic change is that `s_fp_step` and `t_fp_step` are computed once per triangle from `affine_*_dx`, which is the same expression the span used to repeat. No reciprocal edge, no incremental edge, no safe-span predicate, no PIE.

Host `l2d_test_runtime` reported `L2D_RUNTIME_TESTS_OK` at the default host flags and again at `-O3`, including the locked pose CRCs (`STATIC`/`NECK_15` `0x6793bec7`, `ALL_14_5` `0x540d2664`, `ALL_29` `0x7a64b4ad`). P4 short-v1: warmup 5, measure 50, scale 1.0, 360 MHz, 200 MHz PSRAM, 320×320, fast-nearest, PPA FILL, PPA SRM ROI. Model SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`. Deadline misses are 0. Raw rows: [phase4_fast_kernel](phase4_fast_kernel/summary.csv). The previous column is [043](043-nearest-word-blend.md).

| Scenario | de409f8 raster / frame | Fast kernel raster / frame |
| --- | ---: | ---: |
| STATIC | 15565.56 / 22137.36 | 10616.72 / 17299.68 |
| EYE_L_SWEEP | 15470.46 / 23540.92 | 10532.72 / 18594.02 |
| MULTI_AXIS | 15457.30 / 23976.12 | 10606.80 / 19184.78 |

MULTI_AXIS frame p95 / max moves from 24476 / 24751 µs to 19782 / 20014 µs. MULTI_AXIS raster p95 / max is 10723 / 10800 µs. Producer average moves from 23712.46 µs to 18854.64 µs. RT30 stays 829.28 → 818.20 µs, hierarchy 1327.80 → 1313.86 µs, clear 1072.68 → 1113.76 µs, and PPA SRM 4450.78 → 4441.64 µs, so the saving stays in raster. No extra scratch or framebuffer. The kernel is a separate ~2 KB function with a 112-byte stack frame. Pixels match the host CRC lock at `-O0` and `-O3`.

Against the reorganization capture (`846b65e`), MULTI_AXIS raster moves from 19237.90 µs to 10606.80 µs and the frame average from 27896.66 µs to 19184.78 µs.

The remaining raster time is the per-pixel texture address, the PSRAM sample, the partial-alpha blend, and the per-scanline edge division. Reciprocal or incremental edges were not tried in this step; they would change rounding and need a pixel diff, and this split already passed the 15 ms and 13–14 ms raster goals without that.
