# PainterEngine 多轴全离散实时动画扩展计划

> 文档状态：已按 ESP32-P4 与 6 MiB PSRAM 约束重新整定；尚未修改源码。  
> 更新日期：2026-07-13  
> 桌面编辑器：`D:\live2d\project\PainterEngine_LiveEditor`  
> PainterEngine：`D:\live2d\PainterEngine`  
> 目标设备：ESP32-P4  
> 硬约束：PainterEngine 实时模型系统整体使用一个 6 MiB PSRAM arena。

## 1. 已锁定的需求

本计划以以下六条为不可改变的设计输入：

1. 每个实时参数轴固定有 30 个离散姿态样本。
2. 运行时每个轴只选择 30 个样本中的一个，禁止读取相邻样本做轴内插值。
3. 多个轴各自选中一个离散样本，再依据轴权重融合。
4. 时间动画与实时动画是两个独立系统；实时系统只负责眨眼、嘴巴开合、摇头等简单状态。
5. Editor 每个轴只让用户制作 3 个关键姿态，另外 27 个样本由 Editor 离线插值并烘焙。
6. ESP32-P4 部署不新增第三方数学、序列化或动画库；实时模型、30 点数据、运行状态和工作缓冲全部放入 PSRAM，并整体受 6 MiB 上限约束。

显示帧缓冲不属于 `.live` 模型系统的数据，因此不计入本计划的 6 MiB 模型 arena；除显示帧缓冲外，本计划不依赖内部 SRAM 热缓冲方案。

## 2. 最终架构结论

PainterEngine 保留两套互不叠加的动画执行器：

```text
                   ┌─ 时间动画系统 Timeline ── 播放预制帧
基础模型/网格/纹理 ┤
                   └─ 实时动画系统 RT30 ───── 多轴离散状态融合
```

- Timeline 沿用当前 `PX_LiveAnimation`、帧 payload 和时间 VM。
- RT30 是新系统，使用 30 点离散轴，不读取 Timeline 帧。
- 一个模型实例同一时刻只处于 Timeline 或 RT30 模式。
- 两套系统共享基础纹理、网格、图层层级和最终渲染器。
- 模式切换时清空上一系统的运行状态并从中性姿态重新求值。
- ESP32-P4 的实时部署包默认完全剥离 Timeline 数据，只保留基础模型和 RT30。

本方案不实现：

- 时间动画播放过程中叠加实时眨眼。
- 轴内相邻样本插值。
- 30×30 或 30×30×30 多维组合表。
- 运行时曲线、Bezier、关键帧时间或补间器。

## 3. RT30 的精确定义

### 3.1 参数轴

每个实时轴包含：

- 唯一 ID，例如 `ParamEyeLOpen`。
- 显示名，例如“左眼开合”。
- 固定 30 个运行时样本，索引范围 `0..29`。
- 3 个用户关键姿态：索引 0、中间索引 M、索引 29。
- 中间索引 M 由用户设置，范围 `1..28`。
- 默认样本索引，必须等于 0、M、29 三个关键姿态之一。
- 当前离散样本索引 `sampleIndex`。
- 当前轴权重 `weightQ15`。
- 受影响图层、属性和顶点的稀疏绑定。

设备运行时不需要保存 30 个浮点参数坐标。语义范围只存在于 Editor；设备只认 `0..29` 的离散索引。

### 3.2 三个关键姿态

用户只编辑：

```text
K0 = sample[0]
K1 = sample[M]
K2 = sample[29]
```

典型配置：

| 参数 | K0 | K1 | K2 | 默认关键点 |
|---|---|---|---|---|
| 眨眼 | 完全闭眼 | 半闭 | 完全睁眼 | K2 |
| 嘴巴开合 | 闭嘴 | 半开 | 全开 | K0 |
| 摇头 X | 最左 | 正中 | 最右 | K1 |
| 点头 Y | 最下 | 正中 | 最上 | K1 |

默认关键姿态保存零增量，另外两个关键姿态保存相对于默认姿态的模型空间增量。

### 3.3 另外 27 点的离线生成

Editor 在 Bake 时进行两段线性插值：

```text
sample[0..M]  = interpolate(K0, K1)
sample[M..29] = interpolate(K1, K2)
```

规则：

- 顶点位移、图层平移：二维线性插值。
- 旋转：先展开到最短角度路径，再线性插值。
- 伸缩：在 Editor 中插值后直接烘焙每个样本最终值。
- 所有插值只发生在桌面 Editor 的 Bake 阶段。
- Bake 后形成完整 30 点数据，再量化并写入 RT30 数据区。
- ESP32-P4 运行时不知道哪 3 点是关键点，也不执行任何插值。

