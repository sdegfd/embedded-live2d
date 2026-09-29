# Phase 2.5B：图层旋转三角函数的精确缓存

保留。动态帧的 `hierarchy_vertex_us` 从约 4.84 ms 降到约 1.36 ms，整帧从约 33.02 ms 降到约 28.96 ms。250 个正式采样帧没有 deadline miss。ARGB8888 和 RGB565 CRC 与缓存前逐条相同，包括原来的 15 个固定姿态。

没有做整层世界变换缓存，也没有做 float trig 实验。诊断已经说明时间在重复的 sin/cos 里，不在父链遍历本身；精确缓存拿走了这部分时间。

## 1. Git commit

`3b128888b49a279d9103e6e9e14a3518c6d3b89d`（`perf(runtime): cache Live2D layer rotation trig`）。

正式 timing 的 `esp_commit` 就是这个提交。正确性对照在该提交落盘前、用同一份缓存源码采过一次，元数据仍印着前一个提交 `6c8d79e`；85 条 CRC 与缓存前逐条相同。timing 固件重新编译后元数据已是 `3b12888`。

## 2. Build configuration

- ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz，ESP-IDF v5.5.2，`-O2`
- Canvas 320×320，`fast-nearest`，PPA FILL + PPA SRM
- 渲染任务 Core 0 / priority 6
- 正式 timing：`L2D_PROFILE_TIMING=y`，`L2D_PROFILE_STAGE=1`，`L2D_PROFILE_VISUAL=n`，`L2D_PROFILE_FINE=n`，`L2D_PROFILE_CORRECTNESS=n`
- 预热 5，采样 50，1 轮，scale 1.0
- 场景：STATIC、EYE_L_SWEEP、NECK_SWEEP、FACE_SWEEP、MULTI_AXIS
- 默认提交里的 `sdkconfig` 仍是 Stage 0。Stage 1 只用于这次五场景测量，没有写回默认配置

## 3. 模型 SHA256

`786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`

## 4. 修改文件

- `PX_LiveFramework.h` / `PX_LiveFramework.c`：两套角度缓存，以及与 `PX_MatrixRotateZ` 相同乘加顺序的点旋转
- `PX_LiveRealtime.c`：`PX_LiveRealtimeResetAll()` 清掉两套 valid 标志
- `live2d_engine.c` / `live2d_engine.h`：读出本帧 hit/miss
- `profile_benchmark.cpp`：正式 timing 增加 `trig_cache_hit` / `trig_cache_miss`；正确性增加小数姿态、缓存历史、父子依赖、reset 和 reload
- `main.cpp`：只在正确性模式保留模型字节，供 reload

## 5. 原因

[2.5A 诊断](025-visual-transform-diagnostic.md) 里，每个动态帧：

- `GetLayerVisualTransform` 11 次，父链访问 40 次，唯一图层 11 个
- `PX_PointRotate` 80 次，但点旋转角度只有 1 或 2 个
- `PX_sind` 364 次，其中 320 次来自 `PX_MatrixRotateZ` 对同一角度各算两次 sin 和两次 cos
- `rel_currentRotationAngle` 在采样姿态里全是 0；会变的是局部旋转、缩放或平移

所以先减少原函数的执行次数，不改 `PX_sind()`。

## 6. 算法

角度变化时仍调用原来的 `PX_sin_angle()` 和 `PX_cos_angle()`。

点旋转不再调用 `PX_PointRotate()`。它用缓存的 sin/cos 填入和 `PX_MatrixRotateZ` 相同的矩阵，再调用原来的 `PX_PointMulMatrix()`：

```text
x' = x*c + y*(-s)
y' = x*s + y*c
z' = z
```

这和现有矩阵路径的运算顺序一致。顶点循环里已经存在的 `PX_LiveFrameworkRotatePointCached()` 减法公式没有改到这条路径上。

每个图层每帧仍对累计视觉转角调用一次原来的 sin/cos。那是 22 次 `PX_sind`，不在本次缓存键里。

## 7. Cache key

两套键，不能合并：

| 缓存 | 键 | 值 |
|---|---|---|
| 图层旋转 | `rel_cachedRotationAngle` | `rel_currentRotationSin/Cos` |
| 局部旋转 | `rel_cachedLocalRotationAngle` | `rel_currentLocalRotationSin/Cos` |

