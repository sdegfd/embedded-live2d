# File compatibility

Playback reads the existing PainterEngine `.live` file. This round does not define a new container.

## What loads

| Bytes | Support |
| --- | --- |
| `PainterEngineLiveDBinary` header, version 1, layers, textures, vertices, triangles, animations | Loaded by `PX_LiveFrameworkImport`. The only formal model is the five-axis `project/esp.live` (device path `/sdcard/esp.live`). |
| RT30 trailer, magic `RT30`, version 1 | Parsed when the remaining tail matches the trailer checks. The formal model reports 5 axes. |
| PXR3 chunks (`PXR3`, `BASE`, `TEX`, `RT30`, `LAYO`) in `PX_LiveDeviceFormat` | Reader helpers are linked because the RT30 trailer validator calls them. A full PXR3 asset pipeline is not a playback entry. Calling a validator does not mean an arbitrary PXR3 file becomes a running instance. |

## Wire layout is not the runtime struct

Import copies headers into aligned local structs, then range-checks counts before `memcpy` of pixels, triangles, or vertices. Texture bytes are `r,g,b,a` and are copied by channel into `px_color`. Vertex and triangle arrays use the in-memory `PX_LiveVertex` and `PX_Delaunay_Triangle` sizes only after the byte count is known to fit.

`PX_Delaunay_Triangle` is three 32-bit indices (12 bytes). The runtime does not call the Delaunay builder. `px_int` in these headers is a 32-bit `int` on both the host LP64 build and the device ILP32 build. `px_long` is not used in the wire structs that import reads.

Changing `sizeof(PX_LiveLayer)` or adding a field to `PX_LiveVertex` would desynchronize the file. New runtime state (trig counters, geometry bounds, profile times) sits on `PX_LiveFramework`, which import `memset`s. Those fields are not a disk image.

## Checks before a read

- Magic is tested only when at least 24 bytes are present.
- Negative width, height, layer, animation, or texture counts fail before vectors are created.
- Texture width and height must be positive. `width * 4 * height` is checked for overflow and for a fit in the file before any pixel is read.
- Parent index must be `-1` or inside `[0, layerCount)`.
- Triangle and vertex counts must be non-negative, and both payloads must fit before the copy.
- Animation frame payload size is checked before the pool allocation and the copy.
- RT30 binding and sample ranges stay in the existing trailer validator.

A 64-byte buffer fails the host test. Formal CRC is measured on the five-axis `project/esp.live` only. See `docs/refactor/MODEL_BASELINE.md`.

Early failure before the framework is initialized returns without `PX_LiveFrameworkFree`. A failure after vectors exist uses the existing `_ERROR` free path. A failed reload still drops the previously loaded model. That order is unchanged so the pool peak stays the same.

## Budgets

The file format does not encode an ESP32-P4 or ESP32-S3 memory ceiling. The caller passes `pool_bytes`. The 16 MiB P4 pool and the 64 KiB S3 smoke pool are application choices.

Formal memory numbers come from the five-axis `project/esp.live` host run and are recorded in `docs/refactor/MODEL_BASELINE.md`. Older pool notes for a four-axis git blob or for `release.live` are historical and are not acceptance.