## 4. 全离散运行规则

### 4.1 设置轴状态

最快的设备 API 直接接收样本索引：

```text
SetAxisSample(axisHandle, sampleIndex[0..29], weightQ15[0..32767])
```

如果上层输入是归一化数值，可使用一个仅做取整的帮助函数：

```text
sampleIndex = round(normalizedQ15 × 29)
```

这个帮助函数只返回一个索引；不会返回两个索引，也不会生成轴内插值权重。

### 4.2 单轴取样

运行时对每个活动轴只执行：

```text
selectedPose = axis.sample[sampleIndex]
```

不执行以下操作：

- 不读 `sampleIndex + 1`。
- 不计算小数 t。
- 不进行二分查找。
- 不遍历 30 点。
- 不解析 Editor 的 3 个关键姿态。

### 4.3 多轴融合

每个轴选中一个完整离散姿态后，才进行轴之间的权重融合：

```text
FinalPose = NeutralPose + Σ(SelectedPose[axis] × AxisWeight[axis])
```

图层和顶点规则：

```text
FinalLayerTranslation = Σ(discreteTranslation[a] × weight[a])
FinalLayerRotation    = Σ(discreteRotation[a]    × weight[a])
FinalVertexDelta      = Σ(discreteVertexDelta[a] × weight[a])
FinalStretchDelta     = Σ(discreteStretchDelta[a] × weight[a])
```

最后才把这些局部增量应用到基础模型，并统一计算父子图层和顶点。

独立参数轴不做全局归一化。例如眨眼、张嘴、摇头都为 100% 时，三个动作都应完整生效。仅当以后实现嘴形 A/I/U/E/O 等互斥组时，才允许对指定组单独归一化；该功能不进入第一版。

### 4.4 定点权重

轴权重使用无符号 Q0.15：

```text
0      = 0%
16384  = 50%
32767  = 约 100%
```

每个量化姿态分量与权重相乘后立即右移 15 位，再累加到 32 位 accumulator：

```text
contribution = (deltaInt16 × weightQ15 + rounding) >> 15
accumulatorInt32 += contribution
```

这样避免在多个轴相乘结果上使用 64 位累加，也使最大活动轴数量可明确限制。最终 accumulator 再按统一量化比例转换为 PainterEngine 当前需要的模型坐标。

## 5. 时间动画与实时动画分离

### 5.1 Timeline 系统

继续负责：

- 预制动作。
- 帧时长和帧间补间。
- 纹理切换。
- 冲量和带时间状态。
- 桌面端已有动画工作流。

### 5.2 RT30 系统

只负责：

- 左右眼开合。
- 嘴巴开合。
- 头部左右、上下和倾斜。
- 少量身体摆动或眉毛参数。
- 若干轴当前离散状态之间的权重融合。

RT30 不保存 duration、不自动播放、不推进指令指针。只有外部调用改变样本索引或权重时，RT30 才需要重新求值。

### 5.3 运行模式

新增明确模式：

```text
PX_LIVE_MODE_NEUTRAL
PX_LIVE_MODE_TIMELINE
PX_LIVE_MODE_REALTIME30
```

第一版禁止 Timeline 和 RT30 同时启用。这样可以：

- 避免两套系统争用相同的图层字段。
- 避免为叠加系统增加第二套大 accumulator。
- ESP32-P4 实时包直接删除全部时间帧，节省数 MiB。
- 保证实时求值只与活动轴数量和受影响顶点数有关。

## 6. 可复用的 PainterEngine 资源

| 资源 | 复用方式 | 必须调整的部分 |
|---|---|---|
| `PX_LiveLayer/PX_LiveVertex` | 继续作为基础图层、网格和渲染输入 | 新增 RT30 当前局部姿态或独立运行态 |
| 父子图层和 keyPoint | 继续计算骨骼与控制点 | 允许任意子图层局部平移；统一局部空间规则 |
| 当前纹理和三角形渲染 | RT30 最终姿态继续走现有 renderer | 渲染不能推进 RT30 状态 |
| `PX_Object_Live2D` | 增加 RT30 模式和设置轴接口 | 时间播放接口保持不变 |
| `PX_MemoryPool` | 用 6 MiB PSRAM 创建单 arena | 加载前预计算大小；禁止运行时碎片分配 |
| `PX_Object_List/SliderBar/Edit/Widget` | 制作参数列表、30 档滑条和设备导出面板 | Slider 必须严格吸附到 0..29 |
| `KeyTransform/GlobalRotation/VertexTransform` | 制作 3 个关键姿态 | 通过统一 RT PoseAccessor 写关键姿态 |
| `PX_Arle/PX_LZ77` 等 PainterEngine 自有代码 | 若部署文件需要压缩可复用 | 运行时 PSRAM 中仍是最终可直接访问格式 |

