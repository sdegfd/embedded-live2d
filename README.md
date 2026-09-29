# Live2D / PainterEngine 跨平台工作区

本仓库同时保存 PC 引擎、Windows 编辑器、ESP32-P4 独立渲染例程和实测报告。后续优化以此目录结构推进：

| 目录 | 用途 |
| --- | --- |
| `PainterEngine/` | PC 引擎和 RT30 源码 |
| `project/PainterEngine_LiveEditor/` | 当前 Windows 编辑器及构建脚本 |
| `firmware/esp32p4/live2d_rt30_baseline/` | ESP32-P4 Live2D 渲染基线、profiling、SD 示例模型 |
| `doc/` | 移植报告、CSV 和测试说明 |

ESP 工程地址：`firmware/esp32p4/live2d_rt30_baseline/`。它使用 ESP-IDF 5.5.2；进入该目录执行 `idf.py build`、`idf.py -p <串口> flash`。默认 `L2D_PROFILE_TIMING=y`，按 `L2D_PROFILE_STAGE` 选择场景，每场景预热 5 帧、采样 50 帧、只跑 1 轮，scale 固定为 1.0；每组结束通过串口输出原始记录和统计。PC 端运行 `python tools/capture_l2d_profile.py --port COM5 --out doc/profiling-run` 持续捕获，模型由使用者拷到设备 SD 卡根目录并命名为 `esp.live`。`L2D_PROFILE_CORRECTNESS` 单独运行固定姿态 CRC，`L2D_PROFILE_FINE` 与 `L2D_PROFILE_DETAIL` 单独统计诊断工作量，均不作为正式耗时数据。Phase 0–2 的结果和后续工作入口见 [交接文档](doc/optimization/HANDOFF_PHASE0_2.md)。

CPU 占用算法和 33.333 ms 帧预算等待的真机复测见 [CPU 占用与帧等待记录](doc/cpu-monitor-2026-09-28/README.md)。

Linux 下可单独编译 PC RT30 算法测试：

```bash
gcc -std=c11 -O2 -Wall -Wextra -I PainterEngine \
  project/PainterEngine_LiveEditor/tests/rt30_tests.c \
  PainterEngine/kernel/PX_LiveDeviceFormat.c PainterEngine/kernel/PX_LiveRealtime.c \
  PainterEngine/core/PX_MemoryPool.c PainterEngine/core/PX_Typedef.c \
  PainterEngine/core/PX_Log.c -lm -o /tmp/live2d_rt30_tests
/tmp/live2d_rt30_tests
```

Windows 可把仓库克隆到任意目录。用 Git 保存进度时，只提交源码、模型和说明。编译出来的程序、中间文件和旧的 `PainterEngine_make` 整包留在本机，不进入仓库。

当前要改的是这两处：

- `PainterEngine`：引擎。Windows 编辑器链接这里。
- `project/PainterEngine_LiveEditor`：正在使用的编辑器。`build_msvc.bat` 会编译它。

不要用 `PainterEngine_LiveEditor_source` 覆盖 `project/PainterEngine_LiveEditor`。前者是更早的、只有时间轴的副本。

编辑器编译：

```text
project\PainterEngine_LiveEditor\build_msvc.bat
```

无窗口性能程序：

```text
project\PainterEngine_LiveEditor\tests\build_live_bench.bat
project\PainterEngine_LiveEditor\tests\bench_build\live_bench.exe project\esp.live
```

进度说明在 `project/PainterEngine_新对话交接上下文.md`。
