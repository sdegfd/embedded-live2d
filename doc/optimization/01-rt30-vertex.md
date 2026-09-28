# Phase 1：RT30 / 顶点路径实验

## 目标、改动和判定

目标是降低动态姿态的 `hierarchy_vertex_us`，同时维持原模型、scale 1.0、采样器和显示路径。所有场景每次预热 5 帧、采样 50 帧、1 轮。改动前采用 [Phase 0 Baseline v2](00-profile-v2.md)；它包含 STATIC、EYE_L_SWEEP、MULTI_AXIS。NECK_SWEEP 和 FACE_SWEEP 没有同条件的 Phase 0 对照，只用于本阶段内部比较。

1. `8941d63`：REALTIME30 后续强制 `k=0`，故跳过不被读取的 `keyDirection` 子节点方向求和及归一化。Timeline 保持原计算。15 个固定姿态的 ARGB8888/RGB565 CRC 与 Phase 0 全部相同；见 [step1 correctness](phase1_step1_correctness/correctness.csv)。保留。
2. `dcd811d`：尝试每层预计算 child stretch 的单位方向，把顶点循环改为投影。五个场景的层级/顶点平均耗时均增加 5–27 µs，MULTI_AXIS Producer 平均增加约 61 µs；见 [step2 timing](phase1_step2_timing/summary.csv)。当前模型未获得收益，`7c15037` 已回退。因未进入最终版本，没有把该次实验的耗时当正确性验证；最终引擎文件与已通过 CRC 的 `8941d63` 完全相同。

## 耗时数据

单位 µs。每格为平均值；主要字段的 P50/P95/P99 见下表及 [step1 summary](phase1_step1_timing/summary.csv)，全部逐帧记录见 [step1 frames](phase1_step1_timing/frames.csv)。

| 场景 | 层级/顶点：前→保留版→投影实验 | Producer：前→保留版→投影实验 | 整帧：前→保留版→投影实验 |
|---|---:|---:|---:|
| STATIC | 131→129→133 | 26747→26744→26740 | 26983→27038→27040 |
| EYE_L_SWEEP | 4701→4636→4661 | 32417→32331→32397 | 32666→32585→32655 |
| NECK_SWEEP | —→4848→4874 | —→32839→32859 | —→33120→33103 |
| FACE_SWEEP | —→4620→4647 | —→32781→32835 | —→33028→33096 |
| MULTI_AXIS | 4910→4840→4865 | 32914→32815→32877 | 33177→33085→33138 |

| 场景/字段 | 前 P50/P95/P99 | 保留版 P50/P95/P99 | 平均收益 |
|---|---:|---:|---:|
| EYE_L_SWEEP 层级/顶点 | 4703/4722/4727 | 4642/4657/4660 | 1.40% |
| EYE_L_SWEEP Producer | 32377/32769/32813 | 32288/32624/32661 | 0.27% |
| EYE_L_SWEEP 整帧 | 32574/33110/34069 | 32492/32874/33793 | 0.25% |
| MULTI_AXIS 层级/顶点 | 4913/4935/4938 | 4845/4859/4862 | 1.43% |
| MULTI_AXIS Producer | 32895/33203/33280 | 32788/33224/33355 | 0.30% |
| MULTI_AXIS 整帧 | 33159/33476/34402 | 32997/33620/34163 | 0.28% |

## 正确性、环境与限制

两次 timing 分别取得 250 帧，未见 panic；[step1 serial](phase1_step1_timing/serial.log) 和 [step2 serial](phase1_step2_timing/serial.log) 保留原始输出。模型 SHA256、CPU/PSRAM、ESP-IDF、任务 Core、显示配置与 Phase 0 相同，元数据分别见 [step1 metadata](phase1_step1_timing/metadata.txt) 与 [step2 metadata](phase1_step2_timing/metadata.txt)。第一项只省下约 0.07 ms，远未把动态层级/顶点从约 4.9 ms 降到 2–3 ms；不应据此承诺更高 FPS。第二项在其他大量 stretch 的模型上可能不同，但当前基线明确不保留。下一步需细分 keypoint、visual transform、stretch、vertex transform、UV update，定位剩余约 4.8 ms，之后才考虑更大的 transform cache。
