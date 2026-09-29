# Phase 3A：几何 ROI 与 fast-nearest 工作量诊断

## 测试身份和边界

- 代码起点：`cb7ea886eb4a67f36232fbc9924645c2db1e2ff0`。本报告数据由该提交加 Phase 3A 诊断工作树构建；串口 `esp_commit` 字段只记录构建时 HEAD，不表示工作树干净。
- ESP-IDF v5.5.2；ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz；编译 `-O2`；320×320 ARGB8888 → PPA SRM → RGB565 全帧面板提交。Render task Core0 / priority6。
- 板上 SD 模型 `/sdcard/esp.live`：800,796 字节、SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`，11 层、227 顶点、257 三角形、12 纹理、5 RT30 轴，采样器 fast-nearest。此 SHA 与工作区用户替换模型一致。此前四轴数据不可用作本模型 A/B。
- scale 1.0；STATIC、EYE_L_SWEEP、NECK_SWEEP、FACE_SWEEP、MULTI_AXIS；各场景预热 5 帧、采样 50 帧、1 轮。ROI 诊断和 raster detail 分别构建；detail 耗时不作正式 benchmark。
- 原始数据：[`phase30_roi_diag/roi.csv`](phase30_roi_diag/roi.csv)、[`summary.csv`](phase30_roi_diag/summary.csv)、[`work.csv`](phase30_roi_diag/work.csv)、[`detail.csv`](phase30_roi_diag/detail.csv)、两个 metadata 与串口日志。

## ROI 算法

在 `PX_LiveFrameworkRenderLayer` 将每个可见、有纹理三角形的 `currentPosition` 变换为实际提交给 raster 的 `PX_LiveRenderVertex.position` 时，同时取屏幕空间 AABB。变换使用同一组 `renderScale`、调用者传入的 x/y 原点和居中偏移；当前基线是 LEFTTOP 且 scale 1.0。ROI 使用 `floor(min)-2` 到 `ceil(max)+2` 的保守范围，右/下边界为 exclusive，裁剪到 320×320。无可见三角形标记无效；非有限坐标标记 unsafe。前帧、当前帧并集用于后续旧位置清除；第一帧强制 full。没有扫描 framebuffer，也没有从 covered pixels 反推 AABB。

Phase 3A 仍按原先全 320×320 PPA SRM 转换；`roi.csv` 只记录诊断。`dirty_x/y/w/h` 仍是完整清屏矩形，不是几何 ROI。

## 板端 ROI 结果

下表为并集 ROI，面积单位像素；括号内依次为 avg / P95 / max。全部 250 条测量帧有效，测量区间无 force-full。

| 场景 | 并集面积 | 画布比例 q10000 | 宽 | 高 |
|---|---:|---:|---:|---:|
| STATIC | 56715 / 56715 / 56715 | 5538 / 5538 / 5538 | 199 / 199 / 199 | 285 / 285 / 285 |
| EYE_L_SWEEP | 56715 / 56715 / 56715 | 5538 / 5538 / 5538 | 199 / 199 / 199 | 285 / 285 / 285 |
| NECK_SWEEP | 57094 / 58176 / 58176 | 5575 / 5681 / 5681 | 200.3 / 202 / 202 | 285 / 288 / 288 |
| FACE_SWEEP | 57497 / 57772 / 58058 | 5614 / 5641 / 5669 | 201.2 / 202 / 203 | 285.8 / 286 / 286 |
| MULTI_AXIS | 57421 / 58464 / 58667 | 5607 / 5709 / 5729 | 201.2 / 203 / 203 | 285.4 / 289 / 289 |

`summary.csv` 包含 current/union 面积、比例、宽高各自的 avg/P50/P95/P99/min/max。EYE_L 外框不变，因为眼部运动没有扩展到人物最外层几何。MULTI_AXIS P95 为 57.09%，低于计划的 70% 进入阈值，进入 Phase 3B 局部 SRM A/B。

## fast-nearest 工作量

`work.csv` 每场景 50 帧，逐帧统计三角形、scanline、span、span 像素、span 长度、纹理内/外采样、可提前判定全程在纹理内的 span、Alpha 0/255/mixed。全程在纹理内通过 16.16 起点、增量和末点计算，且要求末点不越出 int32，属于保守分类；没有修改实际逐像素 bounds check。`detail.csv` 为每场景最后一帧逐层工作量。

| 场景 | 平均 span/帧 | 平均 span 像素/帧 | 可判定全程纹理内 span | 平均越界采样/帧 |
|---|---:|---:|---:|---:|
| STATIC | 5331 | 52846 | 98.4% | 84 |
| EYE_L_SWEEP | 5270 | 52384 | 98.5% | 81 |
| NECK_SWEEP | 5337 | 52893 | 97.6% | 127 |
| FACE_SWEEP | 5309 | 52814 | 97.8% | 117 |
| MULTI_AXIS | 5195 | 52025 | 97.7% | 120 |

代码核对：fast-nearest 已在三角形 setup 计算 affine UV gradient，不再每 span 求 UV 除法。仍然每 span 计算 `s_fp_step/t_fp_step = affine_*_dx * texture_size * 65536`，共约 5.2k 次 span/帧。未来可独立实验把 fixed step 提到三角形 setup、全纹理内 span 快路径或扫描线边界递增；本轮均未实施。几何 ROI 计算在有诊断开关时增加浮点 min/max 工作，其 timing 不作为正式性能收益。
