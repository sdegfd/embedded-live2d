# ESP32-P4 Live2D 性能优化实施计划

## 0. 目标与基线

本计划用于 `sdegfd/live2d` 项目的分阶段性能优化，核心原则是：

- 不一次性修改模型、运行引擎、PPA、缓存和显示链。
- 每个阶段独立提交、独立测试、独立记录收益。
- 所有性能提升都必须能明确归因。
- 不允许通过偷偷降低模型显示尺寸、降低画质、取消 RT30 动作等方式伪装成性能优化。
- 所有画质或行为变化必须明确标记为 trade-off。
- 上一阶段没有完成正确性验证和 A/B 数据，不进入下一阶段的叠加实现。

2026-09-29 内存前提：不再按「只有内部 RAM、没有 PSRAM」设计。本阶段默认目标带 PSRAM，至少 8 MB。当前 P4 实测仍是 32 MB / 200 MHz；8 MB 是下限，不是新的分配上限。允许用空间换时间，但必须有实测，没有毫秒级收益就撤回。稳态 update/render 仍然不在帧内调用分配器。这条只覆盖本阶段，不改写下面历史基线里的四轴数字。现行五轴计时以 `docs/benchmarks/reorg-final.md` 为准。

同日已保留的平台改动：P4 L2 从 128 KB 调到 256 KB，内部 DMA reserve 从 256 KB 降到 128 KB，否则 256 KB 缓存无法启动。对照实验表明单独降低 reserve 不改变光栅时间。MULTI_AXIS 光栅约 19.24 ms 降到 17.96 ms，帧约 27.90 ms 降到 26.70 ms，p95 约 27.35 ms，deadline miss 仍为 0。细节见 `docs/optimization/041-l2-cache-256.md`。字面 span 试验无收益，已撤回，见 `docs/optimization/040-raster-word-span.md`。

当前冻结基线：

- 芯片：ESP32-P4 rev1.3
- CPU：360 MHz
- PSRAM：32 MiB，200 MHz
- ESP-IDF：v5.5.2
- 编译优化：`-O2`
- 模型：320×320
- Layer：11
- Vertex：227
- Triangle：257
- Texture：12
- RT30 Axis：4
- Sampler：`fast-nearest`
- 当前 MULTI_AXIS 典型平均值：
  - Producer total：约 32.38 ms
  - Software raster：约 18.21 ms
  - RT30 pose：约 0.99 ms
  - Hierarchy / vertex：约 4.91 ms
  - PPA FILL + SRM：约 7.93 ms

当前优化工作统一分为三层：

- **M：Model / Editor**
  - 模型结构、PSD、自动 Mesh、透明轮廓、Overdraw 等。
- **R：Runtime / Renderer**
  - RT30、Hierarchy、Raster、Dirty Render、Frame Reuse 等。
- **P：Platform / ESP32-P4**
  - PPA、PSRAM、Cache、显示提交、Core Affinity、异步流水线等。

---

# Phase 0：完善 Profiling，建立 Baseline v2

> 本阶段只完善测量，不做性能优化。

## 0.1 拆分 PPA 时间

当前：

```text
ppa_wait_us
=
PPA FILL
+
PPA SRM
```

修改为：

```text
ppa_fill_us
ppa_srm_us
ppa_total_us
```

其中：

```text
ppa_total_us = ppa_fill_us + ppa_srm_us
```

必须确保三个值的定义互不重叠。

---

## 0.2 拆分 Cache Sync

将现有统一的：

```text
cache_sync_us
```

细分为：

```text
clear_cache_sync_us
convert_cache_sync_us
flush_cache_sync_us
cache_sync_total_us
```

---

## 0.3 补完整显示路径时间

将已有但未完整输出的字段加入 CSV：

```text
display_lock_wait_us
panel_submit_us
flush_total_us
frame_total_us
```

其中：

```text
frame_total_us
=
从本帧参数输入开始
到 sys_display_flush_submit() 返回
```

这样才能完整解释 deadline miss。

---

## 0.4 修正 Checksum 测试设计

当前 benchmark 的 CRC 是：

```text
warmup
↓
计算一次 CRC
↓
随后多帧写入同一个 CRC
```

所以移动场景不能用该字段判断逐帧正确性。

