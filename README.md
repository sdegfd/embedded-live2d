# Embedded Live2D

面向嵌入式设备的二维角色动画框架，基于 [PainterEngine](https://github.com/matrixcascade/PainterEngine) 的 LiveFramework 播放内核进行裁剪和重构。提供独立 C API、模型与播放实例分离、Timeline 动画、RT30 多轴驱动、软件光栅以及 ESP32-P4 显示例程。

本项目使用 PainterEngine 的 `.live` 格式，与 Cubism 的 `.moc3` 格式不兼容。当前硬件验证平台为 **ESP32-P4 / WT99P4C5-S1**；Host 用于回归与像素对照，其他平台可以实现端口接口接入。

## 当前能力

- 模型持有只读纹理、拓扑、动画与 RT30 数据；实例持有私有姿态、播放游标和计算缓存。同一模型可以创建多个实例。
- 稳定播放阶段不调用系统内存分配器；文件解码检查长度、版本、维度和索引，错误路径释放已申请资源。
- 通用软件光栅保留最近邻采样、16.16 UV、覆盖规则和 BGRA 混合精度；缓存三角形系数，减少重复求值。
- P4 可用两个 CPU 核处理互不重叠的行，在每核 SRAM 条带里按图层顺序进行完整 BGRA 合成，再输出 RGB565。PPA 负责常规路径的清屏、格式转换及 ROI；条带路径将清屏和转换融合到 SRAM 中。
- 保留原始线性纹理与原始模型。分块格式、派生模型及转换工具已移除。
- 包含 Windows 编辑器、PSD/PSB 导入和模型检查工具的源码；嵌入式播放库不链接这些模块。

**30 FPS 是优化目标。当前复杂模型 13 轴连续运动约 25 FPS，尚未达到目标。** 光栅工作由 CPU 完成，当前结果不依赖 AI 外设加速。PIE、BitScrambler 等硬件实验选项默认关闭，不属于已测出的性能收益。

## 项目结构

```text
embedded-live2d/
├── Live2D/                 # 独立播放库、公共 API、Host/ESP-IDF 端口与测试
├── examples/
│   ├── esp32p4/            # P4 板级显示、PPA、PSRAM、SD、性能测试
│   └── host_headless/      # 无显示的 Host 测试说明
├── models/                 # esp.live、light.live 原始模型、JSON、身份清单
├── benchmarks/p4/          # 精简的实测 CSV 与像素基准
├── Live2D_Editor/          # Windows 编辑器与 PSD/PSB 导入
├── PainterEngine/          # 编辑器依赖的上游代码；不整体编入嵌入式固件
├── tools/live-inspect/     # .live → JSON 检查工具源码
├── .github/workflows/      # Host 回归 CI
├── LICENSE                 # 本项目代码：MIT
└── THIRD_PARTY.md           # 上游与第三方协议说明
```

内部审查日志、开发过程文档、串口原始日志、本机配置、编译产物和下载的依赖不纳入发布文件。

## 两个示例模型

| 模型 | 文件大小 | 画布 | 图层 / 纹理 | 顶点 / 三角形 | Timeline / RT30 轴 |
| --- | ---: | --- | --- | --- | --- |
| `esp.live` | 800,796 B | 320×320 | 11 / 12 | 227 / 257 | 1 / 5 |
| `light.live` | 1,248,800 B | 480×640 | 14 / 14 | 459 / 534 | 0 / 13 |

`esp.live` 的轴为眼睛、颈部、面部和嘴巴；`light.live` 增加头部三个方向、双臂、双腿、身体和前后头发。详细轴名及 SHA256 见 [模型清单](models/manifest.json)。两个例程分别选择模型，正常运行只加载一个模型。

## ESP32-P4 实测

测试环境：ESP32-P4 rev 1.3、CPU 360 MHz、32 MiB PSRAM / 200 MHz、256 KiB L2 cache / 128 B cache line、ESP-IDF v5.5.2、1024×600 横屏。两个模型的负载和渲染路径不同，下表用于说明各自表现。

| 模型 / 场景 | 输出区域 | FPS / 吞吐 | 平均帧耗时 | CPU0 / CPU1 |
| --- | --- | ---: | ---: | --- |
| esp：五轴连续运动 | 320×320，scale 1.0 | **约 30.31 FPS（实测）** | 19.477 ms | 约 53% / 0% |
| light：13 轴连续运动 | 480×600，scale 0.9375 | **25.562 FPS（实测提交）** | 38.544 ms | 98% / 72% |
| light：说话与转头 | 同上 | 25.016 FPS | 39.333 ms | 98% / 71% |
| light：摇摆与挥手 | 同上 | 24.986 FPS | 39.450 ms | 98% / 71% |
| light：原地踏步 | 同上 | 24.990 FPS | 39.448 ms | 98% / 71% |
| light：仅头部转动 | 同上 | 27.029 FPS | 36.354 ms | 98% / 76% |
| light：静止 | 同上 | 28.813 FPS | 34.095 ms | 98% / 81% |

- esp 本轮先进行三个短时场景（各预热 5 帧、测量 50 帧），再记录 12 个约一秒的五轴连续播放窗口。表中使用排除第一个过渡窗口后的 11 个窗口，FPS 按成功运行帧数和墙上时间统计；不能将帧耗时倒数当作实际显示 FPS。
- light 为整理后、依赖固定版本的实板复测：每场景预热 2 秒、测量 30 秒。FPS 按成功提交帧数 / 墙上时间计算，CPU 是每核总占用（Idle 时间差），并非单独渲染任务占用。
- light 的 13 轴全动场景光栅约 31.479 ms，姿态约 2.422 ms，显示提交约 1.946 ms；光栅统计包含条带清屏和 RGB565 打包。平均帧耗时与墙上时间 FPS 还存在调度间隔差异。
- 为适配横屏，light 采用固定等比缩放和 `(0,-20)` 原点。这是例程的显示布局设置；算法对照使用相同布局，不以降分辨率、减少轴或改变混合精度换取提速。
- Host 与 P4 的 56 个固定/组合/语义姿态 BGRA、RGB565 CRC 一致；Host 条带输出另作逐字节对照。原始模型字节不变。

精简结果、采样条件和复现说明见 [benchmarks/p4](benchmarks/p4/README.md)。另保留恢复版本的精简历史基线以便对照；历史耗时换算的吞吐不与当前实测 FPS 混用。

### 单模型运行的内存口径

以下为上面对应记录中的内存，**MiB = 1,048,576 字节**。`reserved` 是系统已经分配给内存池的容量；池内空闲部分依然占着 RAM。

| 项目 | esp 例程 | light 条带版本 |
| --- | ---: | ---: |
| 模型池 reserved | 2.527 MiB | 3.382 MiB |
| 模型池 used | 0.797 MiB | 1.304 MiB |
| 单实例池 reserved | 1.000 MiB | 1.000 MiB |
| 单实例池 used | 约 31.6 KiB | 约 116.3 KiB |
| 一组 BGRA + RGB565 画布 | 0.586 MiB | 1.648 MiB |
| 模型 + 单实例 + 画布的预留合计 | 约 4.11 MiB | **约 6.03 MiB PSRAM** |
| 整个例程 PSRAM 占用（含显示等） | 约 6.50 MiB | **约 8.39 MiB** |

light 双核条带另用 **128 KiB 内部 SRAM**（每核 64 KiB），以及任务栈、同步对象等。约 1.417 MiB 是模型池与实例池内部已用量之和，不能当成向系统申请的总量。当前 light 例程还保留完整 BGRA 画布作正确性抓取及备用路径。

Flash 内嵌模型不需要在 PSRAM 再存一份原始文件；SD 加载时临时文件缓冲在导入后释放。模型池容量包含加载预算，不等于文件大小。可用 `l2d_model_info()`、`l2d_instance_info()` 和平台堆统计核对应用自身的实际开销。

## Host 构建与验证

Linux 需要 C11 编译器、CMake、OpenSSL 开发包和 zlib 开发包。Ubuntu 可安装 `build-essential cmake libssl-dev zlib1g-dev`。

```sh
cmake -S . -B build/host
cmake --build build/host -j
ctest --test-dir build/host --output-on-failure
python3 Live2D/tools/check_l2d_boundaries.py
python3 Live2D/tools/check_l2d_model_baseline.py --model esp
python3 Live2D/tools/check_l2d_model_baseline.py --model light
```

测试条带路径及内存错误：

```sh
cmake -S . -B build/bands -DL2D_RASTER_BATCH=ON \
  -DL2D_ENABLE_ASAN=ON -DL2D_ENABLE_UBSAN=ON
cmake --build build/bands -j
ctest --test-dir build/bands --output-on-failure
```

覆盖文件边界与错误回收、Timeline/RT30 切换、多实例隔离、稳定阶段零分配、56 个 light 姿态，以及 20,000 个随机三角形对冻结参考光栅的逐像素比较；另验证 PC JPEG 导入的基线/渐进格式、边界及失败回收。Host 速度只用于代码回归，不能换算成 P4 的 FPS。

## P4 构建与运行

先安装并激活 **ESP-IDF v5.5.2**。当前预设对应 WT99P4C5-S1 revision 1.x 硬件，移植其他板卡时修改 BSP、显示和 PSRAM 配置。

```sh
. "$IDF_PATH/export.sh"
cd examples/esp32p4
# esp.live：将 models/esp.live 复制到 SD 卡根目录
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp -p /dev/ttyUSB0 flash monitor
```

单独构建 light（默认直接嵌入 Flash，启用双核 SRAM 条带）：

```sh
idf.py -B build_light -D SDKCONFIG=sdkconfig.light \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.light.defaults' build
idf.py -B build_light -D SDKCONFIG=sdkconfig.light -p /dev/ttyUSB0 flash
cd ../..
# 在已经激活的 IDF Python 环境中运行，使用 pyserial。
python3 examples/esp32p4/tools/capture_l2d_profile.py \
  --model light --port /dev/ttyUSB0 --reset --timeout 360 --demo-seconds 52 \
  --out captures/light-p4
```

需要从 SD 加载 light 时启用 `CONFIG_L2D_LIGHT_FROM_SD`，复制原始 `light.live` 到 SD 卡根目录。固件会核对模型 SHA256；不会自动改用另一个模型。测试完成后持续循环 13 轴全动、说话转头、摇摆挥手、原地踏步，每种动作 12 秒，继续以 30 FPS 为目标运行。

详细操作见 [P4 例程](examples/esp32p4/README.md) 和 [light 连续测试说明](examples/esp32p4/light-example.md)。

## API 与资源制作

播放库集成和端口约定见 [Live2D/README.md](Live2D/README.md)，公共 API 的输入、生命周期和线程约束写在 [头文件](Live2D/include/l2d/l2d.h) 中。模型必须晚于所有实例销毁；同一实例的更新和绘制由调用方串行执行。

Windows 资源制作见 [Live2D_Editor](Live2D_Editor/README.md)。编辑器使用 MSVC 构建；`.live` 数据可用 [live-inspect](tools/live-inspect/README.md) 导出为 JSON。仓库只分发源码和示例资源，不包含预编译 exe。

## 协议与致谢

项目代码采用 [MIT License](LICENSE)。感谢 PainterEngine / DBinary 提供原始框架；保留 [PainterEngine MIT 署名](PainterEngine/LICENSE)。PSD SDK、Espressif 显示适配器和 BSP 等遵循各自协议，详见 [第三方声明](THIRD_PARTY.md)。

两个示例模型已获提供方确认可随仓库再分发，资源许可范围见 [models/LICENSE](models/LICENSE)。
