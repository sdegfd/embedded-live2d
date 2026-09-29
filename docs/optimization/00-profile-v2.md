# Phase 0：ESP32-P4 Live2D Baseline v2

## 目标和改动

本阶段仅完善观测。`main/profile_benchmark.cpp` 将 PPA FILL、PPA SRM、各处 cache sync、显示锁等待、面板提交、flush 全程和整帧时间单独输出；固定姿态 CRC 在独立 correctness 固件运行，正式 timing 热路径不计算 CRC。`main/Kconfig.projbuild` 增加测试阶段与 correctness 开关，`sys_display_flush.c` 每帧重置显示计时字段，捕获脚本保存 correctness CSV。未修改模型、RT30、光栅、采样器、PPA 后端、显示尺寸或任务 Core。

用户确认的采样配置为每场景预热 **5 帧**、采样 **50 帧**、**1 轮**，scale 1.0。Stage 0 依次测 STATIC、EYE_L_SWEEP、MULTI_AXIS，共 150 条正式计时帧。计时和 CRC 是两次独立固件运行。

## 环境与正确性

ESP 工程 commit `29c66ecfcc6abf14ffbd52748cc6fb4f2e34ad43`，PC Live2D commit `78fc634`。ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz，ESP-IDF v5.5.2，`-O2`，渲染任务 Core 0/优先级 6。模型 SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`，798660 字节，320×320，11 层、227 顶点、257 三角形、12 纹理、4 轴，`fast-nearest` 采样。完整元数据见 [metadata.txt](baseline_v2/metadata.txt)。

固定 15 姿态的 ARGB8888 和 RGB565 CRC 均已获取，15 次渲染和提交均成功；见 [correctness.csv](baseline_v2/correctness.csv)。正式计时 150 帧均无 frame drop、slot miss、lock skip 或 panic；deadline miss 为 EYE_L_SWEEP 2 帧、MULTI_AXIS 8 帧、STATIC 0 帧。实际阈值为从输入到提交返回超过 33333 µs。

收尾时额外验证了 `L2D_PROFILE_TIMING=n`、`L2D_PROFILE_CORRECTNESS=y` 的真正独立入口；[15 姿态复核](standalone_correctness/correctness.csv) 与本阶段 CRC 完全一致，无提交失败或 panic。此复核仅检查正确性，不计入正式 150 帧 benchmark。

## 测量结果

单位 µs。下表每格为 `平均 / P50 / P95 / P99`，完整 min/max 和所有分项见 [summary.csv](baseline_v2/summary.csv)，逐帧记录见 [frames.csv](baseline_v2/frames.csv)。

| 场景 | RT30 参数 | 层级/顶点 | 软件光栅 | PPA FILL | PPA SRM | Producer | 整帧 |
|---|---:|---:|---:|---:|---:|---:|---:|
| STATIC | 39/39/43/47 | 131/126/157/161 | 18701/18722/18815/18828 | 1084/1084/1093/1095 | 6486/6423/6709/6762 | 26747/26721/27021/27088 | 26983/26921/27315/27910 |
| EYE_L_SWEEP | 670/671/694/703 | 4701/4703/4722/4727 | 18611/18619/18770/18853 | 1565/1566/1588/1590 | 6519/6447/6711/6766 | 32417/32377/32769/32813 | 32666/32574/33110/34069 |
| MULTI_AXIS | 986/1000/1032/1067 | 4910/4913/4935/4938 | 18551/18575/18768/18806 | 1562/1569/1591/1596 | 6542/6483/6732/6761 | 32914/32895/33203/33280 | 33177/33159/33476/34402 |

`ppa_total_us=ppa_fill_us+ppa_srm_us`，两项是不同的阻塞调用。`clear_us` 包含 FILL 和 clear cache sync，`producer_total_us` 包含整个渲染和 SRM；这些嵌套字段不能相加估计整帧。此路径未调用独立的 convert cache sync，因此该字段为 0；flush cache sync 单独记录。当前同步单缓冲路径没有 producer slot 排队或 PPA blend，相关字段为 0。`serial.log` 是板端串口原始记录。

## 判定与后续

Phase 0 是观测改动，性能收益不适用。保持原渲染路径，将此组数据作为 Phase 1 前基线。动态场景层级/顶点约 4.7–4.9 ms，满足先做 RT30 无用弹性准备和 child stretch 投影优化的条件。50 帧、1 轮用于快速 A/B，P99 对偶发系统抖动敏感；本报告仅代表此模型和配置。