修改为两套模式：

### Timing 模式

- 不在正式热路径逐帧计算 CRC。
- 仅记录性能数据。

### Correctness 模式

固定姿态测试：

```text
STATIC
ALL_14_5
ALL_29

left_eye = 0 / 15 / 29
right_eye = 0 / 15 / 29
neck = 1 / 15 / 29
face = 5 / 15 / 25
```

记录：

```text
ARGB8888 CRC32
RGB565 CRC32
```

---

## 0.5 Baseline v2 测试集

先只跑：

```text
STATIC
EYE_L_SWEEP
MULTI_AXIS
```

配置：

```text
warmup = 5
measure = 50
rounds = 1
```

输出目录：

```text
doc/optimization/baseline_v2/
    metadata.txt
    frames.csv
    summary.csv
    correctness.csv
    serial.log
```

---

## Phase 0 禁止修改

禁止改动：

```text
模型
RT30 算法
Raster 算法
PPA 策略
任务 Core
Sampler
缓存位置
显示尺寸
```

---

# Phase 1：优化 RT30 / Hierarchy / Vertex

当前动态多轴时：

```text
RT30              ≈ 0.99 ms
Hierarchy/Vertex  ≈ 4.91 ms
Raster            ≈ 18.21 ms
```

STATIC 的 Hierarchy / Vertex 只有约 0.124 ms，因此动态更新带来的约 4.8 ms 增量值得重点优化。

---

## 1.1 跳过 RT30 无用 Elasticity 准备

当前 `PX_LiveFramework_UpdateLayerVertices()` 会提前计算：

```text
keyDirection
child direction normalization
```

但 REALTIME30 模式后续会：

```c
k = 0;
```

禁用弹性物理。

修改为：

```text
Timeline / 实际需要 elasticity
    计算 keyDirection
else
    跳过相关准备
```

要求：

- Timeline 行为完全不变。
- RT30 固定姿态 CRC 必须一致。

---

## 1.2 优化 Child Stretch

当前每个：

```text
vertex × child
```

都会重复执行：

```text
v1 = childKey - parentKey
v2 = vertex

dot(v1, v2)
|v1|
|v2|
normalize(v1)
```

利用：

\[
\cos\theta |v_2|
=
\frac{v_1 \cdot v_2}{|v_1|}
=
u_1 \cdot v_2
\]

优化为：

### 每 Layer / Child 一次

```text
u = normalize(childKey - parentKey)
stretchDelta = stretch - 1
```

### 每 Vertex

```text
projection = dot(u, vertexRelative)

if projection > 0:
    vertex += u * projection * stretchDelta
```

目标：

- 将 `normalize / sqrt / division` 从每顶点执行改成每 child 执行。
- 保持原有方向与 Stretch 语义。

---

## 1.3 Phase 1 不立即做大型 Transform Cache

先完成：

```text
1.1 + 1.2
```

如果 Hierarchy / Vertex 仍然 > 3 ms，再增加细粒度 profiling：

```text
keypoint_us
visual_transform_us
stretch_us
vertex_transform_us
uv_update_us
```

只有拿到数据后才决定是否继续：

```text
world transform cache
identity transform fast path
数学函数替换
```

不要提前重构整个 hierarchy。

---

## Phase 1 测试场景

```text
STATIC
EYE_L_SWEEP
NECK_SWEEP
FACE_SWEEP
MULTI_AXIS
```

重点比较：

```text
hierarchy_vertex_us
rt30_us
producer_total_us
frame_total_us
```

---

## Phase 1 保留条件

如果：

```text
4.9 ms → 4.6 ms
```

只有很小收益，不继续复杂化。

如果：

```text
4.9 ms → 2~3 ms
```

则保留，并继续后续局部更新。

---

# Phase 2：PPA / CPU 四组合 A/B

当前 PPA FILL + SRM 约为：

```text
7.5 ~ 8.0 ms/frame
```

属于第二大成本。

当前工程已经存在：

```text
CPU memset clear
CPU ARGB8888 → RGB565
PPA FILL
PPA SRM
```

不要重新实现。

---

## 2.1 增加 Backend 配置

建议增加：