相等判断是 `==`，没有 epsilon。实时求值会先把新角度写进 `rel_currentRotationAngle`，所以键必须是单独保存的旧角度，不能拿“当前字段和传入参数”自己跟自己比。

## 8. Invalidation

- 当前角度和缓存角度不完全相等
- `rel_rotationTrigValid` 或 `rel_localRotationTrigValid` 为假
- `PX_LiveFrameworkReset()` 和 `PX_LiveRealtimeResetAll()` 把两个 valid 清掉
- 导入模型时 `PX_memset` 整层结构，valid 为 0

父节点角度变化不会错误地复用子节点的最终视觉变换。缓存的只是该层自己的 sin/cos。子节点仍用新的父 sin/cos 重新做矩阵乘法。

## 9. Memory overhead

每个运行时图层增加约 24 字节：两个 `px_bool`（本工程里是 `int`）和四个 `px_float`。11 层约 264 字节。这是运行时字段，不进 `.live` 文件。

## 10. Correctness

缓存前、缓存后各 85 条，ARGB 和 RGB565 CRC 全部相同，`render_ok=1`。原来的 15 条也与 [Phase 0](standalone_correctness/correctness.csv) 相同。

| 组 | 条数 | 结果 |
|---|---:|---|
| 原 15 个固定姿态 | 15 | 与 Phase 0 一致 |
| 四轴小数姿态 0、0.25、0.5、1、3.25、7.5、14.5、14.75、15.25、22.75、28.5、29 | 48 | 缓存前后一致 |
| NECK 的 AAA、ABA、ABCBA | 11 | 重复帧 CRC 相同，回到 A/B 时 CRC 相同 |
| 父变子不变、子变父不变，再回到原组合 | 5 | 重复组合 CRC 相同 |
| reset 后重放 14.5 | 3 | reset 后的静态 CRC 等于 STATIC，重放 CRC 等于 ALL_14_5 |
| load / render / reload / render | 3 | reload 后的静态 CRC 等于 STATIC，14.5 等于 ALL_14_5 |

原始记录：[缓存前](phase25_correctness_before/correctness.csv)、[缓存后](phase25_correctness_cache/correctness.csv)。

## 11–14. Before / After

单位 µs。Before 的 STATIC、EYE_L_SWEEP、MULTI_AXIS 来自 [最终短基线](final_baseline/summary.csv)。NECK_SWEEP 和 FACE_SWEEP 最终基线没有同条件记录，Before 用 [Phase 1 保留版](phase1_step1_timing/summary.csv)。After 是同一次 Stage 1 测量，[summary.csv](phase25_cache_timing/summary.csv)。都是预热 5、采样 50、1 轮、scale 1.0、PPA/PPA、fast-nearest。

| 场景 | hierarchy 前 → 后 | Producer 前 → 后 | 整帧 前 → 后 | deadline 前 → 后 |
|---|---:|---:|---:|---:|
| STATIC | 129 → 131 | 26733 → 26698 | 27086 → 27174 | 0 → 0 |
| EYE_L_SWEEP | 4633 → 1207 | 32270 → 28149 | 32515 → 28452 | 0.04 → 0 |
| NECK_SWEEP | 4848 → 1362 | 32839 → 28644 | 33120 → 29086 | 0.16 → 0 |
| FACE_SWEEP | 4620 → 1209 | 32781 → 28706 | 33028 → 29194 | 0.06 → 0 |
| MULTI_AXIS | 4835 → 1364 | 32771 → 28659 | 33016 → 28959 | 0.04 → 0 |

MULTI_AXIS 的层级/顶点少了 3471 µs。Producer 少了 4112 µs，整帧少了 4057 µs。多出来的几百微秒落在 PPA、RT30 和 raster 的运行波动里；raster 仍是 18452 µs，没有把光栅改动算进这次收益。

After 的 avg / P50 / P95 / P99 / min / max：

