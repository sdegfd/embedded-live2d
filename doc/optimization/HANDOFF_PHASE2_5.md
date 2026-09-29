# 交接：ESP32-P4 Live2D Phase 2.5

Phase 2.5 到此停止。没有继续做 Frame Reuse、Dirty Region、Raster、Mesh、PSD 或 Async PPA。

## 最终保留的代码

| 提交 | 内容 |
|---|---|
| `6c8d79e` | `CONFIG_L2D_PROFILE_VISUAL` 诊断。默认关闭，不进正式 timing |
| `3b12888` | 图层旋转和局部旋转的精确 sin/cos 缓存。点旋转仍用原来的矩阵乘加顺序 |

默认 `sdkconfig`：timing 开、Stage 0、PPA/PPA、visual/fine/correctness 关。正式短测仍是预热 5、采样 50、1 轮、scale 1.0。

模型 SHA256：`786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`。

硬件与路径没有改：ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz，ESP-IDF v5.5.2，`-O2`，320×320，fast-nearest，PPA FILL + PPA SRM，同步单 ARGB + 单 RGB565，渲染任务 Core 0 / priority 6。

## 回退的实验

没有新的已合入再回退的实验。

明确没做：

- 整层世界视觉变换缓存。诊断里父链重复访问占 72.5%，但时间几乎都在 sin/cos。缓存后层级/顶点约 1.36 ms，不够再做一套 affine 推导。
- float `sinf`/`cosf` 实验。剩下的 double 三角函数主要是每层一次累计转角，加上 neck 角度变化时的一次 miss。不值得用数值近似换 CRC。
- 全局修改 `PX_sind()` 或 `px_double`。
- 重新合入已回退的 child stretch 投影。
- 改 PPA、采样器、画布、模型、core affinity、显示锁。

## 最新正式 baseline

Stage 1，五场景各 50 帧，提交 `3b12888`。原始数据在 [phase25_cache_timing](phase25_cache_timing/)。

| 场景 | hierarchy | Producer | 整帧 | deadline miss |
|---|---:|---:|---:|---:|
| STATIC | 131 µs | 26.70 ms | 27.17 ms | 0/50 |
| EYE_L_SWEEP | 1.21 ms | 28.15 ms | 28.45 ms | 0/50 |
| NECK_SWEEP | 1.36 ms | 28.64 ms | 29.09 ms | 0/50 |
| FACE_SWEEP | 1.21 ms | 28.71 ms | 29.19 ms | 0/50 |
| MULTI_AXIS | 1.36 ms | 28.66 ms | 28.96 ms | 0/50 |

250 帧无 drop、无 panic。MULTI_AXIS 最慢整帧 30.06 ms。33.333 ms 预算下，平均余量约 4.4 ms，最慢帧余量约 3.3 ms。

## hierarchy / Producer / Frame

Before 使用 [最终短基线](final_baseline/summary.csv)；NECK 和 FACE 最终基线没有同条件数据，用 [Phase 1 保留版](phase1_step1_timing/summary.csv)。

| 场景 | hierarchy 前 → 后 | Producer 前 → 后 | 整帧 前 → 后 |
|---|---:|---:|---:|
| STATIC | 129 → 131 µs | 26.73 → 26.70 ms | 27.09 → 27.17 ms |
| EYE_L_SWEEP | 4.63 → 1.21 ms | 32.27 → 28.15 ms | 32.52 → 28.45 ms |
| NECK_SWEEP | 4.85 → 1.36 ms | 32.84 → 28.64 ms | 33.12 → 29.09 ms |
| FACE_SWEEP | 4.62 → 1.21 ms | 32.78 → 28.71 ms | 33.03 → 29.19 ms |
| MULTI_AXIS | 4.84 → 1.36 ms | 32.77 → 28.66 ms | 33.02 → 28.96 ms |

MULTI_AXIS 层级/顶点少 3.47 ms。这就是这次 Producer 和整帧下降的来源。raster 仍约 18.45 ms。

## Correctness

缓存前后 85 条 CRC 完全相同。原 15 姿态与 Phase 0 相同。新增了小数姿态、AAA/ABA/ABCBA、父子依赖、RT30 reset 和模型 reload。详见 [026](026-visual-transform-cache.md)。

## 缓存命中率

动态测量帧每帧检查 51 次（11 次图层旋转 + 40 次父链局部旋转）：

- EYE、FACE：51/51 命中
- NECK、MULTI：50/51 命中，1 次 miss 是每帧都在变的 neck 局部旋转
- STATIC：物理更新被跳过，计数为 0

## 剩余热点

按 MULTI_AXIS 这次正式平均值：

1. raster，约 18.45 ms
2. PPA FILL + SRM，约 7.58 ms
3. hierarchy/vertex，约 1.36 ms
4. RT30 求值，约 0.91 ms
5. display lock wait，平均约 0.10 ms，最大仍约 1.1 ms，本轮没有造成 deadline miss

静止帧整帧约 27.2 ms，已经低于 30 FPS 周期。多轴帧的余量主要还卡在 raster 和 PPA，不在视觉变换。

## 下一轮不要机械执行的旧顺序

旧计划把 Frame Reuse 放在最前。它能帮助静止画面，但 MULTI_AXIS 每帧姿态都在变，复用整帧帮不上这 28.96 ms。更值得先看的是：

- PPA FILL / SRM 的实际处理区域
- 当前 `fastNearestSampling` 里真正还在跑的光栅成本

Display lock 继续只记录，不改锁、不改核、不改 LVGL。

诊断细节在 [025](025-visual-transform-diagnostic.md)。缓存细节在 [026](026-visual-transform-cache.md)。
