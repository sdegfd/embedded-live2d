# 043 Nearest span word blend

Status: kept. Baseline is `cae376b` (256 KB L2, 128-byte line, 128-byte framebuffer alignment).

The fast-nearest span still sampled and wrote BGRA one byte at a time. On the P4 object, an opaque pixel was `lbu` plus `lhu` plus `lbu`, then `sh` plus two `sb`. The same formula through `px_color._argb.ucolor` lowers to one `lw`, an alpha check, and one `sw` when alpha is 255. Partial coverage uses the same channel formula as the byte loop: `((256-a)*dst + src*(a+1)) >> 8` and `255 - (((256-da)*(255-sa)) >> 8)`. Alpha 0 still skips the store. UV setup, scanline coverage, and the HDR `blend != NULL` loop are unchanged. There is no safe-span predicate and no approximate edge walker.

Host `l2d_test_runtime` reported `L2D_RUNTIME_TESTS_OK`, including the locked pose CRCs. P4 short-v1: warmup 5, measure 50, scale 1.0, 360 MHz, 200 MHz PSRAM, 320×320, fast-nearest, PPA FILL, PPA SRM ROI. Model SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`. Deadline misses are 0. Raw rows: [phase4_word_blend](phase4_word_blend/summary.csv). The previous column is [042](042-l2-line-128.md).

| Scenario | cae376b raster / frame | Word blend raster / frame |
| --- | ---: | ---: |
| STATIC | 16770.10 / 23305.32 | 15565.56 / 22137.36 |
| EYE_L_SWEEP | 16632.16 / 24731.06 | 15470.46 / 23540.92 |
| MULTI_AXIS | 16645.36 / 25098.22 | 15457.30 / 23976.12 |

MULTI_AXIS frame p95 / max moves from 25687 / 25963 µs to 24476 / 24751 µs. MULTI_AXIS raster p95 / max is 15623 / 15673 µs. Producer average moves from 24783.20 µs to 23712.46 µs. PPA SRM average is 4450.78 µs and clear stays near 1073 µs, so the saving stays in raster. No extra scratch buffer. Pixels match the host CRC lock. Raster is still about 15.5 ms of a 24.0 ms MULTI_AXIS frame.
