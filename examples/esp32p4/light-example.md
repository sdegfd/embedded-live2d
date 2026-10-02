# light.live on ESP32-P4

The `light.live` workload is separate from the preserved five-axis `esp.live` regression baseline. It uses the shared C runtime and the existing PPA FILL, PPA SRM ROI and synchronous RGB565 panel path. Select it with a separate SDK configuration so the normal example's baseline config stays intact.

```sh
cd examples/esp32p4
. "$IDF_PATH/export.sh"
idf.py -B build_light -D SDKCONFIG=sdkconfig.light \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.light.defaults' build
idf.py -B build_light -p /dev/ttyUSB0 flash
cd ../..
python3 examples/esp32p4/tools/capture_l2d_profile.py \
  --model light --port /dev/ttyUSB0 --reset --timeout 300 --demo-seconds 52 \
  --out captures/light-p4-run
```

The default test embeds the exact `models/light.live` bytes in firmware flash and verifies SHA256 before import. `CONFIG_L2D_LIGHT_FROM_SD=y` selects `/sdcard/light.live` instead; that SD file must have the same SHA. Decoded model and frame resources reside in PSRAM in either case. The SD source is an option, not an automatic fallback.

The file is 1248800 bytes, SHA256 `4fe6e75270f59033a5be07073878ad0210e1e88cc3759fd16a2f9a56eadc7bda`, 480×640, 14 layers/textures, 459 vertices, 534 triangles and 13 RT30 axes. It has no Timeline animation. Axis `head_raw` keeps the spelling stored in the file. Eye and mouth samples 0 and 29 both return to the open shape; sample 15 tests their visible middle shape.

On the 1024×600 landscape panel the runtime uniformly scales the model to 0.9375, uses a 480×600 buffer, and draws at origin `(0,-20)` to center it correctly. PPA does format conversion at scale 1.0; the model fit is applied once in runtime geometry. This is an explicit display fit, not a raster performance experiment.

The suite first tests 56 ordered poses: rest; each axis at 0, 15 and 29; four combined stress poses; twelve semantic poses (three actions at four timestamps). Every PPA RGB565 frame is compared byte for byte with CPU conversion, then submitted to the panel. The Host checks the same sequence, mutable instance isolation, and zero allocator calls in steady playback:

```sh
cmake -S . -B build/light-host
cmake --build build/light-host --target l2d_test_runtime l2d_test_light
ctest --test-dir build/light-host --output-on-failure
./build/light-host/l2d_test_light --crc-out captures/light-p4-run/host_correctness.csv
python3 examples/esp32p4/tools/compare_light_crc.py \
  captures/light-p4-run/host_correctness.csv \
  captures/light-p4-run/light_correctness.csv
```

Seven phases each have 2 seconds warmup and 30 seconds measured by default, in this order: ALL_13_AXES, ALL_13_AXES_UNCAPPED, TALK_LOOK, SWAY_WAVE, STEP, HEAD_YAW and STATIC. Only the uncapped phase has no rate cap; the others target 30 FPS. The uncapped phase still yields for 1 ms after each frame so Idle can run. ALL_13_AXES exercises every axis with phase-offset full-range sample sweeps.

The three semantic actions use fractional RT30 positions and smooth waves driven by elapsed time, so their speed is independent of FPS. TALK_LOOK combines blinking, visual mouth motion, looking around, nodding and delayed hair motion. SWAY_WAVE combines body sway, a right-arm wave, head tilt and hair lag. STEP alternates arms and legs with a small body bounce in place. All three drive all 13 axis parameters. These are example motions synthesized from axis names; visual mouth motion has no audio source, and stepping has no world movement.

`light_summary.csv` contains successful panel submissions divided by measured wall time, deadline overruns against 33.333 ms, pipeline timings, and each core's non-Idle percentage. `cpu_windows.csv` retains the one-second samples. CPU sampling runs on core 1 using FreeRTOS Idle runtime deltas. It is total core occupancy, not just the render task. Memory diagnostics run only at phase boundaries, and window rows print after the measured phase. Thus neither heap traversal nor UART CSV output is inside the measurement loop. After `L2D_DONE`, the display cycles ALL_13_AXES → TALK_LOOK → SWAY_WAVE → STEP indefinitely, with 12 seconds per action at a 30 FPS target. The capture tool normally stops at `L2D_DONE`; `--demo-seconds 52` records the following playback on the same serial connection. The firmware continues playing after capture. On Linux the helper disables HUPCL to suppress hangup control-line changes. Some boards still reset when a serial connection opens, so use one capture with `--demo-seconds` to observe the full suite and ongoing demo. The target is not a claim of measured 30 FPS.
