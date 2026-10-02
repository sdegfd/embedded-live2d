# Embedded Live2D

Embedded Live2D 是面向嵌入式设备的二维角色动画框架，可用于桌面宠物、交互角色和带动画的设备界面。项目基于 [PainterEngine](https://github.com/matrixcascade/PainterEngine) 的 LiveFramework，提供独立的 C11 播放库、Windows 资源编辑器和 ESP32-P4 例程。

播放库支持 Timeline 动画和 RT30 多轴姿态驱动，通过软件光栅将模型绘制到应用提供的画布。同一个模型可以创建多个播放实例，分别控制动作与姿态。应用可通过端口接口接入内存管理、计时和日志，并将绘制结果交给自己的显示系统。

项目使用 PainterEngine 的 `.live` 模型格式，与 Cubism 的 `.moc3` 格式不兼容。已验证的平台为 **ESP32-P4 / WT99P4C5-S1**，同时提供用于开发和测试的 Host 端口。

## 快速开始

以下命令从仓库根目录执行。

### Host 构建

需要 C11 编译器、CMake、OpenSSL 和 zlib 开发包。Ubuntu 可先安装依赖：

```sh
sudo apt-get install build-essential cmake libssl-dev zlib1g-dev
cmake -S . -B build/host
cmake --build build/host -j
ctest --test-dir build/host --output-on-failure
```

Host 测试无需显示窗口，覆盖模型加载、动画播放、多实例隔离和像素一致性。条带渲染和内存检查的构建方式见 [Host 测试说明](examples/host_headless/README.md)。

### ESP32-P4 运行

安装并激活 **ESP-IDF v5.5.2**。例程预设对应 WT99P4C5-S1 revision 1.x、32 MiB PSRAM 和 1024×600 MIPI 显示屏；其他板卡需要调整 BSP 和显示配置。

运行 `light.live`：模型默认内嵌在 Flash 中，无需 SD 卡，并启用双核 SRAM 条带渲染。

```sh
. "$IDF_PATH/export.sh"
cd examples/esp32p4
idf.py -B build_light -D SDKCONFIG=sdkconfig.light \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.light.defaults' build
idf.py -B build_light -D SDKCONFIG=sdkconfig.light -p /dev/ttyUSB0 flash monitor
```

例程会先运行姿态校验与性能测试，然后循环播放 13 轴运动、说话转头、摇摆挥手和原地踏步。

运行 `esp.live`：将仓库中的 `models/esp.live` 复制到 SD 卡根目录，再使用独立的配置构建。以下命令在 `examples/esp32p4` 目录执行：

```sh
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp -p /dev/ttyUSB0 flash monitor
```

请按实际连接修改串口名称。SD 加载选项、性能日志抓取和配置说明见 [P4 例程](examples/esp32p4/README.md) 与 [light 例程](examples/esp32p4/light-example.md)。

## 示例模型与性能

仓库提供两个可随项目再分发的示例模型，例程每次加载一个模型。

| 模型 | 文件大小 | 画布 | 图层 / 纹理 | 顶点 / 三角形 | Timeline / RT30 轴 |
| --- | ---: | --- | --- | --- | --- |
| `esp.live` | 800,796 B | 320×320 | 11 / 12 | 227 / 257 | 1 / 5 |
| `light.live` | 1,248,800 B | 480×640 | 14 / 14 | 459 / 534 | 0 / 13 |

`esp.live` 包含眼睛、颈部、面部和嘴巴的动作轴；`light.live` 还包含头部三个方向、双臂、双腿、身体和头发的动作轴。轴名及模型校验信息见 [模型清单](models/manifest.json)。

### ESP32-P4 实测

测试板为 WT99P4C5-S1 / ESP32-P4 rev 1.3，CPU 360 MHz、32 MiB PSRAM / 200 MHz、256 KiB L2 cache / 128 B cache line，使用 ESP-IDF v5.5.2。显示屏为 1024×600，模型绘制区域如下：

| 模型 / 场景 | 绘制区域 | 实测 FPS | 平均帧耗时 | CPU0 / CPU1 |
| --- | --- | ---: | ---: | --- |
| esp：五轴连续运动 | 320×320，scale 1.0 | **30.31** | 19.477 ms | 约 53% / 0% |
| light：13 轴连续运动 | 480×600，scale 0.9375 | **25.562** | 38.544 ms | 98% / 72% |

light 使用等比缩放和 `(0,-20)` 原点适配横屏。FPS 按运行帧数或成功显示提交数与实际经过时间统计；CPU 表示每个核心的总占用。两个模型的负载和渲染路径不同，数据适用于表中的例程配置。

更多动作场景、测量条件、CSV 和像素对照基准见 [性能测试](benchmarks/p4/README.md)。

### 内存占用

下表对应单模型、单播放实例的上述例程配置。MiB = 1,048,576 字节；内存池预留容量是实际向系统申请的内存，包含池内空闲空间。

| 项目 | esp 例程 | light 例程 |
| --- | ---: | ---: |
| 模型池预留 / 已用 | 2.527 / 0.797 MiB | 3.382 / 1.304 MiB |
| 实例池预留 / 已用 | 1.000 MiB / 约 31.6 KiB | 1.000 MiB / 约 116.3 KiB |
| BGRA + RGB565 画布 | 0.586 MiB | 1.648 MiB |
| 模型 + 实例 + 画布预留合计 | 约 4.11 MiB PSRAM | **约 6.03 MiB PSRAM** |
| 整个例程 PSRAM 占用（含显示等） | 约 6.50 MiB | **约 8.39 MiB** |

light 条带渲染另需 128 KiB 内部 SRAM（每核 64 KiB），任务栈和同步对象另计。实际开销随模型、画布和平台配置变化，可通过 `l2d_model_info()`、`l2d_instance_info()` 及平台堆统计查询。

## 项目结构

```text
embedded-live2d/
├── Live2D/                 # C11 播放库、公共 API、平台端口与测试
├── examples/
│   ├── esp32p4/            # ESP32-P4 显示与动画例程
│   └── host_headless/      # Host 测试说明
├── models/                 # 示例模型与模型清单
├── benchmarks/p4/          # 实板性能数据与像素基准
├── Live2D_Editor/          # Windows 编辑器与 PSD/PSB 导入
├── PainterEngine/          # 编辑器使用的上游框架源码
├── tools/live-inspect/     # 模型信息检查工具
├── .github/workflows/      # Host 自动测试
├── LICENSE                 # 项目代码许可
└── THIRD_PARTY.md           # 第三方依赖与许可说明
```

## 集成与资源制作

| 用途 | 文档 |
| --- | --- |
| 接入播放库、管理模型与实例、移植到新平台 | [播放库说明](Live2D/README.md) |
| 查询 API、输入约束和线程约定 | [公共头文件](Live2D/include/l2d/l2d.h) |
| 在 Windows 编辑图层、网格、动画和姿态，导入 PSD/PSB | [编辑器说明](Live2D_Editor/README.md) |
| 检查 `.live` 数据并导出 JSON | [live-inspect](tools/live-inspect/README.md) |

嵌入式应用链接 `Live2D/` 播放库即可。Windows 编辑器使用 MSVC 构建，依赖仓库中的 PainterEngine 源码。仓库提供源码与示例资源。

## 开源协议与致谢

项目代码采用 [MIT License](LICENSE)。项目基于 PainterEngine / DBinary 的工作，保留 [PainterEngine MIT 署名](PainterEngine/LICENSE)。PSD SDK、Espressif 显示适配器和 BSP 等依赖遵循各自协议，详见 [第三方声明](THIRD_PARTY.md)。

示例模型的角色与贴图许可见 [模型资源许可](models/LICENSE)。
