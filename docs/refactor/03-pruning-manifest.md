# Pruning manifest

Compile list: `cmake/l2d_sources.cmake`. It names each `.c`. It does not glob `core/*.c` or `kernel/*.c`.

The P4 timing ELF (`nm -g --defined-only`) retains these `PX_*` globals. If a helper is absent here, the timing image does not call it.

## Linked PX globals

Memory and checks: `PX_ASSERT`, `PX_ERROR`, `PX_memcpy`, `PX_memset`, `PX_memequ`, `PX_memdwordset`, `PX_AllocFromFreq`.

Math and points: `PX_sin_angle`, `PX_cos_angle`, `PX_sind`, `PX_sqrt`, `PX_sqrtd`, `PX_COLOR`, `PX_POINT`, `PX_POINT2D`, `PX_PointAdd`, `PX_PointSub`, `PX_PointMul`, `PX_PointDot`, `PX_PointMod`, `PX_PointNormalization`, `PX_PointMulMatrix`, `PX_Point2DAdd`, `PX_Point2DMul`, `PX_Point2DDot`, `PX_Point2DMod`, `PX_Point2DNormalization`.

Geometry draws: `PX_GeoDrawLine`, `PX_GeoDrawArrow`, `PX_GeoDrawBorder`, `PX_GeoDrawCircle`, `PX_GeoDrawSolidCircle`.

Containers and pixels: `PX_VectorInitialize`, `PX_VectorErase`, `PX_VectorFree`, `PX_Quicksort_ArrayMaxToMin`, `PX_Quicksort_MaxToMin`, `PX_SurfaceCreate`, `PX_SurfaceFree`, `PX_SurfaceDrawPixel`, `PX_SurfaceDrawPixelWithoutLimit`, `PX_TextureCreate`, `PX_TextureFree`, `PX_TextureRender`.

Live playback: `PX_LiveFrameworkImport`, `PX_LiveFrameworkFree`, `PX_LiveFrameworkRender`, `PX_LiveFrameworkRenderCurrent`, `PX_LiveFrameworkReset`, `PX_LiveFrameworkPlay`, `PX_LiveFrameworkPlayAnimation`, `PX_LiveFrameworkUpdateLayerRenderVerticesUV`, `PX_LiveFrameworkGetGeometryBounds`, `PX_LiveEnterRealtime30`, `PX_LiveSetRealtimeAxisSample`, `PX_LiveFindRealtimeAxisById`, and the `PX_LiveRealtime*` / `PX_LiveDevice*` symbols in the same ELF (initialize, enter, leave, update, reset, install baked axis, hash, memory stats, reader, safe add/mul/align).

## Object size, P4 `-O2` (.text / .bss)

| Object | text | bss |
| --- | --- | --- |
| `PX_Typedef.c` | 6790 | 0 |
| `PX_Log.c` | 77 | 0 |
| `PX_MemoryPool.c` | 2032 | 0 |
| `PX_Quicksort.c` | 942 | 0 |
| `PX_Surface.c` | 956 | 0 |
| `PX_Texture.c` | 776 | 0 |
| `PX_BaseGeo.c` | 80 | 0 |
| `PX_Vector.c` | 1554 | 0 |
| `PX_LiveDeviceFormat.c` | 8139 | 0 |
| `PX_LiveFramework.c` | 38512 | 2092 |
| `PX_LiveRealtime.c` | 13776 | 0 |
| `live2d_engine.c` | 5453 | 0 |
| `l2d_api.c` | 1322 | 0 |
| `l2d_roi.c` | 1346 | 0 |
| `l2d_port_esp.c` | 704 | 8 |

Sum of those `.text` sizes is 82459 bytes before garbage collection. `l2d_api.c` is not in the P4 ELF.

Whole timing image: `.flash.text` 0x11e456, `.iram0.text` 0x19406, `.flash.rodata` 0x0650a0, `.dram0.data` 0x3180, `.dram0.bss` 0x26fc, `riscv32-esp-elf-size` text 1690180, binary 1703328 bytes. Most of that image is the board, LVGL, and the IDF, not the Live2D list.

Host test after `--gc-sections`: text 72552, data 808, bss 2120.

S3 smoke image: text 163021, data 53692, bss 372205, binary 216832 bytes. `l2d_s3_smoke.map` does not mention `libesp_driver_ppa`.

## Dropped from the link on purpose

| Original | Action | Test |
| --- | --- | --- |
| `PX_Memory.c` | source kept on disk, not compiled | Host and P4 link. |
| `PX_Delaunay.c` | source kept, header kept for the index struct | ELF has no `PX_Delaunay` builder symbol. |
| String helpers only used by export / create-layer | left in `PX_LiveFramework.c`, GC removes the calls | P4 `nm` has no `PX_strcpy`. Host test links. |
| Rest of `PX_Typedef.c` | deleted from this snapshot's `.c` | Four-axis CRC unchanged, so the deleted helpers were not on the raster path. |
| Second firmware copy of the engine | directory removed | P4 `idf.py build` uses `ports/esp_idf/components/live2d_engine`. |

`PX_Typedef.h` still declares many unused prototypes. The public ABI does not include that header. Shrinking the header further would be a later edit. It is not required to keep the unused `.c` bodies.

## License column

See `02-dependency-audit.md`. No license text was invented for the PainterEngine sources.
