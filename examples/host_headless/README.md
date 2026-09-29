# Host headless runtime

The host test is the headless playback entry. It uses the four-axis model
stored in git blob `4174304b8374697b7dd8cfab034d5398f5243306` and does not
open `project/esp.live`.

```bash
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime
./build/host/l2d_test_runtime
```