不引入 Eigen、GLM、protobuf、FlatBuffers、zlib、LZ4、外部 tween/animation 库。ESP-IDF 作为设备平台基础不视为新增动画依赖。

## 7. 代表模型的内存证据

### 7.1 ESP32-P4 已验证基线

已有设备实测结论：在当前 PainterEngine 框架下，使用其示例模型时分配 **5 MiB 内存池可以正常运行**。因此本计划不再只按 `.live` 文件大小估计设备占用，而把 5 MiB 作为保守的现有框架基线。

在尚未加入内存池高水位统计前，预算规则固定为：

```text
现有框架 + 示例模型：按最多 5 MiB 预留
RT30 新增能力：只能使用剩余 1 MiB
总 PSRAM arena：6 MiB
```

5 MiB“足够运行”不等于实际峰值恰好为 5 MiB。实施阶段 0 必须增加 arena used/high-water 统计，测得真实峰值后才能重新分配余量；在此之前不借用这 5 MiB 中可能存在但未经证明的空闲空间。

### 7.2 文件内容拆解

只读解析 `D:\live2d\project\release.live`：

| 内容 | 字节数 |
|---|---:|
| 文件总量 | 3,747,332 |
| RGBA 纹理像素 | 717,812 |
| 完整纹理区 | 720,404 |
| 图层、网格和顶点区 | 21,016 |
| 时间动画区 | 3,005,832 |
| 去掉时间动画后的基础模型 | 741,500 |

去掉 Timeline 后，代表模型文件中的基础部分仅占 6 MiB 的约 11.79%。这是磁盘内容拆解，不代表 PainterEngine 加载后的完整内存池峰值；设备预算优先采用上面的 5 MiB 实测基线。文件拆解仍证明“ESP32-P4 实时包剥离时间动画”可以避免把约 3.01 MiB 时间帧数据带入实时包。

该模型有 34 个图层、160 个顶点。若 8 个实时轴全部影响全部顶点，并使用 int16 二维顶点增量：

```text
160 vertices × 30 samples × 4 bytes × 8 axes = 153,600 bytes
```

再加少量图层变换和索引，RT30 仍远低于 1 MiB。实际眨眼、嘴巴和摇头通常只影响部分图层，稀疏绑定后会更小。

## 8. 6 MiB PSRAM 单 arena

### 8.1 硬上限

```text
6 MiB = 6 × 1024 × 1024 = 6,291,456 bytes
```

以下数据全部来自这个 PSRAM arena：

- 纹理像素。
- 基础图层、网格、顶点和 UV。
- RT30 轴定义、绑定、顶点索引和 30 点姿态。
- 当前轴的样本索引和 Q15 权重。
- 图层 accumulator。
- 顶点 accumulator。
- 当前变换后顶点、控制点和必要物理状态。
- 加载后的索引表和对齐填充。

禁止把性能成立的前提建立在内部 SRAM 热缓存上。

### 8.2 建议预算

在取得精确高水位前，采用最保守的固定预算：

| 分类 | 建议上限 | 占比 |
|---|---:|---:|
| 当前框架 + 示例模型基线 | 5,242,880 B | 83.3% |
| RT30 烘焙轴数据 | 655,360 B | 10.4% |
| RT30 运行状态/accumulator | 131,072 B | 2.1% |
| RT30 元数据、对齐和安全余量 | 262,144 B | 4.2% |
| 总计 | 6,291,456 B | 100% |

前三项是硬预算边界，不能依赖“5 MiB 池可能还有空闲”来超配 RT30。获得设备高水位数据后可以重新整定分类，但 6 MiB 总上限不变。Editor 导出时必须显示真实分类统计，而不是只显示文件压缩后大小。

### 8.3 arena 分配策略

- 启动时一次性获得 6 MiB PSRAM 连续块。
- 用 PainterEngine 自有 bump/arena 分配器从低地址向高地址顺序分配。
- 模型加载完成后冻结静态区。
- RT30 运行状态和 accumulator 在加载阶段一次性分配。
- 每帧禁止 `MP_Malloc/Free`、`malloc/free` 和 vector 扩容。
- 模型卸载时整体释放 arena，不逐对象释放。
- 所有块至少 4 字节对齐；样本数据建议 16 字节对齐，便于连续读取和以后使用 ESP32-P4 128 位扩展。