```text
L2D_CLEAR_CPU
L2D_CLEAR_PPA

L2D_CONVERT_CPU
L2D_CONVERT_PPA
```

可通过 Kconfig 或编译期配置切换。

---

## 2.2 测试四种组合

### A：当前基线

```text
PPA clear
PPA convert
```

### B

```text
CPU clear
PPA convert
```

### C

```text
PPA clear
CPU convert
```

### D

```text
CPU clear
CPU convert
```

每种运行：

```text
STATIC
MULTI_AXIS
```

配置：

```text
warmup 5
measure 50
rounds 1
```

---

## 2.3 必须看整帧

不能只比较：

```text
clear_us
convert_us
```

必须同时比较：

```text
raster_us
producer_total_us
frame_total_us
P95
P99
```

因为 CPU clear / convert 可能改变 Cache 和 PSRAM 后续访问行为。

---

## Phase 2 输出

创建：

```text
doc/optimization/02-ppa-ab.md
```

表格：

| Clear | Convert | Raster | Clear | Convert | Producer | Frame |
|---|---|---:|---:|---:|---:|---:|
| PPA | PPA | | | | | |
| CPU | PPA | | | | | |
| PPA | CPU | | | | | |
| CPU | CPU | | | | | |

最终保留：

```text
producer_total_us / frame_total_us
```

最优的一组。

---

# Phase 3：静止帧复用

STATIC 当前仍然每帧支付：

```text
Raster ≈ 18.4 ms
PPA ≈ 7.6 ms
```

即使视觉完全不变，也重新生成整帧。

---

## 3.1 增加 Visual Revision

新增：

```c
visualRevision
lastRenderedRevision
```

以下变化必须增加 `visualRevision`：

```text
RT30 pose
Texture
Layer visibility
Layer opacity / color
RenderScale
View transform
Model reload
Sampler state
```

---

## 3.2 视觉状态不变时

当：

```text
visualRevision == lastRenderedRevision
```

则跳过：

```text
clear
hierarchy update
raster
ARGB→RGB565 convert
```

直接复用上一帧结果。

如果显示端仍要求提交，则重新提交已有 RGB565 frame。

---

## 3.3 Controller 仍继续运行

不能因为复用画面而冻结：

```text
时间
Blink timer
Animation state
Event
Action state
```

视觉发生改变后必须立即恢复正常 render。

---

## Phase 3 验收

STATIC 的：

```text
producer_total_us
frame_total_us
```

应出现数量级下降。

同时验证：

```text
下一次 axis 改变后
画面立即刷新
```

---

# Phase 4：局部更新 / Dirty Region

该阶段复杂度较高，但潜在收益很大。

---

## 4.1 建立 Dirty Layer Set

RT30 已知每个 Axis 影响的 Binding Layer。

流程：

```text
Changed Axis
↓
Binding Layers
↓
Affected Child Dependencies
↓
Dirty Layer Set
```

禁止根据 Layer 名字猜依赖关系，必须读取模型真实绑定。

---

## 4.2 保存每层 Old / New AABB

每个 Layer 保存：

```text
oldBounds[layer]
newBounds[layer]
```

Dirty Rect：

\[
Dirty =
\bigcup (oldBounds \cup newBounds)
\]

并向外扩展：

```text
1 ~ 2 px
```

作为安全边界。

---

## 4.3 清区后重新绘制所有相交 Layer

错误做法：

```text
只重画发生变化的眼睛
```

正确做法：

```text
Clear Dirty Rect
↓
遍历全部 visible layer
↓
如果 layer AABB 与 Dirty Rect 相交
    按原 Render Order 重画
```

否则会出现：

```text
残影
空洞
遮挡关系错误
```

---

## 4.4 Dirty Rect 传递到 PPA FILL

当前实现固定：

```text
dirty = 320×320
```

修改为：

```text
dirty_x
dirty_y
dirty_w
dirty_h
```

只清对应区域。

---

## 4.5 Dirty Rect 传递到 PPA SRM

必须确保是：

```text
Source Dirty Rect
→
RGB565 Buffer 同坐标区域
```

第一版要求：

```text
scale = 1:1
```

不能沿用现有：

```text
buffer_width / source_block_width
```

否则局部区域会被缩放到整张画布。

