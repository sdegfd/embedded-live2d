# Phase 3B：PPA SRM 局部 ROI A/B

## 范围和构建身份

基于 Phase 3A 提交 `49dce8999308a1cdd50de46a8e606b22ec0d8b78` 实施；两个 timing 固件由相同 Phase 3B 工作树构建，唯一有意切换的渲染开关是 `CONFIG_L2D_SRM_ROI=n/y`。串口 `esp_commit=49dce89…` 表示构建时 HEAD，**不**表示工作树干净；本报告与代码由 Phase 3B 提交一起交付。

板端：ESP32-P4 rev1.3、ESP-IDF v5.5.2、360 MHz CPU、200 MHz PSRAM、`-O2`、render task Core0/priority6、320×320、单 ARGB8888 与单 RGB565 持久缓冲、PPA FILL+SRM、完整面板提交。模型是 SD 卡上的 800,796 字节五轴版本，SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`；11 层、227 顶点、257 三角形、12 纹理，fast-nearest。源码中的 `project/esp.live` 用户替换件未由本轮修改或提交。

构建开关：`CONFIG_L2D_PROFILE_TIMING=y`、`CONFIG_L2D_PROFILE_STAGE=1`、`CONFIG_L2D_PROFILE_DETAIL=n`、`CONFIG_L2D_PROFILE_CORRECTNESS=n`；只切换 `CONFIG_L2D_SRM_ROI`。提交后的默认 `sdkconfig` 启用 ROI，但保留原 Stage 0；五场景 A/B 采集时临时设 Stage 1。5 个场景 STATIC、EYE_L_SWEEP、NECK_SWEEP、FACE_SWEEP、MULTI_AXIS，scale 1.0，各预热 **5 帧**、采样 **50 帧**、**1 轮**。正式 A/B 不是 100+1000×3；这是用户指定短测。原始数据在 [`phase31_srm_full`](phase31_srm_full/) 与 [`phase31_srm_roi`](phase31_srm_roi/) 的 `frames.csv`、`summary.csv`、`metadata.txt`、`serial.log`，ROI 组另有 `roi.csv`。`summary.csv` 对每项给出 avg/P50/P95/P99/min/max。

## 实现

每帧仍先完整 PPA FILL 清 320×320 ARGB，再完整软件 raster。只有 PPA SRM 的输入和输出块改为前一帧与当前帧屏幕空间几何 AABB 的并集：`in.pic=320×320`、`in.block=(x,y,w,h)`，`out.pic=320×320`、`out.offset=(x,y)`、`scale_x=scale_y=1`。RGB565 缓冲持续保存旧值；并集内旧人物位置随已清空的 ARGB 转为背景，外部像素保持不变。面板仍提交完整 320×320；清屏、raster、缓存同步策略和任务配置不变。PPA 参数已对照 ESP-IDF v5.5.2 `esp_driver_ppa/src/ppa_srm.c`：输出块尺寸由输入块与 scale 得出，ARGB8888/RGB565 无 YUV 的偶数坐标限制。

ROI 坐标取自实际 raster 使用的变换后顶点：`currentPosition * renderScale + x/y + renderOffset`，LEFTTOP 对齐；向外保守扩 2 px 并裁剪画布。第一帧、模型 load/reload、RT30 reset、renderScale 变化、origin 变化、画布尺寸变化、RGB565 buffer 更换、PPA backend/转换开关变化、几何非有限值或并集无效时强制完整 SRM。非 1:1 source block 保持原 full SRM 路径。`renderer_init` 重置 ROI history；帧循环没有动态分配。

额外常驻状态是 framework 内四个浮点 bounds/有效位，renderer 内三个小 ROI 结构及 history/revision 字段；timing ring 多 50×92=4,600 字节 ROI 采样字段，预先在 PSRAM 分配。没有新增帧缓冲。改动涉及 `live2d_engine` 的 revision/getter、`live2d_renderer` 的并集/SRM 参数、Kconfig 与 profile/correctness 输出，以及 CRC 比较脚本。`sys_display_flush` 未改，仍完整 RGB565 cache sync 和 panel submit。

## 板端耗时

下面均为微秒，`full → ROI`；各场景每组 50 帧。数值来自原始 `summary.csv`，不是旧四轴基线。

| 场景 | SRM avg | PPA total avg | Producer avg | Frame avg | Frame P95 | Frame P99/max | deadline miss |
|---|---:|---:|---:|---:|---:|---:|---:|
| STATIC | 6489 → 4407 | 7585 → 5442 | 26653 → 25148 | 27017 → 25380 | 27739 → 25615 | 27875 → 25653 | 0 → 0 |
| EYE_L_SWEEP | 6548 → 4392 | 7635 → 5495 | 28239 → 26670 | 28503 → 27000 | 28871 → 27828 | 29403 → 28137 | 0 → 0 |
| NECK_SWEEP | 6522 → 4493 | 7622 → 5543 | 28635 → 27166 | 29034 → 27387 | 29712 → 27861 | 29813 → 27975 | 0 → 0 |
| FACE_SWEEP | 6448 → 4471 | 7547 → 5533 | 28638 → 27228 | 28886 → 27450 | 29135 → 28017 | 29223 → 28188 | 0 → 0 |
| MULTI_AXIS | 6533 → 4346 | 7632 → 5419 | 28714 → 27112 | 28992 → 27333 | 29392 → 27817 | 30074 → 28085 | 0 → 0 |

MULTI_AXIS 完整分位数（微秒；avg/P50/P95/P99/min/max）：

| 指标 | Full SRM | ROI SRM |
|---|---|---|
| PPA SRM | 6533/6483/6723/6751/6422/6751 | 4346/4327/4927/5096/4023/5096 |
| PPA total | 7632/7573/7828/7842/7511/7842 | 5419/5398/5902/6057/5131/6057 |
| Producer | 28714/28689/28932/29062/28545/29062 | 27112/27139/27604/27866/26597/27866 |
| Frame total | 28992/28912/29392/30074/28768/30074 | 27333/27341/27817/28085/26806/28085 |

MULTI_AXIS 平均 SRM 节省 2.188 ms，PPA total 节省 2.214 ms，producer 节省 1.602 ms，整帧节省 1.660 ms。ROI 的几何收集位于 raster 内；两组 raster 平均为 18.413/18.929 ms，差约 +0.516 ms，故不能把 SRM 单项收益等同整帧收益。此为顺序进行的一轮短测，差额中也包含系统波动。

## ROI 面积与 SRM 的关系

MULTI_AXIS 并集平均 57,421 px、P95 58,464 px；占 102,400 px 画布平均 56.07%、P95 57.09%，宽高平均约 201.16×285.44，P95 203×289。ROI SRM 的平均时间是 full 的 66.5%，说明固定事务与行跨度成本仍在。50 帧中面积变化很窄，面积与 SRM 耗时的 Pearson 相关约 0.09；不据此推断线性缩放。其余场景面积和分位数见 ROI `summary.csv`，与 Phase 3A 诊断相符。
ROI timing 的 250 条测量帧 `current_valid` 全为 1，测量区间 `force_full` 全为 0；初始化与每次 RT30 reset 的强制 full 位于 5 帧预热段。

## 正确性与结论

独立 correctness 构建关闭 timing/detail，分别运行 full 与 ROI。保留原 85 姿态测试，另加静态重复 20 帧、NECK 0→29→0 的 0.5 步进 117 帧、EYE_L 同样 117 帧、五轴确定性 MULTI 120 帧，共 **459 帧**。包括 RT30 reset、模型 reload。两组模型 SHA 相同；[`compare_l2d_crc.py`](../../tools/compare_l2d_crc.py) 对姿态名、顺序、frame_id、ARGB8888 CRC、RGB565 CRC、render/submit 状态逐项对照，**459/459 完全一致**，全部提交成功。原始记录在 [`phase31_correctness_full`](phase31_correctness_full/) 与 [`phase31_correctness_roi`](phase31_correctness_roi/)；四次测试串口日志均无 panic/PPA SRM 失败。

**保留 ROI SRM。** 在当前五轴模型、320×320 和 PPA/PPA 同步链上，净帧收益约 1.66 ms，P95/P99 也下降且没有 33.33 ms deadline miss。已知边界：测试只有每场景 50 帧一轮；换模型、打开 debug 辅助绘制、改变 canvas/source scaling 或 display buffer 生命周期后，需重新验证 ROI 覆盖与 CRC。当前未缩小 FILL、raster、cache sync 或 panel submit；也未实现 frame reuse、dirty raster 或 scanline 增量。下一轮优先评估 **raster**：仍约 18.9 ms/帧，且 Phase 3A 显示约 98% 的 span 可提前判定纹理内；Partial FILL 的理论上限约 1.1 ms，Frame Reuse 涉及更大架构与时序语义。
