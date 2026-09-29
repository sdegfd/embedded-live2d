# Phase 2.5A：视觉变换调用与复用诊断

本报告只记录调用次数、复用机会和诊断分段时间。它不改变变换数学，也不作为正式 timing 的 Before/After。分段微秒包含 `esp_timer_get_time()` 读数，不能和 [最终短基线](final_baseline/summary.csv) 的 4.84 ms 直接相减。

## 1. Git commit

诊断代码：`6c8d79e283b770437b1f1438a8d27e76029e05d6`（`perf(profile): measure visual transform reuse`）。

固件按该提交编译，并临时打开 `CONFIG_L2D_PROFILE_VISUAL=y`。默认 `sdkconfig` 仍关闭该开关，正式 timing 不包含这些计数器。元数据里的 `esp_commit` 是上面的完整 SHA。工作区当时还有未提交的 sdkconfig 开关，所以 IDF 版本字符串显示 `6c8d79e-dirty`；源码提交本身不含该开关。

## 2. Build configuration

- ESP32-P4 rev1.3，CPU 360 MHz，PSRAM 200 MHz，ESP-IDF v5.5.2，`-O2`
- Canvas 320×320，`fast-nearest`，PPA FILL + PPA SRM
- 渲染任务 Core 0 / priority 6
- `L2D_PROFILE_VISUAL=y`，`L2D_PROFILE_FINE=n`，`L2D_PROFILE_CORRECTNESS=n`，`L2D_PROFILE_DETAIL=n`
- 预热 5 帧，采样 50 帧，1 轮，scale 1.0
- 场景：STATIC、EYE_L_SWEEP、NECK_SWEEP、FACE_SWEEP、MULTI_AXIS
- 275 条逐帧记录，55 条图层快照，无 panic

## 3. 模型 SHA256

`786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`

11 层，227 顶点，257 三角形，12 张纹理，4 个 RT30 轴。加载 262462 µs，模型 798660 字节。

## 4. 修改文件

- `PX_Typedef.c` / `PX_Typedef.h`：诊断窗口内的 `PX_sin_angle`、`PX_cos_angle`、`PX_sind`、`PX_PointRotate` 计数
- `PX_LiveFramework.c` / `PX_LiveFramework.h`：父链深度、角度唯一值、帧间变化和四段计时
- `PX_LiveRealtime.c`：RT30 reset 时丢掉诊断用的上一帧快照
- `profile_benchmark.cpp`、`main.cpp`、`Kconfig.projbuild`、`tools/capture_l2d_profile.py`

开关关闭时，上述计数和计时不参与编译。

## 5. 原因

Phase 1 细分诊断把动态 `hierarchy/vertex` 的主要比例标在 `PX_LiveFramework_GetLayerVisualTransform()`。这一轮先数清调用和复用，再决定是否缓存。`PX_sind()` 仍是 double 泰勒展开，本轮没有改它。

## 6. 调用链（已对照源码）

每个会更新物理的帧：

1. REALTIME30 对 11 个图层调用 `PX_LiveFrameworkSetLayerCurrentRotationAngle()`，各算一次 `PX_sin_angle` 和 `PX_cos_angle`。`rel_currentRotationSin/Cos` 已有缓存，但视觉变换没有使用它。
2. 每个图层调用一次 `GetLayerVisualTransform()`。沿父链每走一层做两次 `PX_PointRotate`：局部平移乘 `rel_currentRotationAngle`，相对点乘 `rel_currentLocalRotationAngle`。
3. `PX_PointRotate` → `PX_MatrixRotateZ`。后者对同一个角各调用两次 `PX_sin_angle` 和 `PX_cos_angle`。
4. 每个图层再对累计视觉转角各调用一次 `PX_sin_angle` 和 `PX_cos_angle`。顶点循环使用这对值，不再逐顶点求三角函数。
5. `PX_LiveFramework_GetLayerVisualKeyPoint()` 没有调用者。

`PX_cos_angle` 经 `PX_cos_radian` 进入一次 `PX_sind`。因此每个 `PX_sin_angle` 或 `PX_cos_angle` 对应一次 `PX_sind`。

姿态 revision 不变且没有弹性顶点时，整段物理更新被跳过。STATIC 只有 reset 后的第一帧会跑上面的链。

## 7. Cache key

本提交没有运行时缓存。观察到的可复用键是两个不能合并的角度：

- `rel_currentRotationAngle`
- `rel_currentLocalRotationAngle`

比较必须用完全相等，不能用 epsilon。

## 8. Invalidation 条件

诊断用的上一帧快照在 `PX_LiveFrameworkReset()` 和 `PX_LiveRealtimeResetAll()` 时失效。这只影响“相对上一帧是否变化”的计数，不改变渲染。

## 9. Memory overhead

诊断固件在 BSS 中保存最多 256 层的上一帧快照，以及最多 1024 个点旋转角度样本。默认 timing 固件不包含这些数组。

## 10. Correctness

本提交不改数学。正确性仍以 Phase 0 的 15 个固定姿态 CRC 为准；扩展姿态对照在缓存实现前另采。

## 11–14. 正式 Before / After

没有。正式短基线仍是 [final_baseline/summary.csv](final_baseline/summary.csv)：MULTI_AXIS `hierarchy_vertex_us` 平均约 4.84 ms，Producer 约 32.77 ms，整帧约 33.02 ms。下表是诊断固件自己的分段，不是那组数字的替代。