同时必须处理：

```text
source offset
destination offset
row stride
```

---

## 4.6 第一版仍可提交完整 RGB565 Frame

当前 Panel submit 本身耗时很小，因此第一版 Dirty Render 可以：

```text
局部 clear
局部 raster
局部 convert
↓
完整 320×320 framebuffer submit
```

先降低实现风险。

后续再研究 Partial Panel Update。

---

## Phase 4 重点测试

```text
EYE_L
EYE_R
NECK
FACE
MULTI
```

预期：

- Eye 类动作可能受益最大。
- Face 轴由于影响全模型，收益可能较小。

必须以实际数据为准。

---

# Phase 5：Fast-Nearest 软件 Raster 优化

注意：

当前 ESP32-P4 已经使用：

```text
Affine UV
16.16 fixed-point
Nearest sampling
Direct dst pointer
Alpha fast path
```

不要把 PC 双线性优化方案直接搬到 MCU 主路径。

---

## 5.1 去掉 Fast-Nearest Span 中重复 UV Division

当前 Triangle 已经计算：

```text
affine_s_dx
affine_s_dy
affine_t_dx
affine_t_dy
```

但 Span 仍然重新：

```text
s_step =
    (sright - sleft) /
    (xright - xleft)

t_step =
    ...
```

然后再：

```text
s_fp_step = s_step * texture_width * 65536
t_fp_step = t_step * texture_height * 65536
```

实验修改为：

```text
s_fp_step =
    affine_s_dx * texture_width * 65536

t_fp_step =
    affine_t_dx * texture_height * 65536
```

目标：

```text
移除每 Span 的 UV Division
```

### 强制要求

必须进行逐像素输出比对。

如果 CRC 或像素发生变化，则不能归类为 exact optimization。

---

## 5.2 Scanline 左右边界增量

将每行重复边界除法尝试替换为：

```text
x_left += dxdy_left
x_right += dxdy_right
```

必须单独验证：

```text
水平边
垂直边
极细三角形
共享边
负坐标
裁剪边缘
```

如果覆盖发生差异，归类为 numeric approximation，不得混入 exact 分支。

---

## 5.3 Alpha Occupancy 实验

当前 STATIC：

```text
fragment ≈ 52762
alpha_zero ≈ 8437
约 16%
```

透明浪费主要集中在：

```text
front_hair
back_hair
```

所以第一版 occupancy 只针对两层。

建议：

```text
8×8 texture blocks

EMPTY
NONEMPTY
```

如果整个连续 Span 可确认落在 EMPTY 区域：

```text
跳过 Texture Fetch
```

不要一开始给所有 Layer 增加 occupancy 查询。

---

## Phase 5 提交规则

每项独立提交：

```text
R5.1 affine dx span step
R5.2 incremental scanline edge
R5.3 hair alpha occupancy
```

不能合并成一个 Commit。

---

# Phase 6：模型结构优化

到这里才开始改 `.live` 模型。

---

## 6.1 保留原模型

创建：

```text
esp_original.live
esp_mesh_opt.live
```

禁止直接覆盖原始版本。

---

## 6.2 第一轮只修改 Hair

优先：

```text
front_hair
back_hair
```

因为两者占总采样约 57%，且透明采样主要集中在这里。

不要第一轮同时修改：

```text
face
eye
mouth
cloth
```

---

## 6.3 优化目标是 Fragment，不是 Triangle 数

允许：

```text
80 triangles → 110 triangles
```

只要：

```text
Fragment 明显下降
Raster 时间下降
```

就是成功。

目标：

```text
Mesh 更贴 Alpha Contour
```

而不是机械减少 Triangle。

---

## 6.4 RT30 顶点绑定必须重新处理

当前 RT30 Sample 依赖 Vertex Index。

如果 Mesh 重建：

```text
Vertex 数量 / Index
```

发生改变，则禁止继续直接复用旧 RT30 Vertex Sample。

必须：

```text
重新映射
或
重新 Bake RT30
```

然后验证所有姿态。

---

## Phase 6 验收指标

```text
fragments
alpha_zero
average_overdraw
max_overdraw
raster_us
hierarchy_vertex_us
visual correctness
```

