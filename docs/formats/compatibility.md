# File compatibility

Playback reads the existing PainterEngine `.live` file. This round does not define a new container.

## What loads

| Bytes | Support |
| --- | --- |
| `PainterEngineLiveDBinary` header, version 1, layers, textures, vertices, triangles, animations | Loaded by `PX_LiveFrameworkImport`. The only formal model is the five-axis `models/esp.live` (device path `/sdcard/esp.live`). |
| RT30 trailer, magic `RT30`, version 1 | Parsed when the remaining tail matches the trailer checks. The formal model reports 5 axes. |
| PXR3 chunks (`PXR3`, `BASE`, `TEX`, `RT30`, `LAYO`) in `PX_LiveDeviceFormat` | Reader helpers are linked because the RT30 trailer validator calls them. A full PXR3 asset pipeline is not a playback entry. Calling a validator does not mean an arbitrary PXR3 file becomes a running instance. |

## Wire layout is not the runtime struct

Import reads the legacy image with `src/format/l2d_wire.c`. Each integer and float is taken as an explicit little-endian field. A float is the IEEE754 bit pattern copied into a host float. Texture bytes are `r,g,b,a` and are copied by channel into `px_color`. Vertices and triangles are written field by field into runtime records. The file is not cast onto a runtime struct.

`PX_Delaunay_Triangle` is three 32-bit indices (12 bytes). The runtime does not call the Delaunay builder. `px_int` in these headers is a 32-bit `int` on both the host LP64 build and the device ILP32 build. `px_long` is not used in the wire structs that import reads.

Changing `sizeof(PX_LiveLayer)` or adding a field to `PX_LiveVertex` would desynchronize the file. New runtime state (trig counters, geometry bounds, profile times) sits on `PX_LiveFramework`, which import `memset`s. Those fields are not a disk image.

## Checks before a read

- Magic is tested only when at least 24 bytes are present.
- Width and height must be positive and at most 8192. Zero or larger canvas sizes fail before vectors are created.
- Negative layer, animation, or texture counts fail before vectors are created.
- Texture width and height must be positive. `width * height * 4` is checked for overflow and for a fit in the file before any pixel is read.
- Parent index must be `-1` or inside `[0, layerCount)`. A self-parent or a cycle fails after the layer records are read.
- Triangle indices must lie inside that layer's vertex array. Non-finite vertex positions or UV values fail.
- Animation frame payload size is checked before the pool allocation.
- A short or inconsistent RT30 trailer fails. A tail that is not RT30 is ignored.
- A vector slot is published only after its allocation exists, so a failed import can free what it created.

A truncated buffer fails the host test. Formal CRC is measured on the five-axis `models/esp.live` only. See `docs/formats/model-baseline.md`.

A failed import frees only the framework it was filling. Loading again into an engine that already holds a model leaves that model in place.

## Budgets

The file format does not encode an ESP32-P4 or ESP32-S3 memory ceiling. The public model loader currently reserves `2 * wire_bytes + 1 MiB` for its immutable model arena, with an overflow check. For the formal 800796-byte model this is 2650168 bytes; the measured used portion is 837184 bytes. Each instance has a separate mutable arena. The internal engine and S3 smoke example can still choose their own `pool_bytes`. The sizing rule is a conservative heuristic, so other models must be tested against their own import and memory requirements.

Formal memory numbers come from the five-axis `models/esp.live` host run and are recorded in `docs/formats/model-baseline.md`. Older pool notes for a four-axis git blob or for `release.live` are historical and are not acceptance.
