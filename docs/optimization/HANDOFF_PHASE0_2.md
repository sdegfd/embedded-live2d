# 交接：ESP32-P4 Live2D Phase 0–2

## 工作区与最终基线

- 统一仓库：`/home/ubuntu/esp/project/live2d_repo`，远端私有仓库 `git@github.com:sdegfd/live2d.git`，分支 `main`。
- 板端工程：`firmware/esp32p4/live2d_rt30_baseline/`；PC 引擎：`PainterEngine/`；当前 Windows 编辑器：`project/PainterEngine_LiveEditor/`。不要用旧副本 `PainterEngine_LiveEditor_source/` 覆盖当前编辑器。
- 模型由使用者拷入 SD 卡根目录，文件名 `/sdcard/esp.live`。板端测试只读模型，不把 CSV 或模型写入 SD；PC 通过串口捕获。
- 默认 `sdkconfig`：`L2D_PROFILE_TIMING=y`、`L2D_PROFILE_STAGE=0`、`L2D_PROFILE_CORRECTNESS=n`、`L2D_PROFILE_FINE=n`、`L2D_CLEAR_CPU=n`、`L2D_CONVERT_CPU=n`。后端保留 PPA clear + PPA convert。
- ESP-IDF v5.5.2，ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz，320×320，`fast-nearest`，模型 SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`。渲染任务 Core 0/优先级 6；记录的是任务配置，不代表整机只使用一个核心。
- 最终板端固件所含源码 commit `369939e59a092dee54cdc63fc339af4f7e83adf8`；后续文档提交不会改变该固件。串口日志显示连续渲染窗口 CPU0=87%/82%、CPU1=0%，这是当时固件统计口径下的两个 1 秒窗口，不应外推为所有负载的 CPU 占用。

## **正式测试帧数：每场景预热 5 帧、采样 50 帧、1 轮**

这是使用者最终指定的短基线，scale 1.0。不要恢复方案初稿的 100 帧预热、1000 帧采样、3 轮，也不要按 30 秒总时长重新设计。场景与正式采样行数如下：

| 模式 | 场景 | 正式帧数 |
|---|---|---:|
| Stage 0 默认基线 | STATIC、EYE_L_SWEEP、MULTI_AXIS | 3×50=150 |
| Stage 1 RT30/顶点 | STATIC、EYE_L_SWEEP、NECK_SWEEP、FACE_SWEEP、MULTI_AXIS | 5×50=250 |
| Stage 2 后端 A/B | 每种 A/B/C/D 后端各测 STATIC、MULTI_AXIS | 4×2×50=400 |

`L2D_PROFILE_CORRECTNESS` 是独立模式，每个后端测试 15 个固定姿态的 ARGB8888 和 RGB565 CRC，**不属于正式计时**。`L2D_PROFILE_FINE` 在顶点循环加计时，仅用于工作量定位，也按 5+50 帧采集，**不能把它的绝对耗时当 benchmark**。热路径不打印逐帧记录；固定 ring 在每组结束后串口输出，PC 端保存 `frames.csv`、`summary.csv`、`metadata.txt`、`serial.log`。

## 已完成与证据

| 阶段 | 结论 | 记录 |
|---|---|---|
| Phase 0 | 拆分 PPA、cache、flush、整帧计时，另设固定姿态 CRC。150 帧无丢帧或 panic。 | [报告](00-profile-v2.md)、[原始数据](baseline_v2/frames.csv) |
| Phase 1 | 保留 REALTIME30 跳过无用 `keyDirection` 准备；15 姿态 CRC 与 Phase 0 一致。child stretch 预计算在此模型反而变慢，已回退。诊断发现动态物理时间约 70%–72% 集中在 visual transform；尚未重构 transform cache。 | [报告](01-rt30-vertex.md)、[细分数据](phase1_fine/summary.csv) |
| Phase 2 | 四组合 CRC 全一致；PPA/PPA 在 Producer 与整帧均最快，保留原后端。 | [报告](02-ppa-ab.md)、[A/B 数据](phase2_ab_timing/summary.csv) |

最终默认固件已重新编译刷入真机，并取得 [Stage 0 最终 150 帧](final_baseline/frames.csv)：STATIC、EYE_L_SWEEP、MULTI_AXIS 各 50 帧；150 个 `frame_id` 均唯一，0 frame drop、0 lock skip、4 deadline miss，无 panic。MULTI_AXIS Producer 平均 32.77 ms、整帧平均 33.02 ms，详见 [最终 summary](final_baseline/summary.csv) 和 [metadata](final_baseline/metadata.txt)。测试完成后继续记录到两个实时窗口，FPS 30.14/29.53、`batch_fail=0`；见 [continuous.log](final_baseline/continuous.log)。另以 timing 关闭、correctness 单独开启的固件复核 [15 姿态 CRC](standalone_correctness/correctness.csv)，全部匹配 Phase 0。

可追溯提交包括 Phase 0 `29c66ec`/`54436b0`，Phase 1 `8941d63`、实验 `dcd811d` 与回退 `7c15037`、细分诊断 `2cbe719`/`085f49b`，Phase 2 `fdc482e`/`e09faff`；`369939e` 修复独立 correctness 入口及后端日志。所有正式和诊断记录都在 `doc/optimization/`。PC 源码 commit 由元数据标记为 `78fc634`；本轮没有改 Windows 编辑器或 PC 渲染算法。

## Windows 复现

在已配置 ESP-IDF 5.5.2 的终端，从仓库根目录运行：

```bat
cd firmware\esp32p4\live2d_rt30_baseline
idf.py build
idf.py -p COM5 flash
```

另开终端从仓库根目录捕获；把 `COM5` 换成真实串口：

```bat
python tools\capture_l2d_profile.py --port COM5 --out doc\optimization\next_run
```

串口打开会重启板子；脚本捕获到 `L2D_DONE` 后退出。profile suite 结束后固件继续运行实时 RT30 连续帧。切换 correctness 或 fine 模式时修改项目 `sdkconfig`/menuconfig，重新编译刷机；正式比较必须恢复默认 timing、Stage 0、PPA/PPA 配置。串口单次记录会报告实际 `frame_id`、dirty ROI、丢帧和 deadline miss 等字段。

## 下一位 AI 的起点

先读三份阶段报告和 `doc/optimization/final_baseline/`，确认模型 SHA256、后端和 5+50 帧条件一致，再同步 PC 侧的新算法代码。下一处值得研究的是 `PX_LiveFramework_GetLayerVisualTransform()` 的逐层祖先链旋转与复用；先测函数调用、三角函数次数与固定姿态 CRC，再考虑局部缓存。不要将本轮被回退的 child stretch 投影实验重新并入基线，也不要在没有单独 A/B 数据时改变画质、scale 或模型。