如果整个模型结构重建带来的 raster 改善 < 5%，不建议继续大规模人工重做模型。

---

# Phase 7：编辑器模型优化能力

只有 Phase 6 证明模型结构优化确实有效后，再将其产品化到编辑器。

---

## 7.1 Performance Inspector

编辑器显示：

```text
Layer ID
Vertices
Triangles
Fragment Estimate
Alpha-zero Ratio
Coverage
```

用于快速找到性能最差 Layer。

---

## 7.2 Alpha Contour Auto Mesh

流程：

```text
Texture Alpha
↓
Threshold
↓
Morphological Padding 1~2 px
↓
Contour Extraction
↓
RDP Simplify
↓
Interior Points
↓
Delaunay
```

生成后允许人工调整。

---

## 7.3 PSD Import

流程：

```text
PSD
↓
Layer Hierarchy
↓
Texture Crop
↓
Create Live Layers
↓
Auto Mesh
↓
LiveEditor Project
```

要求：

- PSD 只由 PC 编辑器解析。
- ESP Runtime 不支持 PSD。
- 第一版不修改 `.live` 格式。
- Photoshop Group 与 Live2D Transform Parent 的关系要允许导入后调整。

---

# Phase 8：完整系统集成优化

当前 Benchmark 不是最终桌宠完整负载。

最终必须同时开启：

```text
Live2D
LVGL
ESP-Hosted
Wi-Fi
MCP
UAC Audio
SD
Touch
```

重新 profiling。

---

## 8.1 Full-System Benchmark

重新测：

```text
RT30
Hierarchy / Vertex
Raster
PPA
PSRAM
Cache
Display Lock
Frame Interval
P50
P95
P99
```

---

## 8.2 Core Affinity A/B

当前基线：

```text
Render → Core0 priority6
LVGL   → Core0 priority4
```

完整系统下测试：

### A

```text
Render Core0
LVGL Core0
```

### B

```text
Render Core1
LVGL Core0
```

结合实际：

```text
Wi-Fi
Audio
SD
ESP-Hosted
```

所在 Core 决定最终方案。

不能提前认定 Core1 一定更快。

---

## 8.3 最后才考虑异步 PPA

如果完整系统优化后仍存在：

```text
PPA Wait ≈ 5~8 ms
```

再考虑：

```text
Double Buffer
+
Non-blocking PPA
+
CPU / PPA Overlap
```

例如：

```text
CPU Render Frame N+1
        │
        └─────────────┐
                      │
PPA Convert Frame N ──┘
```

这需要明确：

```text
ARGB slot A
ARGB slot B
RGB565 slot A
RGB565 slot B
```

和严格的 Buffer Ownership。

不能在当前单缓冲结构上直接把 PPA 调用改成 async。

---

# 每阶段 AI 必须输出的文档

统一目录：

```text
doc/optimization/
```

建议文件：

```text
00-profile-v2.md
01-rt30-vertex.md
02-ppa-ab.md
03-frame-reuse.md
04-dirty-render.md
05-raster.md
06-model-mesh.md
07-editor-model-tools.md
08-full-system.md
```

每份文档必须包含：

```text
1. 本阶段目标
2. 修改文件
3. 修改内容
4. 修改原因
5. 正确性测试
6. 测试环境
7. Before
8. After
9. P50 / P95 / P99
10. 收益百分比
11. 是否保留本优化
12. 已知风险
13. 下一阶段建议
```

---

# Git Commit 规范

建议：

```text
perf(profile): split PPA and display timing

perf(runtime): skip unused RT30 elastic preparation

perf(runtime): optimize child stretch projection

perf(platform): benchmark CPU and PPA backends

perf(runtime): reuse unchanged Live2D frame

perf(runtime): add dirty-layer rendering

perf(renderer): remove redundant fast-nearest span divisions

perf(model): optimize hair contour mesh

feat(editor): add Live2D performance inspector

feat(editor): add PSD layer importer
```

每项单独 Commit。

---

# 推荐实际执行顺序

第一轮只实施：

```text
Phase 0
Phase 1
Phase 2
```

原因：

- 模型不变。
- `.live` 格式不变。
- 编辑器不变。
- 风险最低。
- 收益容易独立归因。
- 可以先确认 MCU 真正的下一阶段优先级。

