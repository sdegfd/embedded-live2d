# ESP32-P4 port

P4-only image operations stay in the board example, not in this directory.

- Shared runtime component: `ports/esp_idf/components/live2d_engine`
- Allocator, log, and time: `ports/esp_idf/common/l2d_port_esp.c`
- PPA FILL, PPA SRM, cache sync, and the panel path: `firmware/esp32p4/live2d_rt30_baseline/components/live2d_renderer`
- Board, SD card, and tasks: `firmware/esp32p4/live2d_rt30_baseline`

`examples/esp32p4/README.md` points at that IDF project. Do not copy `src/` into the example.
