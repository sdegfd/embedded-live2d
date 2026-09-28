# PainterEngine Live2D → ESP32-P4 移植与 profiling 报告

日期：2026-09-28。PC 引擎基线：`78fc6347891611616dd75b16e7718aa8ba7a75e2`。ESP 源码目录：`firmware/esp32p4/live2d_rt30_baseline`。本次采样来自提交前的同一源码树，设备 metadata 因此写为 `uncommitted-worktree`；提交哈希以本报告所在的 Git 提交为准。

## 工程与移植范围

- `PainterEngine/`：PC 引擎；`project/PainterEngine_LiveEditor/`：当前 Windows 编辑器；`firmware/esp32p4/live2d_rt30_baseline/`：独立 ESP32-P4 例程。旧的 `PainterEngine_LiveEditor_source/` 只是历史副本。
- ESP 内核的 `PX_LiveRealtime.c/.h` 与 PC 提交 `78fc634` 的对应文件一致；包含连续采样位置、轴贡献缓存、纹理滞回和拓扑校验。ESP `PX_LiveFrameworkUpdate` 保留 PC 的姿态 revision 早退思路和已有 ESP 层级/顶点、光栅、PPA 路径。PC 版框架文件未整体覆盖 ESP 版。
- 本轮基线工作只加 frame ID、阶段计时、固定数组采样、独立工作量统计和测试编排；未实施透明像素占用、dirty rendering、frame reuse、fixed bilinear、layer sampler、texture crop 或 mesh rebuild。
- Windows 编辑器和两个 Windows 测试脚本的 `ENGINE_DIR` 改为相对仓库路径，因此不要求克隆到固定 `D:\live2d`。Windows GUI 必须由 Windows/MSVC 环境验证；本机 Linux 无法宣称已经编译该 GUI。Linux 下 PC RT30 测试 2305 项全部通过；ESP-IDF 5.5.2 在独立路径和仓库路径均编译成功。

## 板端架构与可观测边界

ESP32-P4 Rev 1.3，双核 FreeRTOS 配置；Live2D render 任务固定 CPU0/优先级 6，LVGL 任务固定 CPU0/优先级 4，CPU1 不承担此例程的渲染工作。CPU 360 MHz，PSRAM 200 MHz，编译 `-O2`。画布和 Live2D ROI 均为 320×320，LCD 为 1024×600。Live2D 维护一张 ARGB8888 渲染缓冲和一张 RGB565 帧缓冲；BSP 的 LVGL draw buffer 为单 buffer 配置。

实际链路：`frame_id → ARGB8888 producer → PPA FILL 清屏 → RT30/层级/软件光栅 → PPA SRM RGB565 → sys_display_flush → 面板 draw_bitmap`。帧号放在 `sys_display_buffer_t`，renderer 记录相同 ID，flush 读取该 ID 并记录 consumer/panel 提交 ID。`panel_submit` 仅表示面板 API 返回，不表示物理 scanout 完成。

本例程没有异步 producer/consumer frame slot，`publish→consume` 是同任务函数交接；`slot wait`、`slot miss`、`PPA blend` 为不适用（CSV 中按 0 记录）。`bounds_us` 是当前 dirty ROI 决策耗时，不是逐三角形包围盒统计；实际 dirty ROI 仍是完整 320×320。`ppa_wait_us` 为 PPA FILL 与 SRM 阻塞调用之和。`cache_sync_us` 为清屏后的 M2C 和 flush 前的 C2M。计时模式的帧循环不做动态分配，不打印逐帧日志；每组样本留在预分配 ring，组间经串口输出，PC 持续捕获。

## 模型与采集设置

