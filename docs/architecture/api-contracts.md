# API contracts

Public headers are `include/l2d/l2d.h` and the headers it includes. They are C11 with `extern "C"`. They do not include `esp_*.h`, `FreeRTOS.h`, `lvgl.h`, `windows.h`, `driver/ppa.h`, or `PainterEngine.h`.

`l2d_instance_t` and `l2d_output_t` are opaque. The P4 example still calls `live2d_engine_*` directly so the measured frame function stays one call. Those engine declarations live in `src/engine/live2d_engine.h` and are an internal C API used by the firmware, not a second runtime.

## Status

| Code | Meaning |
| --- | --- |
| `L2D_OK` | Completed. |
| `L2D_ERR_INVALID_ARG` | Null object, bad size, or stride smaller than the row. |
| `L2D_ERR_NO_MEM` | The port allocator returned null at create time. |
| `L2D_ERR_FAIL` | Import rejected the bytes. |
| `L2D_ERR_UNSUPPORTED` | Embedded-nearest was asked for a BGRA stride other than `width * 4`. |
| `L2D_ERR_RANGE` | A conversion would read or write outside the caller buffer. |

A failed `update` or `render_current` leaves the previous pose. `load_memory` is different: it releases the previous model before import. If import fails, the instance is empty. To switch models safely, load the new bytes into a second instance and keep the first until that load returns `L2D_OK`.

## Ownership and memory

`l2d_instance_create(pool_bytes, &out)` allocates the wrapper and one aligned pool through the port. The caller destroys both with `l2d_instance_destroy`. `load_memory` copies the file into that pool. The caller may free the file bytes after `L2D_OK`.

The P4 example passes 16 MiB. That size is the caller's choice. The S3 smoke creates a 64 KiB pool and only checks that a null model is rejected. A model that does not fit returns at load time.

After a successful load, `update`, `render_current`, and `pipeline_frame` do not call `l2d_port_alloc`. The host test arms an allocator trap around three frames and expects zero calls. `MP_Malloc` inside the existing pool is allowed. Create and load are the allocating calls.

Two instances do not share parameter state, trig counters, or the pool. Immutable instructions in the binary are shared. The caller must not update and render the same instance at the same time. There is no library-wide mutex.

The core does not read a clock and does not sleep. `elapsed_ms` is an argument. `render_current` does not advance the pose. `pipeline_frame` is one update plus one draw, matching `PX_LiveFrameworkRender`.

## Pixels

`L2D_PIXEL_BGRA8888_LE` memory order is `[B, G, R, A]`. The uint32 value is `B | (G<<8) | (R<<16) | (A<<24)`.

The raster writes already-composited RGB. Destination alpha is coverage from the shader. `l2d_bgra8888_to_rgb565` ignores alpha:

```text
((bgra >> 8) & 0xF800) | ((bgra >> 5) & 0x07E0) | ((bgra >> 3) & 0x001F)
```

RGB565 is `RRRRRGGGGGGBBBBB`, stored little-endian. Conversion does not multiply by alpha again.

Embedded-nearest indexes pixels as `x + width * y` and requires `stride_bytes == width * 4`. Any other stride returns `L2D_ERR_UNSUPPORTED`. `l2d_convert_bgra_to_rgb565` does accept a padded byte stride and checks `stride * height` against the caller sizes.

File textures are stored as `r,g,b,a`. Import copies those channels by name into `px_color`.

## Geometry and ROI

`l2d_geometry_bounds_t` is the screen-space min/max of vertices submitted to the raster.

- `valid` and not `unsafe`: finite bounds.
- neither flag: a legal empty frame.
- `unsafe`: a non-finite coordinate. Callers use the full target.

`l2d_roi_from_geometry` produces a half-open rect. Left/top is `floor(min) - 2`. Right/bottom exclusive is `ceil(max) + 3`, then clipped to the canvas. The host test checks bounds `(10.2, 20.2)-(30.1, 40.1)` on a 320 canvas and expects `x=8, y=18, w=26, h=26`.

`l2d_output_plan` returns the conversion rect and does not store it. The union is the previous frame's geometry ROI with the new one. The first use, a view discontinuity, an unsafe bound, or an empty union selects the full canvas. `l2d_output_commit` stores history only when the caller invokes it after a successful write. History is per `l2d_output_t`, so two targets do not share a rectangle.

The P4 renderer does not call `l2d_output_plan`. It keeps the previous firmware's fields and writes them before SRM returns, so the measured path stays the same. SRM failure switches that renderer to a full software convert and the frame function still returns success, because the fallback wrote the whole RGB565 buffer.

## Trig cache

Hit and miss counters live on the framework object (`trigCacheHit`, `trigCacheMiss`). An update zeroes them, then the render's pose step counts that frame. A second update that does not change parameters reports 0/0 even though the pixels stay the same. Comparison is `==` on the float angle. `+0` matches `-0`. A NaN misses. Reset and reload clear the cached angles with the rest of the framework.

## Thread and error summary

| Call | Allocates | Blocks | Thread |
| --- | --- | --- | --- |
| create / destroy | yes, port alloc/free | no | caller serializes the pointer |
| load_memory | inside the existing pool | no | same instance only |
| update / render_current / pipeline_frame | no port alloc after load | no | not concurrent on one instance |
| output plan / commit | no | no | caller serializes that output object |
| bgra to rgb565 helper | no | no | pure |