动态测量帧（50/50 都执行了物理更新）的一次代表帧，五场景结构相同：

| 项目 | 数值 |
|---|---:|
| `GetLayerVisualTransform` | 11 |
| 父链访问 / 唯一图层 | 40 / 11 |
| 平均深度 / 最大深度 | 3.64 / 5 |
| 父链重复访问比例 | (40−11)/40 = 0.725 |
| `PX_PointRotate` | 80 = 40×2 |
| `PX_sin_angle` / `PX_cos_angle` / `PX_sind` | 182 / 182 / 364 |
| 其中 transform / final / set-rotation / other | 320 / 22 / 22 / 0 |

320 = 80×4，和 `PX_MatrixRotateZ` 对同一角度各算两次 sin、两次 cos 一致。22 = 11×2，分别对应累计转角和图层旋转缓存刷新。`other` 为 0，这一帧的 `PX_sind` 没有第四个来源。

角度唯一值：

| 场景 | 唯一 `rel_currentRotationAngle` | 唯一 `rel_currentLocalRotationAngle` | 点旋转角度唯一值 | 点旋转角度复用 |
|---|---:|---:|---:|---:|
| STATIC 首帧、EYE、FACE | 1 | 1 | 1 | 79/80 = 0.988 |
| NECK、MULTI | 1 | 2 | 2 | 78/80 = 0.975 |

采样到的 `rel_currentRotationAngle` 全部是 `0`。图层旋转缓存因此可以跨帧命中，但当前视觉变换路径仍每次重建矩阵。

父链（深度含自身）：

| 图层 | 父 | 深度 |
|---|---|---:|
| cloth | 无 | 1 |
| neck | cloth | 2 |
| face | neck | 3 |
| mouth、left_eye、right_eye、left_eyeup、right_eyeup、front_hair | face | 4 |
| back_hair、hairwear | front_hair | 5 |

深度和为 1+2+3+6×4+2×5 = 40。

帧间变化（测量帧平均值，完全相等才算变化）：

| 场景 | 旋转角 | 局部旋转 | 局部缩放 | 局部平移 | 层级平移 | 关键点 | 祖先视觉输入 |
|---|---:|---:|---:|---:|---:|---:|---:|
| EYE_L_SWEEP | 0 | 0 | 1 | 1.98 | 0 | 0 | 0 |
| NECK_SWEEP | 0 | 1 | 0 | 0 | 0 | 0 | 9 |
| FACE_SWEEP | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| MULTI_AXIS | 0 | 1 | 2 | 3 | 0 | 0 | 9 |

FACE_SWEEP 的 50 个测量帧里，上述字段的变化计数最大值也是 0。脸轴仍让 mesh revision 变化，所以视觉变换照样完整重算，只是输入和上一帧相同。NECK 只改 neck 的局部旋转；9 个子孙的祖先视觉状态跟着变，关键点不变。EYE 改的是缩放和局部平移，不改角度。

诊断分段，MULTI_AXIS 测量帧平均（含计时器读数，不是正式 benchmark）：

| 段 | 平均 µs |
|---|---:|
| 父链上除两次旋转以外的运算 | 162 |
| 局部平移 × 图层旋转 | 3173 |
| 相对点 × 局部旋转 | 3379 |
| 累计转角 sin/cos | 548 |
| 诊断固件的整段 physical | 8384 |

两次 `PX_PointRotate` 合计约占这次诊断 physical 的 78%。父链遍历本身不是热点。正式基线的 4.84 ms 不能按这个比例直接扣减，但复用方向已经明确：先去掉重复的原函数 sin/cos，而不是先改泰勒级数或做整层世界矩阵。

STATIC 测量帧 50/50 跳过物理更新，调用计数为 0，`diag_physical_us` 平均约 130 µs，只剩 revision 判断。reset 后的第一帧仍然是 11 / 80 / 364 那组调用。

原始记录：[visual.csv](phase25_visual_diag/visual.csv)、[visual_pose.csv](phase25_visual_diag/visual_pose.csv)、[metadata.txt](phase25_visual_diag/metadata.txt)、[serial.log](phase25_visual_diag/serial.log)。

## 15. cache hit/miss

没有运行时缓存，没有 hit/miss。点旋转角度复用比例见上表。

## 16. 是否保留

保留诊断开关，默认关闭。不把这次诊断固件当作正式基线。

## 17. 已知风险

- 诊断绝对时间包含每个祖先步骤上的多次计时器读取。
- 角度唯一值来自图层字段和当帧实际传入 `PX_PointRotate` 的角度。累计视觉转角另有每层一次 sin/cos，已计入 `sind_final`。
- FACE 轴不改变这些图层视觉字段，但仍然重算视觉变换。只缓存角度不能表达“整组视觉输入没变”；那是后续才考虑的事情。

## 18. 下一步

先做精确三角函数缓存：角度完全相等时复用原 `PX_sin_angle` / `PX_cos_angle` 的结果，两套角度分开存储。点旋转若要保持 CRC，应沿用 `PX_MatrixRotateZ` 加 `PX_PointMulMatrix` 的运算顺序，而不是改成另一条加减公式。父链重复组合是真的（72.5% 的访问是重复图层），但诊断时间几乎都在两次旋转里，所以在精确缓存的正式 timing 出来之前不做整层世界变换缓存，也不改全局 `PX_sind()`。
