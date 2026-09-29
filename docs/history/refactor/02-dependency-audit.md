# Dependency audit

The playback closure was taken from the ESP32-P4 timing ELF built on this branch before the temporary correctness rebuild:

`firmware/esp32p4/live2d_rt30_baseline/build/live2d_rt30_baseline.elf`

`riscv32-esp-elf-nm -g --defined-only` shows 82 global `PX_*` symbols and the `live2d_engine_*` / `l2d_roi_*` / `l2d_port_*` symbols listed in `03-pruning-manifest.md`. The ELF does not define `PX_strcpy`, `PX_strequ`, `PX_MemoryCat`, `PX_Delaunay*`, or `l2d_instance_*`.

`l2d_instance_*` is still compiled into `liblive2d_engine.a`. The P4 app calls `live2d_engine_*`, so the linker drops the instance wrapper. The S3 smoke calls `l2d_instance_create` and the symbol is in `l2d_s3_smoke.elf`.

## Why each compiled file is present

| Source | Why it stays |
| --- | --- |
| `PX_Typedef.c` | The linked trig, point, color, and memory helpers. The file was reduced to those helpers (880 lines). FFT, strings, and the unused math bulk are gone. |
| `PX_Log.c` | `PX_ASSERT`, `PX_ERROR`. |
| `PX_MemoryPool.c` | The single arena used by import and realtime tables. |
| `PX_Vector.c` | Layer, texture, animation, and vertex arrays. |
| `PX_Quicksort.c` | Vector ordering. `PX_Quicksort_MinToMax` is referenced from `PX_Vector.c`. |
| `PX_Surface.c` | Raster destination. |
| `PX_Texture.c` | Imported textures and `PX_TextureRender`. |
| `PX_BaseGeo.c` | The five draw functions the framework still calls (`PX_GeoDrawLine` and the other four globals in the ELF). |
| `PX_LiveFramework.c` | Import, hierarchy, render, trig cache. Export and layer-create helpers remain in the same translation unit. `--gc-sections` drops their unreferenced callees. |
| `PX_LiveRealtime.c` | RT30 evaluation. |
| `PX_LiveDeviceFormat.c` | Trailer reader and the overflow helpers the validator calls. |
| `live2d_engine.c` | Load, nearest shader, render entry. |
| `l2d_api.c` | Public instance API. Linked on host and S3. |
| `l2d_roi.c` | Conversion rectangle. Linked on P4 because the renderer calls `l2d_roi_from_geometry`. |

## Not compiled

| File | Reason |
| --- | --- |
| `PX_Memory.c` | Zero references in the previous production ELF. Host link without this file succeeds once `--gc-sections` is on. |
| `PX_Delaunay.c` | Playback only needs the 12-byte index struct in `PX_Delaunay.h`. The builder is an editor operation. |
| Root `PainterEngine/core/*.c` (62 files) and `kernel/*.c` (103 files) | Editor and PC tools. Wildcard compilation of that tree is the Windows editor script, not the firmware. |
| GUI, font, audio, FFT, network, script, 3D, PNG/JPEG/PSD import | Not in `L2D_RUNTIME_SOURCES`. |

## Header boundary

`PX_Typedef.h` includes `l2d_config.h` instead of `sdkconfig.h`. Public `include/l2d/` does not include `PX_Core.h` or `PainterEngine.h`. Firmware code that still touches `px_surface` includes `PX_Surface.h` from the renderer, which is platform-side.

`PX_LiveFramework.h` wraps only the diagnostic prototypes in `extern "C"`. C++ in the P4 example calls `live2d_engine_*`, which is already `extern "C"`.

## License

The extracted PainterEngine files in this repository do not carry a license banner, and the repo root has no `LICENSE` file that names them. This refactor does not assign a new license or a new author to those files. Copyright lines that already exist in unrelated platform snippets (tinyalsa, minifb) stay in root `PainterEngine/` and are not part of the runtime list.

## Editor boundary

`git diff` against `PainterEngine/` and `project/PainterEngine_LiveEditor/` is empty. The editor project was not rebuilt here (no MSVC). Its `build_msvc.bat` still wildcards the root engine, including GUI and import code, and that is the intended authoring build.