### 8.4 加载前预检

设备文件头必须包含 `requiredPsramBytes`。加载流程：

1. 读取并校验固定头。
2. 检查 `requiredPsramBytes <= 6,291,456`。
3. 检查所有 chunk 的 size/offset/count 无溢出。
4. 通过后才创建模型对象并顺序装载。
5. 任一步失败就整体放弃，不留下半初始化结构。

Editor 的 ESP32-P4 导出器使用与设备完全相同的布局计算器，保证桌面报告值与设备实际申请值一致。

## 9. 紧凑 RT30 数据格式

### 9.1 为什么不能直接复制现有帧 payload

当前 `PX_LiveAnimationFramePayload` 包含全部属性和 `reserve[32]`，每帧还保存所有图层、所有顶点。直接把它复制 30 次/轴会浪费大量 PSRAM。

RT30 必须使用专用结构，只保存该轴真正影响的数据。

### 9.2 静态轴布局

```text
RT30AxisHeader
  idHash32
  middleKeyIndex
  defaultSampleIndex
  bindingCount
  sampleStride
  bindingsOffset
  samplesOffset

RT30Binding[]
  targetLayerIndex
  propertyMask
  affectedVertexCount
  vertexIndexOffset
  dataOffsetWithinSample

sample[0]  contiguous data block
sample[1]  contiguous data block
...
sample[29] contiguous data block
```

特点：

- 同一轴 30 个样本的受影响顶点集合相同，顶点索引只存一次。
- 样本按 sample-major 排列，运行时选定一个 index 后只读一个连续块。
- 全部引用优先使用相对 32 位 offset，减少指针数量并方便一次性搬入 PSRAM。
- 设备 Release 包可只保留 32 位 ID hash；Editor 保留完整字符串。
- Editor 导出时检查 hash 冲突。

### 9.3 姿态量化

Editor 内部继续用 float32 编辑，ESP32-P4 RT30 包从第一版就量化：

- 顶点/图层平移：int16，使用模型统一 `coordFractionBits`。
- 旋转：int16，使用独立 `rotationFractionBits`。
- 伸缩增量：int16，使用 Q 格式。
- 轴权重：uint16 Q0.15。
- 图层索引/顶点索引：顶点数小于 65535 时使用 uint16。
- sampleIndex：uint8。

统一比例使不同轴可以先在 int32 中直接累加，最后只转换一次到 PainterEngine 模型坐标。

Editor 自动选择尽可能高的小数位数，同时保证所有已烘焙增量落在 int16 范围。导出报告必须显示：

- 每种数据的量化步长。
- 最大绝对误差。
- 是否发生饱和。
- 饱和时禁止导出，而不是静默 Clamp。

### 9.4 顶点稀疏化

Bake 后检查每个轴的所有 30 点：

- 若一个顶点在全部样本中的 x/y 增量都为零，则从该轴绑定删除。
- 图层属性 30 点全为零时清除对应 property bit。
- 多个相邻顶点索引可按连续区间编码，但第一版优先使用简单 uint16 索引表。
- 稀疏化只在 Editor 导出时执行；设备不做优化扫描。

## 10. PSRAM 性能策略

ESP32-P4 官方资料说明 PSRAM 经过缓存访问；大块工作集超过缓存能力后会回落到外部 RAM 访问速度。因此重点不是遍历速度微优化，而是减少每帧从 PSRAM 读取的字节数。

### 10.1 每个活动轴只读一个样本块

运行时帧读取量近似：

```text
Σ(activeAxis.selectedSampleStride)
```

与 30 无关。30 个样本只增加静态存储，不增加单帧读取量。

### 10.2 样本块目标

- 单轴选中样本块建议不超过 16 KiB。
- 所有活动轴本帧样本读取总量建议不超过 32 KiB。
- 简单脸部模型建议总轴数不超过 16，活动轴不超过 8。
- 格式硬上限可设为 32 个轴，但导出器必须按实际样本块大小给出性能警告。

### 10.3 只在状态变化时求值

每个 RT30 实例保存 revision：

- sampleIndex 或 weight 变化时标记 dirty。
- 全部轴状态未改变时，复用上次最终姿态，不扫描轴和顶点。
- 任意轴变化时，从零清空预计算的“受影响顶点并集”，再顺序累加所有活动轴。
- 第一版不做增量撤销旧轴贡献，避免复杂状态和累计误差。

### 10.4 连续内存和循环

