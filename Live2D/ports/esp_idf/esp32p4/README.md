# ESP32-P4 port

The shared ESP-IDF component is `Live2D/ports/esp_idf/components/live2d_engine`. Allocation, clock and logging hooks are in `Live2D/ports/esp_idf/common/l2d_port_esp.c`. PPA FILL, PPA SRM ROI, cache sync, SD and the panel live in `examples/esp32p4/`; see its README for build and capture commands.
