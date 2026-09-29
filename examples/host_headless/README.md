# Host headless runtime

The host test is the headless playback entry. It opens the worktree file
`project/esp.live` and accepts only the five-axis baseline recorded in
`docs/refactor/MODEL_BASELINE.md`.

```bash
cmake -S . -B build/host
cmake --build build/host --target l2d_test_runtime
./build/host/l2d_test_runtime
```
