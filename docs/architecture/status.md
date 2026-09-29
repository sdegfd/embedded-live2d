# Runtime status

The shared library in `Live2D/` is the Embedded Live2D Runtime. Host, ESP32-P4 and ESP32-S3 compile `Live2D/cmake/l2d_sources.cmake` plus one port file. This tree is in maintenance: platform tuning stays in the platform example, and the runtime is not being specialized further for ESP32-P4.

## Formal model and playback

The only formal model is `models/esp.live` (device path `/sdcard/esp.live`).

| Field | Value |
| --- | --- |
| SHA256 | `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100` |
| Size | 800796 bytes |
| Canvas | 320×320, benchmark scale 1.0 |
| Content | 11 layers, 227 vertices, 257 triangles, 12 textures, 5 RT30 axes |

Timeline and RT30 are both official. An instance is in NEUTRAL, TIMELINE or RT30. `l2d_instance_reset_rt30()` resets RT30 parameters and does not switch modes. Steady update and render do not call the allocator.

The public C ABI is `Live2D/include/l2d/l2d.h`. It does not include a platform SDK or the internal `PX_*` headers. This closeout does not change that ABI.

## Raster

The verified fast-nearest path is `Live2D/src/raster/l2d_raster_nearest.c`. It runs when fast-nearest sampling, a pixel shader, a texture and a null blend are set. Coverage, 16.16 UV and the BGRA word blend are the formulas measured in [044](../optimization/044-fast-nearest-kernel.md). The generic span, including the HDR `blend != NULL` path, stays in `Live2D/src/internal/pe/kernel/PX_LiveFramework.c`.

Host and the ESP-IDF component compile this runtime with `-O2 -fno-strict-aliasing -ffp-contract=off`. That is the contract for pose CRCs. GCC at `-O2` otherwise contracts floating-point expressions.

## Correctness reference

`l2d_test_runtime` on the host is the software-raster reference for a later renderer, SIMD path, fixed-point path or another backend. After the kernel moved to its own file, the locked poses were unchanged:

| Pose | ARGB CRC |
| --- | --- |
| STATIC, NECK_15 | `0x6793bec7` |
| ALL_14_5 | `0x540d2664` |
| ALL_29 | `0x7a64b4ad` |

The historical P4 capture of 459 RGB565 CRC rows predates the dedicated kernel and this float-flag alignment. It is not a pass/fail gate for the current firmware. A new device CRC would be a new baseline. It was not recaptured in this closeout. The timing firmware leaves `CONFIG_L2D_PROFILE_CORRECTNESS` off.

## Current P4 baseline

P4 is the hardware check, not the center of the runtime. The maintenance short-v1 capture is [045](../optimization/045-runtime-closeout.md): same model, 320×320, scale 1.0, warmup 5, measure 50, 360 MHz CPU, 200 MHz PSRAM, fast-nearest, PPA FILL, PPA SRM ROI. MULTI_AXIS raster averages 10957.20 µs and the frame averages 19487.44 µs, with frame p95 20086 µs, max 20147 µs and zero deadline misses.

[044](../optimization/044-fast-nearest-kernel.md) remains the record of the kernel at `763c358`, before the ESP component matched the host float flags (MULTI_AXIS raster 10606.80 µs, frame 19184.78 µs). Quote 045 for the image that uses the host compile contract.

P4 cache policy stays in the example: 256 KB L2, 128-byte line, 128 KB internal DMA reserve, 128-byte framebuffer alignment. It is not a runtime dependency.

## Left in place

The public model, instance, Timeline, RT30, allocator, surface and output boundaries did not show a defect that justified an ABI change. The legacy framework file is still large; the hot kernel is no longer added to it. Warning suppressions on that legacy code stay. `PX_LiveDeviceFormat` PSRAM fields are wire-format budget checks, not an ESP-IDF dependency. `Live2D/src` and `Live2D/include` do not include ESP-IDF, FreeRTOS, LVGL, PPA or PIE.

## Later, if measured

These are optional and are not part of the runtime contract:

- Fixed-point or incremental edges, only with a pixel diff against the host CRCs.
- ESP32-P4 PIE as a backend behind the existing raster entry, not as a core dependency.
- Texture row pointers or other locality changes that stay bit-exact.
- Dirty-region raster, partial clear, or overlapping CPU and PPA work in the P4 example.
- An ESP32-S3 panel and a measured frame time. S3 today is the headless smoke in `examples/esp32s3/`.
- A fresh P4 RGB565 CRC baseline. Do not compare it to the pre-kernel 459-row file.
