# Host headless example

The root CMake build runs the shared runtime without a display. `l2d_test_runtime` checks loader, Timeline/RT30 transitions, multi-instance isolation, raster CRC, ROI and allocation behavior using only `models/esp.live`. `l2d_host_benchmark` uses the same short-v1 axis scenarios as the P4 runner.

```sh
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime l2d_host_benchmark
ctest --test-dir build/host --output-on-failure
./build/host/l2d_host_benchmark
```
