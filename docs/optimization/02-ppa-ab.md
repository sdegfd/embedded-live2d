# Phase 2：PPA / CPU clear 与 convert 四组合 A/B

## 目标与实现

本阶段只选择现有清屏、ARGB8888→RGB565 转换后端，不修改 RT30、软件光栅、模型、采样器、缓冲尺寸或显示链。`L2D_CLEAR_CPU` 和 `L2D_CONVERT_CPU` 是常规运行的编译配置；Stage 2 在同一固件中依次切换已注册的 PPA client 和现有 CPU 路径。A= PPA/PPA，B= CPU/PPA，C= PPA/CPU，D= CPU/CPU。`profile_benchmark.cpp` 的 CSV 加入 backend 与 `convert_us`，CPU 和 PPA 转换都可直接测量。

构建与板端条件：ESP32-P4 rev1.3、CPU 360 MHz、PSRAM 200 MHz、ESP-IDF v5.5.2、`-O2`、320×320、同一 798660 字节模型（SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`）、scale 1.0、`fast-nearest`，渲染任务 Core 0/优先级 6。每个后端的 STATIC 和 MULTI_AXIS 各预热 5 帧、采样 50 帧、1 轮。完整配置见 [metadata](phase2_ab_timing/metadata.txt)。

## A/B 结果

单位 ms；表内是 50 帧平均，完整 avg/P50/P95/P99/min/max 见 [summary.csv](phase2_ab_timing/summary.csv)，逐帧见 [frames.csv](phase2_ab_timing/frames.csv)。`convert` 是整个后端转换调用；PPA SRM 阻塞时间另有 `ppa_srm_us` 字段。

| 后端 | 场景 | Raster | Clear | Convert | Producer | Frame | 33.333 ms miss |
|---|---|---:|---:|---:|---:|---:|---:|
| A PPA/PPA | STATIC | 18.64 | 1.13 | 6.53 | 26.72 | 27.02 | 0/50 |
| A PPA/PPA | MULTI_AXIS | 18.49 | 1.59 | 6.57 | 32.76 | 33.06 | 7/50 |
| B CPU/PPA | STATIC | 19.09 | 6.83 | 6.59 | 33.06 | 33.29 | 23/50 |
| B CPU/PPA | MULTI_AXIS | 18.84 | 6.73 | 6.60 | 38.43 | 38.70 | 50/50 |
| C PPA/CPU | STATIC | 18.83 | 1.64 | 11.41 | 32.45 | 33.43 | 27/50 |
| C PPA/CPU | MULTI_AXIS | 18.60 | 1.65 | 11.41 | 37.81 | 38.66 | 50/50 |
| D CPU/CPU | STATIC | 19.05 | 6.73 | 11.43 | 37.76 | 39.45 | 50/50 |
| D CPU/CPU | MULTI_AXIS | 18.83 | 6.74 | 11.42 | 43.25 | 44.99 | 50/50 |

| MULTI_AXIS 后端 | Producer P50/P95/P99 ms | Frame P50/P95/P99 ms | Frame 相对 A |
|---|---:|---:|---:|
| A PPA/PPA | 32.76/33.10/33.18 | 33.00/33.65/34.41 | 基准 |
| B CPU/PPA | 38.42/38.95/39.18 | 38.67/39.33/39.48 | +17.1% |
| C PPA/CPU | 37.81/38.12/38.18 | 38.58/39.34/39.38 | +16.9% |
| D CPU/CPU | 43.22/43.55/43.60 | 44.86/45.97/45.99 | +36.1% |

CPU clear 在当前 PSRAM 画布上约 6.7 ms，PPA clear 约 1.6 ms；CPU convert 约 11.4 ms，PPA SRM 约 6.6 ms。CPU 路径也改变后续光栅与 flush 的 cache 行为，因此选择依据是整帧和 Producer，而非单项耗时。四组正式计时共 400 帧，均无 frame drop、lock skip 或 panic；原始记录见 [serial.log](phase2_ab_timing/serial.log)。

## 正确性与保留决定

[correctness.csv](phase2_ab_correctness/correctness.csv) 包含四组各 15 个固定姿态，共 60 个 ARGB8888/RGB565 CRC。每个姿态的两个 CRC 均与 Phase 0 完全一致，60 次渲染与提交均成功；该校验固件与最终 timing 固件的渲染后端实现相同，后者仅多记录了 `convert_us`。

**保留 A：PPA clear + PPA convert。** B/C/D 没有性能或画质收益；常规配置维持两个 CPU 开关关闭。此结果只适用于当前 320×320 模型与 ESP32-P4 板端条件；若画布、PSRAM 或缓存策略改变，应再用相同 5+50 帧方法比较。后续重点仍是软件光栅与动态层级/顶点，而不是改换这两个后端。