第一轮完成后，应暂停后续修改，重新提交：

```text
Baseline v2
RT30 Vertex Before / After
PPA 四组合 Before / After
```

再根据新数据决定 Phase 3～5 的实际优先顺序。

---

# 可直接交给 AI 的任务提示词

对 `sdegfd/live2d` 项目进行 Live2D 性能优化。不要一次性实施全部优化，必须严格按照 Phase 0 → Phase 8 的顺序开发。

当前冻结性能基线为 ESP32-P4 rev1.3、CPU 360 MHz、PSRAM 200 MHz、ESP-IDF 5.5.2、`-O2`，模型 320×320、11 层、227 顶点、257 三角形、12 纹理、4 个 RT30 轴，Sampler 为 `fast-nearest`。当前 MULTI_AXIS 约为：Producer 32.38 ms、Software Raster 18.21 ms、RT30 0.99 ms、Hierarchy/Vertex 4.91 ms、PPA FILL+SRM 7.93 ms。禁止把 PC 双线性 benchmark 当作当前 MCU 渲染主路径。

Phase 0 只完善 profiling：拆分 PPA FILL/SRM、各阶段 Cache Sync、Display Lock、Flush Total、Frame Total；修正当前 checksum 设计，建立独立 correctness 测试。使用 STATIC、EYE_L、MULTI_AXIS 长测建立 baseline_v2。

Phase 1 优化 RT30 hierarchy/vertex：先跳过 REALTIME30 下不会使用的 elasticity `keyDirection` 准备；再将 child stretch 从每顶点重复 `dot/length/normalize` 改为每 layer/child 预计算单位方向，每顶点只做投影。禁止整体用 PC `PX_LiveFramework.c` 覆盖 ESP 文件。每项必须独立测试 CRC 和 timing。

Phase 2 进行 PPA/CPU A/B：利用已有 CPU clear、CPU RGB565 convert、PPA FILL、PPA SRM 实现，建立四种 Backend 组合并比较完整 Producer/Frame Time，不能只比较单函数耗时。

Phase 3 建立静止帧复用：使用 visual revision；画面没有变化时不重新 clear、update、raster、convert，但继续推进 controller/time。

Phase 4 做 Dirty Region：根据 RT30 Binding 生成 dirty layer；保存 old/new AABB；Clear Dirty Rect 后必须按原 Render Order 重画所有与 Dirty Rect 相交的 visible layer。Dirty Rect 同时作用于 PPA FILL 和 SRM。第一版允许最后仍提交完整 RGB565 framebuffer。

Phase 5 优化当前 fast-nearest Raster：不要优先开发 double→float bilinear。先尝试利用已有 `affine_s_dx/affine_t_dx` 移除每 Span 重复 UV Division，再实验 Scanline 边界增量。所有 Exact Optimization 必须逐像素验证。透明 Occupancy 第一版仅针对 front_hair / back_hair。

Phase 6 才修改模型结构：保留原 `esp.live`，只先实验 front_hair / back_hair 的 Alpha Contour Mesh。指标是 Fragment 与 Raster 时间，不是 Triangle 数。任何改变 Vertex Index 的 Mesh 重建都必须重新映射或重新 Bake RT30。

Phase 7 再修改编辑器：增加 Performance Inspector、Alpha Contour Auto Mesh 和 PSD Import。PSD 只用于 PC 制作端，第一版不得要求 ESP Runtime 支持 PSD，也不得无必要修改 `.live` 格式。

Phase 8 将优化版本接回完整桌宠系统，在 LVGL、Wi-Fi/ESP-Hosted、MCP、UAC、SD 等负载同时开启的条件下重新 Profiling，再测试 Core Affinity。只有完整系统下仍确认 PPA 阻塞显著，才考虑双缓冲和 Non-blocking PPA。

每个阶段必须独立 Commit、独立 Benchmark、保存原始 CSV，并生成 `doc/optimization/XX-*.md`。每份报告必须给出 Before/After、P50/P95/P99、正确性结果、收益比例和是否建议保留。不得为了性能指标偷偷降低模型显示尺寸、降低画质、取消 RT30 动作或改变模型内容。任何画质或行为变化必须明确标记为 Trade-off。
