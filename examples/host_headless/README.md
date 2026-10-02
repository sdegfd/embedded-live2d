# Host 无显示验证

从仓库根目录构建公共播放库：

```sh
cmake -S . -B build/host
cmake --build build/host -j
ctest --test-dir build/host --output-on-failure
./build/host/l2d_host_benchmark
```

默认覆盖 esp 的加载、Timeline/RT30、实例隔离、ROI 和分配行为；light 的 56 个姿态；20,000 个随机三角形像素对照；独立的 PC JPEG 导入测试。`-DL2D_RASTER_BATCH=ON` 增加条带输出对照，`-DL2D_ENABLE_ASAN=ON -DL2D_ENABLE_UBSAN=ON` 开启内存检查。

`l2d_host_benchmark` 运行与 P4 一致的 esp 短时动作。Host 时间用于代码回归，不代表嵌入式平台帧率。
