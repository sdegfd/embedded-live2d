# ESP32-P4 example

The board application stays in `firmware/esp32p4/live2d_rt30_baseline`.
It links `ports/esp_idf/components/live2d_engine`, which compiles the shared
sources under `src/`. This directory is the example entry in the framework
layout. It does not contain a second copy of the runtime.

Build from the firmware project after sourcing ESP-IDF v5.5.2:

```bash
cd firmware/esp32p4/live2d_rt30_baseline
idf.py build
```