- 样本数据 sample-major、绑定数据连续。
- 内层循环只做 int16 load、Q15 multiply、shift 和 int32 add。
- 避免函数指针、哈希、字符串和链表出现在顶点循环。
- 避免 per-vertex property mask 分支；在导出时按属性拆成连续区段。
- 标量 C 实现先达到正确性和基准，再按测试结果增加 ESP32-P4 128 位扩展的可选实现。
- SIMD 实现必须由编译宏隔离，标量实现始终保留，不依赖外部 DSP 库。

### 10.5 单核优先

RT30 求值第一版使用单个 HP 核顺序执行。对于几千次 int16 累加，多核任务同步成本可能高于收益。只有硬件基准证明模型更新成为瓶颈后，才考虑按图层拆分；不把双核作为满足性能目标的必要条件。

## 11. 性能与内存硬指标

### 11.1 内存指标

- PainterEngine 实时模型 arena 固定 6,291,456 字节 PSRAM。
- 加载结束后记录 used、remaining、largest category。
- 每帧动态分配次数必须为 0。
- 设备 RT30 包必须剥离 Timeline 和 Editor authoring 数据。
- 在当前 5 MiB 实测基线下，RT30 烘焙数据不得超过 640 KiB。
- RT30 运行状态与 accumulator 不得超过 128 KiB。
- RT30 元数据、对齐和安全余量保留 256 KiB。
- 阶段 0 测得真实 high-water 前，不得扩大上述 RT30 预算。

### 11.2 性能指标

在 ESP32-P4 HP 核 400 MHz 的设备实测：

- 8 个活动轴。
- 每帧总计不超过 4096 个顶点贡献。
- RT30 选样 + 融合 P95 目标小于 1 ms。
- RT30 融合 + 层级/顶点更新 P95 目标小于 3 ms。
- 60 Hz 参数输入下无分配、无内存增长。
- 状态完全不变时 RT30 更新应接近常数时间。
- 性能报告必须分开记录 RT30 融合、骨骼/顶点更新和光栅化，不能只看总 FPS。

若未达到指标，优化顺序固定为：

1. 减少活动轴影响的顶点数。
2. 改善 sample-major 连续布局。
3. 减少本帧 PSRAM 读取字节数。
4. 展开和合并内层循环。
5. 使用 ESP32-P4 自带 128 位扩展。
6. 最后才考虑多核。

## 12. Framework 扩展方案

### 12.1 独立 RT30 模块

建议新增：

```text
kernel/PX_LiveRealtime.h
kernel/PX_LiveRealtime.c
```

职责：

- RT30 静态数据定义。
- 6 MiB arena 布局计算。
- 轴 handle 查找。
- 设置 sampleIndex/weightQ15。
- dirty/revision 管理。
- 离散样本块累加。
- RT30 当前姿态输出。

不要把 RT30 指令塞进现有 `PX_LiveFrameworkExecuteInstr()`；它是时间 VM，语义不同。

### 12.2 API

桌面和设备共用的核心 API：

```text
PX_LiveRealtimeEnter(instance)
PX_LiveRealtimeLeave(instance)
PX_LiveRealtimeFindAxis(idHash/id)
PX_LiveRealtimeSetAxisSample(handle, uint8 sampleIndex, uint16 weightQ15)
PX_LiveRealtimeSetAxisNormalizedQ15(handle, uint16 valueQ15, uint16 weightQ15)
PX_LiveRealtimeResetAxis(handle)
PX_LiveRealtimeResetAll()
PX_LiveRealtimeUpdate()
PX_LiveRenderCurrent()
```

高频循环必须使用预先缓存的整数 handle；按字符串 ID 查询只允许在初始化阶段。

### 12.3 Render 解耦

无论 Timeline 还是 RT30，Renderer 都只应画当前姿态。至少拆出：

```text
UpdateTimeline(elapsed)
UpdateRealtime30()
RenderCurrentPose(...)
```

RT30 没有 elapsed 参数。重复 Render 不得改变样本索引、权重或顶点状态。

### 12.4 子图层局部平移

为了让实时参数直接驱动单独眼睑、嘴部或头部子图层，RT30 支持所有图层的局部平移：

```text
childLocal = baseBoneVector + realtimeLocalTranslation
childWorld = parentWorldTransform × childLocal
```

RT30 样本保存模型/图层局部空间增量，不保存屏幕坐标。父子矩阵只在多轴局部增量融合后计算一次。

## 13. Editor 新功能

### 13.1 主导航

右侧侧栏改为：

```text
[图层] [资源] [动画] [实时]
```

