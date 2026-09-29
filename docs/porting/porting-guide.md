# Porting

A new platform compiles the source list in `Live2D/cmake/l2d_sources.cmake` and provides the small port hooks declared in `Live2D/src/internal/l2d_pe_port.h`: allocation/free, log and microsecond clock. See `Live2D/ports/host/` and `Live2D/ports/esp_idf/`. Keep display, SD, DMA, cache and task code in the platform example.

Ports that should match the host pose CRCs use `-O2 -fno-strict-aliasing -ffp-contract=off`. GCC at `-O2` otherwise contracts floating-point expressions. The ESP-IDF component sets these flags for both ESP32-P4 and ESP32-S3. Do not put PPA, PIE, cache size or an RTOS into the shared source list.

The public ABI is C11. A caller supplies model bytes, an immutable model, one or more mutable instances, elapsed time and a BGRA8888 target. Model texture pixels and RT30 samples are shared across instances; persistent mutable state belongs to each instance. Platform allocation classes let an ESP port place large data in PSRAM without changing the core.

A panel with no PPA can use `l2d_convert_bgra_to_rgb565()`. The core does not require a panel or RTOS. A new display adapter must validate stride, alignment and buffer lifetime before submitting to DMA.

On ESP32-P4, CPU writes to cached PSRAM must be visible before a DMA reader consumes them; DMA writes must be visible before CPU reads. The P4 example owns this handoff around FILL, SRM and display submit. The [Espressif memory synchronization documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/mm_sync.html) defines C2M and M2C directions. The ESP-IDF PPA SRM driver also synchronizes its input and output windows; keep cache changes at ownership boundaries and validate them on hardware.

ESP32-S3 currently provides a headless build and API smoke entry under `examples/esp32s3/`. No S3 panel, pin map or measured frame rate is claimed.
