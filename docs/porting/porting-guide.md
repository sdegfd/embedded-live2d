# Porting

The core compiles without ESP-IDF, LVGL, or Windows. A new board needs a port file and, if it has a panel, its own display code.

## Minimum port

Declare the functions in `src/internal/l2d_pe_port.h`:

| Function | Host | ESP-IDF |
| --- | --- | --- |
| `l2d_port_alloc(align, size)` | `posix_memalign`. Alignment is raised to at least `sizeof(void*)` and a power of two. The block is not zeroed. | `heap_caps_aligned_alloc`. `MALLOC_CAP_8BIT`, plus `MALLOC_CAP_SPIRAM` when `CONFIG_SPIRAM` is set. |
| `l2d_port_free` | `free` | `heap_caps_free` |
| `l2d_port_log` | `fprintf` | `ESP_LOGE` / `ESP_LOGW` / `ESP_LOGI` |
| `l2d_pe_time_us` | `clock_gettime` | `esp_timer_get_time` |

Pixel CRC does not depend on the clock. Tests pass elapsed `33` and can run with a time function that returns 0.

`src/` does not read `CONFIG_*`. The ESP component maps `CONFIG_L2D_*` to `L2D_CFG_*` in `ports/esp_idf/components/live2d_engine/CMakeLists.txt`. Host tests define `L2D_CFG_SRM_ROI=1` so geometry bounds stay compiled. Bounds tracking does not change pixels.

A log line in `live2d_engine.c` still says the pool was allocated in PSRAM. On the host that string is only a log. The host allocator is `posix_memalign`.

## No RTOS

Link `l2d_runtime` (see the root `CMakeLists.txt`). The application allocates the pool, loads bytes, and calls `l2d_pipeline_frame` or `update` plus `render_current`. The library does not create tasks and does not wait 33.333 ms.

## No PPA

Use `l2d_convert_bgra_to_rgb565` for the panel format. It honors destination stride. Do not describe that helper as a hardware renderer. Triangle raster stays on the CPU in `live2d_engine.c`.

Return an error if a hardware unit cannot express the stride or alignment. Do not allocate a hidden full-frame staging buffer inside the core to hide that.

## Cache and DMA

The shared runtime does not call `esp_cache_msync`. The P4 renderer still performs the Phase 3B sync around FILL, SRM, and the full-frame submit. A sync that covers a sub-rectangle uses the row stride of the whole buffer. `roi_w * roi_h` is not the byte length when rows are not packed at the start of the allocation.

An empty cache callback is valid only for memory the device does not read through a cache-incoherent DMA path. The S3 scaffold has no DMA display path, so it does not register a no-op cache hook.

`present` in the P4 flush path means the driver accepted the buffer. It does not mean the panel scan finished. The benchmark field is `panel_submit_us`. There is no presented-FPS counter.

## P4 freeze used for the measured binary

- ESP-IDF v5.5.2
- `sdkconfig`: `CONFIG_L2D_PROFILE_TIMING=y`, `CONFIG_L2D_PROFILE_STAGE=0`, `CONFIG_L2D_SRM_ROI=y`
- correctness, fine, visual, and detail are off in that file
- canvas 320×320, synchronous PPA FILL and PPA SRM, original task core and priority
- engine compile options include `-O2` and do not add `-ffp-contract=off`

Host tests do pass `-fno-strict-aliasing -ffp-contract=off` so they match the host oracle. That flag pair is not on the device component.

## S3

`examples/esp32s3` sets the target to esp32s3 and requires only `live2d_engine`. The component requires `esp_common`, `esp_timer`, and `heap`. The linked map does not contain `libesp_driver_ppa`. No pins, panel, or PSRAM size are invented. `sdkconfig.defaults` sets the target and an 8192-byte main stack. The generated `sdkconfig` is gitignored.

Build that was run:

```bash
cd examples/esp32s3
idf.py -B build set-target esp32s3
idf.py -B build build
```

The result is an application image. It was not flashed. There is no S3 board on this machine.