“动画”继续编辑 Timeline；“实时”只编辑 RT30，两者 UI 和状态完全分开。

### 13.2 实时轴列表

每个轴显示：

- ID/名称。
- 当前 sampleIndex：0..29。
- 当前权重：0..100%。
- 中间关键索引 M。
- 默认关键点。
- 影响图层数、顶点数。
- 烘焙是否过期。
- 设备内存占用。

### 13.3 三关键点编辑器

编辑区只允许选中三个可编辑格：

```text
[K0: 0] -------- [K1: M] -------- [K2: 29]
```

其余 27 格显示为只读烘焙结果。用户可以：

- 设置 M。
- 选择默认关键点 K0/K1/K2。
- 编辑 K0、K1、K2。
- 从默认复制关键姿态。
- 镜像 K0/K2。
- Bake 30 点。
- 查看任意一个烘焙样本，但不能直接修改过程点。

### 13.4 全离散预览

预览滑条只有 30 档并强制吸附：

- 鼠标拖动只改变整数 sampleIndex。
- 画面只显示当前样本。
- 不绘制两个样本之间的过渡结果。
- 多轴 Mixer 中每个轴也只有整数档位和独立权重。

### 13.5 复用现有编辑工具

新增 `RealtimeKeyPoseAccessor`，让以下工具编辑 K0/K1/K2：

- KeyTransform。
- GlobalRotation。
- VertexTransform。
- 子图层局部平移。

Panc、纹理切换、Impulse 不进入首版 RT30。

编辑时自动 Solo：

- 进入 RT30 模式。
- 其他轴回默认或 weight=0。
- 当前关键姿态 weight=100%。
- Finish 写入关键姿态；Cancel 恢复临时快照。

### 13.6 ESP32-P4 导出面板

必须实时显示：

```text
PSRAM hard limit:       6,291,456
Textures:               ...
Base mesh/layers:       ...
RT30 samples:           ...
Runtime accumulators:   ...
Metadata/alignment:     ...
Total required:         ...
Remaining:              ...
Largest sample block:   ...
Active-axis read budget:...
```

超出 6 MiB、量化饱和、单样本块过大或 hash 冲突时禁止导出，并列出占用最大的轴、图层或纹理。

## 14. 文件格式与部署包

### 14.1 桌面工程格式

`.live` v2 可同时保存两个互相独立的 chunk：

```text
TIME  // 现有时间动画
RTAU  // RT30 Editor authoring：3 个 float32 关键姿态
```

30 点可在打开后重新 Bake；Editor 也可保存 Bake cache，但 cache 不是权威数据。

### 14.2 ESP32-P4 实时格式

设备导出 profile 只包含：

```text
BASE  // 基础模型
TEX   // 设备纹理
RT30  // 已烘焙、量化、稀疏化的 30 点轴
LAYOUT// requiredPsramBytes、offset、count、量化参数
```

明确不包含：

- TIME。
- RTAU 三关键点 float32 authoring 数据。
- Editor 名称、提示、控件状态。
- 未使用图层属性。
- float32 顶点增量样本。

### 14.3 手写安全 Reader

使用 PainterEngine 自己实现的固定宽度 reader：

- little-endian 明确读取。
- 每个 chunk 有 type/version/offset/size/count。
- 所有乘法和加法检查溢出。
- 所有 offset 必须落在文件范围内。
- requiredPsramBytes 必须与布局器重算结果一致。
- 所有 layer/vertex/sample 索引检查范围。

不引入第三方序列化框架。

## 15. 纹理内存策略

纹理通常是 6 MiB 预算中最大的部分，设备导出支持 PainterEngine 自己实现的三种格式：

1. RGBA8888：4 B/px，质量最高，直接兼容现有路径。
2. RGBA4444：2 B/px，适合半透明 Live2D 图层。
3. RGB565：2 B/px，仅适合完全不透明纹理。

第一阶段可以先使用 RGBA8888 完成正确性；但导出面板必须从开始就计算三种格式的实际 PSRAM 占用。后续格式转换和采样由 PainterEngine 自己实现，不引入图片运行库。

文件压缩不等于 PSRAM 节省。6 MiB 校验始终按加载后纹理格式和模型布局计算。

## 16. 分阶段实施计划

### 阶段 0：冻结现有坐标基线

- 完成当前缩放、控制点、父子层级和动画进入的回归。
- 保存正式版和 `release.live` 统计。
- 复现并记录 ESP32-P4 上“5 MiB 内存池可运行示例模型”的配置、模型、峰值 used 和剩余字节。
- 在 PainterEngine arena 增加 used/high-water 统计；只统计，不改变分配行为。
- 确认不引入 RT30 时现有 Timeline 行为不变。

