# Phase 3B 交接：ESP32-P4 Live2D ROI SRM

## 当前状态

- Phase 3A 独立提交：`49dce8999308a1cdd50de46a8e606b22ec0d8b78`。Phase 3B 代码、CSV 和本交接在下一独立提交；完成后停止，不继续 Phase 3C/Dirty Raster/Frame Reuse。
- 工程：`firmware/esp32p4/live2d_rt30_baseline`。只有 `CONFIG_L2D_SRM_ROI=y` 时启用局部 PPA SRM；关闭即可回到 full SRM。PPA FILL、软件 raster、缓存同步、RGB565 全帧面板提交均未改。
- 当前板上模型 SHA256 `1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100`，800,796 字节，5 轴。工作区 `project/esp.live` 是用户替换件，本轮没有修改或纳入提交；复测必须先核对 SD 卡 `/sdcard/esp.live` SHA。
- 板端测试：ESP-IDF v5.5.2、ESP32-P4 rev1.3、CPU 360 MHz、PSRAM 200 MHz、`-O2`、320×320、render task Core0/priority6、fast-nearest。正式 timing 固定每场景 **预热 5 帧、采样 50 帧、1 轮、scale 1.0、33.33 ms 截止**。detail/CRC 是独立构建，不能拿其耗时当 benchmark。
- 交付时板子已恢复 ROI timing 固件（detail/correctness 关闭）；再次串口检查 250 帧，drop 0、deadline miss 0、无 panic/PPA 错误。

## 必答结果（MULTI_AXIS，50 帧短测）

| 问题 | 板端结果 |
|---|---|
| 平均 union ROI | 57,421 px，约 201.16×285.44 |
| P95 union ROI | 58,464 px；宽高各自 P95 为 203、289（这两个分位数未必来自同一帧） |
| 占 320×320 比例 | 平均 56.07%，P95 57.09% |
| PPA SRM | 平均 6.533 → 4.346 ms，省 2.188 ms |
| PPA total | 平均 7.632 → 5.419 ms，省 2.214 ms |
| Producer | 平均 28.714 → 27.112 ms，省 1.602 ms |
| Frame total | 平均 28.992 → 27.333 ms，省 1.660 ms |
| 尾延迟 | Frame P95 29.392 → 27.817 ms；P99/max 30.074 → 28.085 ms，均改善 |
| Deadline miss | full 0/50，ROI 0/50 |
| 动态序列 CRC | 原 85 + 新 374 = 459 帧；ARGB8888 与 RGB565 两路 CRC 全部逐帧一致，submit 全成功 |
| 是否保留 | 保留 `CONFIG_L2D_SRM_ROI=y`；当前模型收益和正确性均成立 |
| 下一步候选 | 优先 Raster（ROI 后约 18.9 ms/帧）；Partial FILL 约 1.1 ms 上限；Frame Reuse 涉及更广行为变化，留待评审 |

## 文件与复测

- 诊断说明：[`030-roi-raster-diagnostic.md`](030-roi-raster-diagnostic.md)，原始 [`phase30_roi_diag`](phase30_roi_diag/)。
- ROI A/B：[`031-ppa-srm-roi.md`](031-ppa-srm-roi.md)，原始 [`phase31_srm_full`](phase31_srm_full/) 与 [`phase31_srm_roi`](phase31_srm_roi/)。各目录有 `frames.csv`、`summary.csv`、`metadata.txt`、`serial.log`；ROI 另有 `roi.csv`。
- 正确性：[`phase31_correctness_full`](phase31_correctness_full/) 与 [`phase31_correctness_roi`](phase31_correctness_roi/)，运行 `python tools/compare_l2d_crc.py <full/correctness.csv> <roi/correctness.csv>` 必须 PASS 459 帧。
- 串口捕获：`python tools/capture_l2d_profile.py --port COMx --timeout 120 --out <目录>`，Windows 需先安装 pyserial。板端只读取 SD 模型，不写 CSV 或导出模型。
- 已提交的 `sdkconfig` 默认 `CONFIG_L2D_SRM_ROI=y`，保留原本 Stage 0；本报告五场景测量时临时设 `CONFIG_L2D_PROFILE_STAGE=1`，正确性构建临时设 `CONFIG_L2D_PROFILE_CORRECTNESS=y`。测试用 stage/correctness 差异不提交。正式/日常固件关闭 `CONFIG_L2D_PROFILE_DETAIL` 与 correctness。detail 固件逐像素计数和串口输出会明显影响流畅度，不应用其画面判断正式性能。

## 风险与边界

ROI collector 在实际三角形顶点变换处采集 AABB，加 2 px padding；强制 full 的条件与 1:1 PPA 保护见 Phase 3B 报告。当前仅检验这版 5 轴模型和 320×320 路径。改变模型、scale、调试辅助线、图层可见性语义或输出链后，先跑逐帧 RGB565 CRC，再看短测耗时。一次 50 帧采样不能代替长时间温度/负载稳定性测试。
