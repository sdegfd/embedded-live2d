# ESP32-P4 example

This is the WT99P4C5-S1 ESP-IDF v5.5.2 application. It links the one runtime from `Live2D/ports/esp_idf/components/live2d_engine`. PPA FILL, PPA SRM ROI, PSRAM/cache, SD and LCD remain here. The formal SD file is `/sdcard/esp.live`, matching `models/esp.live` SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`.

```sh
. /home/ubuntu/esp/esp-idf-v5.5.2/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash
python3 tools/capture_l2d_profile.py --port /dev/ttyUSB0 --out ../../docs/benchmarks/new-p4-run
```

The default `sdkconfig` is the short-v1 timing preset: stage 0, five warmup frames, 50 measured frames, scale 1.0 and a 33.333 ms deadline. Switch to `CONFIG_L2D_PROFILE_CORRECTNESS=y` for the separate CRC capture, then restore the timing config before performance measurement. The capture tool rejects a mismatched model. See [benchmark workflow](../../docs/benchmarks/README.md) and [P4 baseline](../../docs/benchmarks/refactor-regression.md).