设备 `/sdcard/esp.live`：798660 字节，SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`，与仓库 `project/esp.live` 一致。模型为 11 层、227 顶点、257 三角形、12 纹理、4 个 RT30 轴（left_eye/right_eye/neck/face）。采样器为 `fast-nearest`，软件光栅；PPA 用于 FILL 和 SRM 格式转换。加载耗时 263288 μs。模型由使用者自行拷入 SD，固件只读取，不输出模型文件。

按用户最新要求，正式采样改为约 30 秒短测：现有 4 轴模型运行 8 个场景 × 1.0/0.75 scale，每组预热 5 帧、记录 45 帧，总计 720 帧，约 27 秒；因模型无嘴轴，跳过嘴轴场景。逐轴场景在 50 帧内完成 0→29→0；multi-axis 使用确定性相位。P99 在每组仅 45 帧的条件下接近最大值，适合回归和发现异常，不作为稳定尾延迟结论。长测配置仍可通过 `L2D_PROFILE_LONG_RUN` 显式启用，但本轮未运行。

PC 使用 [串口捕获脚本](../tools/capture_l2d_profile.py) 保存原始数据：[frames.csv](profiling-2026-09-28/frames.csv)、[summary.csv](profiling-2026-09-28/summary.csv)、[metadata.txt](profiling-2026-09-28/metadata.txt)。summary 对每个阶段给出 avg/P50/P95/P99/min/max；全部 720 帧均可用 `frame_id` 关联。下面是短测的关键项，单位 μs：

| scale | 场景 | producer avg | P50 | P95 | P99 | raster avg |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1.0 | STATIC | 26404 | 26398 | 26623 | 26667 | 18370 |
| 1.0 | ALL_14_5 | 26136 | 26137 | 26280 | 26580 | 18016 |
| 1.0 | ALL_29 | 26324 | 26322 | 26619 | 26876 | 18353 |
| 1.0 | MULTI_AXIS | 32408 | 32426 | 32549 | 32651 | 18208 |
| 0.75 | STATIC | 20555 | 20553 | 20736 | 20815 | 12595 |
| 0.75 | ALL_14_5 | 20442 | 20413 | 20621 | 20744 | 12464 |
| 0.75 | ALL_29 | 20995 | 20983 | 21168 | 21239 | 13030 |
| 0.75 | MULTI_AXIS | 26508 | 26494 | 26768 | 26971 | 12944 |

720 帧中，`frame_drop=0`、`slot_miss=0`、`lock_skip=0`，`deadline_miss=3`（scale 1.0 的 FACE_SWEEP 1 帧、MULTI_AXIS 2 帧）。固定姿态的最终 ARGB CRC32_LE：1.0× STATIC `eae4bd6c`、ALL_14_5 `d522cbf1`、ALL_29 `1033faa1`；0.75× 分别为 `8fa036f7`、`2cd2e74d`、`73e8cc44`。这些值可用于后续确保 profiling/移植没有改变画面；跨模型不可比较。

独立 detail 编译模式已另行上板运行，PC 端直接从串口保存 [detail.csv](detail-2026-09-28/detail.csv)，共 16 个场景姿态 × 11 层 = 176 行。1.0× STATIC 一帧提交 257 个三角形、52762 个有效采样 fragment，其中最终 alpha 0/255/mixed 分别为 8437/34791/9534；uint8 overdraw buffer 显示 33488 个 covered pixels，平均 overdraw 1.576、最大 5。1.0× MULTI_AXIS 为 51479 fragments，covered 32801，平均 1.569、最大 6。主路径采用最近邻，每个有效 fragment 一次 sampler call，四个 bilinear taps 全透明项为 0/不适用。detail 构建插入了逐 fragment 计数，不能把它的耗时和正式 timing CSV 比较。

## 已验证与未覆盖

- 真机完成编译、烧录和短测，无 panic、Guru Meditation、Backtrace；固定姿态 checksum 不同，逐轴/多轴画面变化。性能表是板端串口导出的实测 CSV，不是估算。
- `L2D_PROFILE_DETAIL` 是单独编译的工作量模式，统计每层三角形、fragment、sampler call、四个 bilinear tap 全透明、最终 alpha 分类；可选 uint8 overdraw buffer，统计 covered pixels、平均/最大 overdraw。其耗时不可与正式 timing CSV 混合。当前主路径为最近邻，bilinear 四 tap 项为 0/不适用。
- 此独立例程没有接真实 controller，也没有完整系统应用负载；本轮 `MULTI_AXIS` 是确定性输入代理，不能标成“真实 controller 60s”或“完整系统负载 60s”。用户已将本轮验收改为约 30 秒短测。后续集成到完整系统时再补这两类测量。
- 当前采样只覆盖 1 轮，每组 45 帧；观察到 3 次 deadline miss，因此不能宣称所有姿态稳定锁定 30 FPS。后续可在同一源码与模型 SHA 下启用长测复核。
