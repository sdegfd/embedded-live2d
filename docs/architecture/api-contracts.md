# Public API contracts

Include `l2d/l2d.h`. Public headers expose C ABI and do not include platform SDK or `PX_*` headers.

1. Load `models/esp.live` bytes through `l2d_model_load_memory()`. The returned model owns immutable data; input bytes may be released after success.
2. Create one or more instances with `l2d_instance_create()`. Destroy every instance before its model.
3. Choose Timeline with `l2d_instance_play_animation(instance, index)` or RT30 with `l2d_instance_enter_rt30(instance)`. Timeline supports pause, resume and stop. RT30 supports axis position, sample and batch writes.
4. Call `l2d_instance_update()` then `l2d_instance_render_current()`, or `l2d_pipeline_frame()` for the same order in one call. The caller supplies elapsed milliseconds and a tight BGRA8888 buffer. Rendering alone does not advance time.
5. Use `l2d_output_plan()` and `l2d_output_commit()` for conversion ROI history, or the software `l2d_convert_bgra_to_rgb565()` fallback.

One instance must not be updated and rendered concurrently. Separate instances share immutable model data and have separate playback state. The model must remain alive while instances exist. A failed model load returns no new model and does not destroy an existing one. A failed parameter write leaves the previous pose.

`L2D_PIXEL_BGRA8888_LE` stores bytes `[B,G,R,A]`. The nearest renderer requires `stride_bytes == width * 4`. A P4 panel adapter owns cache sync, PPA transactions and display submission; the runtime only writes pixels.

Only `models/esp.live` is accepted for formal correctness and performance results. The model checker and tests enforce its SHA256. See [model identity](../formats/model-baseline.md).