通过条件：已有坐标问题不会与 RT30 混在一起诊断，并获得可重复的 5 MiB 设备内存基线。

### 阶段 1：RT30 数据模型与 6 MiB 布局器

- 新建独立 RT30 核心结构。
- 实现 3 关键点 authoring 结构。
- 实现 30 点 Bake。
- 实现 int16/Q15 量化和误差报告。
- 实现稀疏化和 requiredPsramBytes 精确计算。

通过条件：桌面确定性测试能从 3 点生成 30 点，布局总量逐字节可重算。

### 阶段 2：全离散单轴运行时

- 直接 sampleIndex 选择。
- 只读一个样本块。
- 权重 Q15。
- dirty/revision。
- 零每帧分配。

通过条件：30 个索引逐一输出与 Bake 数据完全一致；不存在轴内插值代码路径。

### 阶段 3：多轴融合和层级

- 多个选中样本的 int32 加权累加。
- 图层、顶点和旋转/伸缩属性。
- 所有图层局部平移。
- 父子层级只求一次。

通过条件：离散眨眼 + 嘴巴 + 摇头可同时按权重工作，无控制点偏移。

### 阶段 4：Timeline/RT30 模式分离

- 三种运行模式。
- 独立 UpdateTimeline/UpdateRealtime30。
- RenderCurrentPose。
- 模式切换重置。

通过条件：Timeline 和 RT30 不共享寄存器，不允许同时写姿态。

### 阶段 5：Editor 实时标签和三点编辑

- RT30 轴列表。
- 三关键点编辑器。
- 30 格只读 Bake 预览。
- 30 档离散 Mixer。
- RealtimeKeyPoseAccessor 和现有变换工具接入。

通过条件：用户只制作 3 点即可预览完整 30 档，多轴预览严格离散。

### 阶段 6：设备格式和 PSRAM 导出器

- BASE/TEX/RT30/LAYOUT chunks。
- 剥离 TIME/RTAU。
- 6 MiB 精确预检。
- 手写 reader。
- 内存分类报告和超限定位。

通过条件：设备 loader 实际 arena used 与 Editor 报告完全一致。

### 阶段 7：ESP32-P4 标量端口和硬件基准

- 新建最小 `platform/esp32p4`/ESP-IDF component。
- 仅接入 RT30 需要的 PainterEngine 子集。
- 全数据放 PSRAM。
- 标量 C 定点融合。
- 记录 cycle、PSRAM 读取量和 P95。

通过条件：达到 6 MiB 和性能硬指标。

### 阶段 8：针对性优化

- 根据硬件 profile 调整布局。
- 合并顶点循环。
- 可选 128 位指令实现。
- 可选 RGBA4444/RGB565 设备纹理。
- 不改变 RT30 数学结果。

通过条件：优化前后逐顶点结果在量化允许误差内一致。

## 17. 测试矩阵

### 17.1 离散规则

- 每个轴只能输出 0..29 的一个样本。
- 连续输入映射后只产生整数索引。
- 检查运行时没有相邻样本读取和 t。
- 30 个样本逐一与 Editor Bake 输出一致。
- sampleIndex 越界被拒绝或 Clamp 到 0/29，不访问非法内存。

### 17.2 三点 Bake

- M=1、M=15、M=28。
- 默认关键点分别为 K0/K1/K2。
- 顶点、平移、旋转、伸缩两段插值。
- 重新 Bake 结果逐字节稳定。
- 未修改关键点时 27 个过程点不漂移。

### 17.3 多轴融合

- 单轴 weight=0/50%/100%。
- 眨眼、嘴巴、摇头三个轴取不同 sampleIndex。
- 多轴影响同一顶点。
- 轴 UI 排列顺序改变不影响固定执行顺序的结果。
- accumulator 不溢出；超出模型允许范围时最终安全 Clamp。

### 17.4 系统分离

- Timeline 模式不读取 RT30。
- RT30 模式不读取 Timeline。
- 两模式切换后从中性姿态开始。
- ESP32-P4 实时包中不存在 TIME chunk。

### 17.5 坐标和图层

- 根图层局部平移。
- 子图层局部平移。
- 父旋转 + 子平移 + 子顶点形变。
- 25%～800% Editor 缩放不改变关键姿态数据。
- 进入/退出 K0/K1/K2 编辑时控制点不偏移。

### 17.6 内存

