# Embedded Live2D

A C runtime for resource constrained devices. It loads PainterEngine `.live` models and supports both Timeline animation playback and realtime RT30 multi-axis playback. One runtime is shared by Host, ESP32-P4 and ESP32-S3; platform display and memory code stays in each port or example.

| Path | Purpose |
| --- | --- |
| `Live2D/` | Public C API, loader, playback, raster, ports, tests and shared benchmark scenarios |
| `Live2D_Editor/` | Current Windows model editor and RT30 authoring tools |
| `examples/esp32p4/` | ESP-IDF display, SD, PSRAM, PPA and benchmark application |
| `examples/esp32s3/` | ESP-IDF headless build and API smoke example |
| `examples/host_headless/` | Host usage notes |
| `models/esp.live` | Only formal test model, five axes, SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100` |
| `PainterEngine/` | Original PC engine retained for reference and editor builds |
| `docs/` | Current architecture, format, porting and benchmark documentation plus historical work logs |

## Host

```sh
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime l2d_host_benchmark
ctest --test-dir build/host --output-on-failure
./build/host/l2d_host_benchmark
```

The test and benchmark refuse a model other than `models/esp.live`. The benchmark shares its scenario generator and short-v1 preset with the P4 runner; host timings are not P4 timings.

## ESP-IDF

Use ESP-IDF v5.5.2. Build `examples/esp32p4/` or `examples/esp32s3/` with `idf.py build`. The P4 example expects the same model at `/sdcard/esp.live`; its capture tool checks the model identity before accepting results. S3 is a headless build example and has no claimed display performance.

Read [the documentation index](docs/README.md) for the API, porting notes and measured baselines. Current posture and the P4 maintenance numbers are in [runtime status](docs/architecture/status.md). The retained PainterEngine sources have no project license declaration in this repository; resolve redistribution terms before a public release.