| 场景 | 字段 | avg | P50 | P95 | P99 | min | max |
|---|---|---:|---:|---:|---:|---:|---:|
| STATIC | hierarchy | 130.80 | 130 | 135 | 159 | 124 | 159 |
| STATIC | producer | 26698.46 | 26668 | 26865 | 26943 | 26505 | 26943 |
| STATIC | frame | 27174.38 | 27063 | 27907 | 28213 | 26726 | 28213 |
| EYE_L_SWEEP | hierarchy | 1207.10 | 1201 | 1238 | 1241 | 1190 | 1241 |
| EYE_L_SWEEP | producer | 28149.00 | 28140 | 28316 | 28395 | 27951 | 28395 |
| EYE_L_SWEEP | frame | 28451.82 | 28355 | 29099 | 29250 | 28167 | 29250 |
| NECK_SWEEP | hierarchy | 1361.58 | 1358 | 1395 | 1397 | 1242 | 1397 |
| NECK_SWEEP | producer | 28643.94 | 28608 | 28851 | 28924 | 28423 | 28924 |
| NECK_SWEEP | frame | 29086.36 | 29003 | 29784 | 29907 | 28640 | 29907 |
| FACE_SWEEP | hierarchy | 1208.80 | 1198 | 1231 | 1234 | 1191 | 1234 |
| FACE_SWEEP | producer | 28706.44 | 28703 | 28921 | 28951 | 28396 | 28951 |
| FACE_SWEEP | frame | 29194.36 | 28992 | 30014 | 30217 | 28601 | 30217 |
| MULTI_AXIS | hierarchy | 1364.16 | 1356 | 1395 | 1400 | 1265 | 1400 |
| MULTI_AXIS | producer | 28658.60 | 28664 | 29020 | 29030 | 28343 | 29030 |
| MULTI_AXIS | frame | 28959.16 | 28920 | 29337 | 30062 | 28548 | 30062 |

250 帧 `frame_id` 唯一，frame drop 0，deadline miss 0。MULTI_AXIS 最慢整帧 30062 µs，离 33333 µs 还有约 3.3 ms。平均整帧余量约 4.4 ms。

RT30、raster、PPA、display lock 的 After 平均值：

| 场景 | rt30 | raster | ppa_total | display_lock_wait |
|---|---:|---:|---:|---:|
| STATIC | 46 | 18584 | 7624 | 271 |
| EYE_L_SWEEP | 556 | 18476 | 7558 | 103 |
| NECK_SWEEP | 586 | 18717 | 7617 | 242 |
| FACE_SWEEP | 894 | 18620 | 7632 | 288 |
| MULTI_AXIS | 909 | 18452 | 7575 | 104 |

display lock 最大值仍约 0.8–1.1 ms，但这 250 帧没有再把它推过 33.333 ms。本轮没有改锁。

## 15. cache hit/miss

测量帧平均。STATIC 跳过物理更新，所以计数是 0，不是缓存失效。

| 场景 | hit | miss | 命中率 |
|---|---:|---:|---:|
| STATIC | 0 | 0 | 物理更新未运行 |
| EYE_L_SWEEP | 51 | 0 | 51/51 |
| NECK_SWEEP | 50 | 1 | 50/51 |
| FACE_SWEEP | 51 | 0 | 51/51 |
| MULTI_AXIS | 50 | 1 | 50/51 |

51 = 11 次图层旋转检查 + 40 次父链局部旋转检查。NECK 和 MULTI 的那 1 次 miss 是 neck 局部旋转每帧都变。其余角度完全相等，直接复用旧 sin/cos。

## 16. 是否保留

保留。可以单独 `git revert 3b12888` 回到只有诊断、没有这层缓存的版本。

## 17. 已知风险

- 累计视觉转角的 22 次 sin/cos 仍走原来的 double `PX_sind()`。
- 缓存键是角度的精确位模式。同一几何角如果写成不同的 float 位，会 miss，结果仍正确，只是少一次命中。
- 正式 timing 的 hit/miss 计数是每帧两次整数读取，相对 1.36 ms 可以忽略。
- Stage 1 的 sdkconfig 没有留在默认配置里。

## 18. 下一步

不要接着做世界变换缓存。父链重复访问还在，但精确缓存之后层级/顶点只剩约 1.36 ms，再做一整套 affine 推导换来的是几百微秒和 CRC 风险。

也不要在这一轮把 `PX_sind()` 换成 `sinf()`。剩下的三角函数次数已经很少，而 float 路径会变成数值近似，不能自动换成新的 CRC。

下一轮应重新看整帧，而不是回到旧的 Phase 3 清单机械执行。当前 MULTI_AXIS 的大头是 raster 约 18.5 ms 和 PPA 约 7.6 ms。静止帧已经在 27 ms 左右。Frame Reuse 主要帮助静止画面；多轴画面要继续省时间，优先看 PPA 处理区域和当前 fast-nearest 热路径。Display lock 仍只观察。