- Editor total 与设备 arena used 相同。
- 6,291,455/6,291,456 字节允许加载。
- 6,291,457 字节拒绝加载。
- 每帧 allocation count=0。
- 反复更新 1 小时 used bytes 不变化。
- 损坏 count/offset/stride 不造成越界分配。

### 17.7 性能

- 0、1、4、8 活动轴。
- 512、2048、4096、8192 顶点贡献。
- 状态不变与每帧变化。
- 记录 selected sample PSRAM 总读取字节。
- 标量与可选 128 位实现对比。
- 分别记录 RT30 融合、层级顶点更新、渲染。

## 18. 预计源码范围

本轮没有修改以下源码；实施时预计涉及：

### PainterEngine

- `kernel/PX_LiveFramework.h/.c`
- 新增 `kernel/PX_LiveRealtime.h/.c`
- 新增 `kernel/PX_LiveDeviceFormat.h/.c`
- `kernel/PX_Object_Live2D.h/.c`
- 新增 `platform/esp32p4/` 最小平台层

### LiveEditor

- `main.c`
- `PainterEngine_LiveEditorModules_LiveController.*`
- 新增 `PainterEngine_LiveEditorModules_RealtimeController.*`
- 新增 `PainterEngine_LiveEditorModules_RealtimeAxisEditor.*`
- 新增 `PainterEngine_LiveEditorModules_RealtimeBake.*`
- 新增 `PainterEngine_LiveEditorModules_RealtimeMixer.*`
- 新增 `PainterEngine_LiveEditorModules_RealtimePoseAccessor.*`
- 新增 `PainterEngine_LiveEditorModules_ESP32P4Exporter.*`
- `KeyTransform/GlobalRotation/VertexTransform`
- `assets/language.json`

## 19. MVP 验收定义

首个可用版本必须全部满足：

1. 一个轴固定 30 点，设备每次只取其中一个。
2. 运行时源码不存在轴内相邻点混合路径。
3. 用户只制作 K0、K1、K2，Editor 正确 Bake 其余 27 点。
4. 至少眨眼、嘴巴开合、摇头三个轴可选择不同离散样本并按 Q15 权重融合。
5. RT30 与 Timeline 是独立模式，ESP32-P4 实时包不含 Timeline。
6. 支持子图层局部平移和顶点变形。
7. RT30 每帧零动态分配。
8. 模型、RT30 数据、状态和 accumulator 全部来自同一个 6 MiB PSRAM arena。
9. 在未测得更低 high-water 前，现有框架基线固定预留 5 MiB，RT30 全部新增数据固定限制在剩余 1 MiB。
10. Editor 导出前给出逐分类精确内存报告，超限禁止导出。
11. 代表模型剥离 Timeline 后正常显示并运行 RT30。
12. ESP32-P4 实测达到既定 P95 性能目标。
13. 不新增第三方动画、数学、压缩或序列化依赖。

## 20. 官方硬件依据

- ESP32-P4 HP 系统为最高 400 MHz 的双核 32 位 RISC-V，并具有单精度浮点、定制 AI/DSP 和 128 位向量扩展：[ESP32-P4 Datasheet](https://documentation.espressif.com/esp32-p4_datasheet_en.pdf)。
- ESP-IDF 把外部 PSRAM 映射到地址空间并通过缓存访问；大块数据超过缓存工作集后会退回外部 RAM 速度，因此本计划限制每帧选中样本的读取量：[ESP-IDF External RAM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/external-ram.html)。
- ESP-IDF 提供 PSRAM、对齐和 SIMD 能力相关的分配标志，但本计划只在启动时取得一次 PSRAM arena，PainterEngine 热路径不调用 heap API：[ESP-IDF Heap Allocation](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/mem_alloc.html)。

## 21. 第一轮开发边界

第一轮实现：

- 30 点全离散。
- 三关键姿态离线 Bake。
- int16 姿态和 Q15 权重。
- 稀疏顶点绑定。
- 独立 Timeline/RT30 模式。
- 子图层平移、旋转、伸缩、顶点位移。
- 6 MiB PSRAM 精确布局与导出报告。
- 标量 C ESP32-P4 基准。

第一轮不实现：

- 任意形式的轴内运行时插值。
- Timeline + RT30 同时叠加。
- 多维 30×30 样本表。
- 互斥嘴形组和修正轴。
- 摄像头、音频或网络输入采集。
- Panc、纹理切换、Impulse 的 RT30 参数化。
- 第三方数学、动画、序列化或压缩库。

先把“一个轴只取一个点、多轴定点融合、6 MiB PSRAM、零分配”做成不可破坏的基础，再扩展其他能力。
