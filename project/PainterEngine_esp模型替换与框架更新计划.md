# PainterEngine `esp.live` 模型替换与 ESP 框架更新计划

> 文档状态：实施计划，尚未修改任何源码。  
> 编制日期：2026-07-13  
> PC 框架：`D:\live2d\PainterEngine`  
> LiveEditor：`D:\live2d\project\PainterEngine_LiveEditor`  
> ESP32-P4 工程：`D:\重要文件\嵌入式竞赛\aaa\live2d_demo`  
> 目标模型：`D:\live2d\project\esp.live`

## 1. 目标与边界

本计划完成两件事：

1. 用 `esp.live` 替换设备当前从 `/sdcard/release.live` 加载的模型资源，并建立可校验、可回滚的替换流程。
2. 把 PC 端已经实现的 RT30 多轴离散实时动画能力增量合并到 ESP32-P4 的 PainterEngine 分支，使设备可以读取 `esp.live` 尾部的实时轴并按离散采样运行。

约束如下：

- 继续使用 PainterEngine 自有内存池、数据结构和整数融合，不增加第三方动画、序列化或数学库。
- 模型、纹理、RT30 样本、累加器和替换时的文件缓冲均放入 PSRAM；热路径不调用系统堆分配。
- 保留当前游戏路径使用的 5 MiB PainterEngine 模型池，先以实测 high-water 决定是否调整，不因本次移植直接扩大。
- RT30 每个轴固定 30 个离散样本；运行时只读取选中的一个样本，不做相邻样本插值。
- 多轴使用 Q15 权重融合，所有轴参数一次提交、一次求值。
- Timeline 时间动画和 RT30 实时状态是互斥模式，不能让两个系统同时写模型姿态。
- PC 编辑器继续是模型和 RT30 数据的唯一生产端；设备端只加载、校验和运行，不负责 Bake。
- 本文只制定计划，不修改 PC 或 ESP32-P4 代码，也不进行板端测试。

## 2. `esp.live` 已审计基线

### 2.1 文件与基础模型

| 项目 | 实际值 |
| --- | ---: |
| 文件大小 | 798,660 B |
| SHA-256 | `786B18E342F3A7B3E67042822F138824AAAD610D6C46130F58F5C59BBB379B0B` |
| 文件魔术字 | `PainterEngineLiveDBinary` |
| 基础格式版本 | 1 |
| 模型 ID | `esp` |
| 模型尺寸 | 320 × 320 |
| 图层数 | 11 |
| 纹理数 | 12 |
| Timeline 动画数 | 1 |
| 基础 `.live` 主体 | 768,836 B |
| RT30 尾部 | 29,824 B |

12 张纹理在文件中合计包含 736,940 B RGBA8888 像素。最大的纹理为 319×319，其余纹理是局部图层资源。该数值是纹理像素下限，不包含 PainterEngine 对象、向量、顶点、三角形和内存池管理开销。

### 2.2 RT30 轴

| 轴 ID | 中间点 | 中性/默认样本 | 绑定 | 顶点索引 | 单样本读取 | 30 样本 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `left_eye` | 15 | 0 | 2 | 0 | 12 B | 360 B |
| `right_eye` | 15 | 0 | 2 | 0 | 8 B | 240 B |
| `neck` | 15 | 15 | 1 | 7 | 32 B | 960 B |
| `face` | 15 | 15 | 11 | 227 | 908 B | 27,240 B |

量化格式统一为：坐标 Q4、旋转 Q6、缩放 Q14。四轴全部激活时，一次 RT30 求值只读取 960 B 选中样本数据。

轴的实际属性如下：

- `left_eye`：局部平移、局部缩放，作用于两个眼部相关图层。
- `right_eye`：局部平移、局部缩放，作用于两个眼部相关图层。
- `neck`：局部旋转和 7 个顶点偏移，作用于图层 3。
- `face`：11 个图层、227 个顶点的顶点偏移。

当前模型没有名为 `mouth` 的嘴部参数轴。若设备需要独立嘴巴开合，必须先在 LiveEditor 中新建并 Bake 嘴部轴，再重新导出模型；不能在设备端把 `face` 轴直接假定为嘴部轴。

### 2.3 RT30 内存量级

根据当前 RT30 结构：

- 16 个 binding 的运行静态描述约 256 B。
- 234 个顶点索引约 468 B。
- 四轴样本共 28,800 B。
- RT30 动态累加器约 2.4 KiB：12 个顶点边界偏移、11 个图层累加器和约 227 个顶点累加器。
- 不计内存池块头和 `PX_LiveRealtime` 固定元数据时，新增数据约 32 KiB，远低于现有 640 KiB 静态和 128 KiB运行预算。

因此 `esp.live` 的主要内存仍是纹理和基础 PainterEngine 模型，而不是 RT30。

## 3. 当前 PC 与 ESP 框架差异

### 3.1 PC 框架已有能力

PC 端当前已经具备：

- `PX_LiveRealtime.h/.c`：30 点离散轴、Q15 多轴融合、dirty revision、无帧循环分配。
- `.live` v1 主体后的 `RT30` v1 尾部导出、字段级读取、范围和拓扑校验。
- `NEUTRAL`、`TIMELINE`、`REALTIME30` 三种互斥状态。
- 父子层级下的局部平移、局部旋转、局部缩放和顶点偏移传播。
- `PX_LiveEnterRealtime30`、`PX_LiveSetRealtimeAxisSample`、`PX_LiveFindRealtimeAxisById` 等运行接口。
- `PX_LiveDeviceFormat.h/.c` 中可复用的定长小端读取、溢出检查和范围检查。

### 3.2 ESP 分支已有能力

ESP32-P4 分支当前具备：

- 5 MiB PSRAM PainterEngine 内存池。
- 从 `/sdcard/release.live` 整体读取文件并调用 `PX_LiveFrameworkImport`。
- Timeline 播放、动画循环和静态模型回退。
- ESP32-P4 专用最近邻采样、快速像素着色器和 `renderScale`。
- 图层旋转正弦/余弦缓存等设备侧优化。
- 已加载模型释放后复用同一内存池的基础能力。

但它没有：

- `PX_LiveRealtime` 数据结构和求值器。
- RT30 尾部解析和严格校验。
- 三种动画模式仲裁。
- 图层局部平移、旋转、缩放状态。
- 实时轴查询、参数批量提交和权重控制接口。

旧导入器会在 Timeline 主体结束后直接成功返回，`esp.live` 的 RT30 尾部会被静默忽略。因此只替换 SD 卡模型文件，只能加载基础模型和可能兼容的 Timeline，不能得到实时眨眼、摇头或多轴融合。

## 4. 总体技术决策

### 4.1 第一阶段直接使用 `.live + RT30` 尾部

`esp.live` 已经是 PC 编辑器当前输出的兼容格式。第一阶段设备直接读取该格式，不先引入另一个模型转换步骤。这样可以先验证 PC 与 ESP 的离散样本结果一致，减少同时调试“框架移植”和“第二种设备包格式”的风险。

后续如需进一步减少 Timeline 占用，可再从同一模型导出只含基础模型和 RT30 的 `PXR3` 设备包；这属于优化阶段，不阻塞本次模型替换。

### 4.2 采用增量合并，不整份覆盖

禁止直接用 PC 的 `PX_LiveFramework.h/.c` 覆盖 ESP 分支，因为这样会丢失：

- `fastNearestSampling`；
- `renderScale`；
- 快速纹理采样路径；
- 图层旋转正弦/余弦缓存；
- ESP32-P4 已验证的渲染优化。

正确方法是在 ESP 文件上合入 RT30、局部变换、模式仲裁和安全导入逻辑，同时保留上述设备优化。

### 4.3 时间动画与实时模式明确切换

状态规则固定为：

```text
NEUTRAL -> PlayAnimation -> TIMELINE
NEUTRAL/TIMELINE -> EnterRealtime30 -> REALTIME30
REALTIME30 -> LeaveRealtime30 -> NEUTRAL
REALTIME30 -> PlayAnimation -> 先 LeaveRealtime30，再进入 TIMELINE
```

实时状态下不执行 Timeline VM；Timeline 播放时不运行 RT30。退出任一系统都先恢复中性姿态，防止上一个模式的最后状态残留。

## 5. 计划修改范围

以下为未来实施范围，本次没有执行这些修改。

| 文件/模块 | 计划动作 | 注意事项 |
| --- | --- | --- |
| `components/live2d_engine/painterengine/kernel/PX_LiveRealtime.h` | 从 PC 端加入 | 保持定长整数和预算常量一致 |
| `components/live2d_engine/painterengine/kernel/PX_LiveRealtime.c` | 从 PC 端加入 | 热路径不得分配；只读一个离散样本 |
| `PX_LiveDeviceFormat.h/.c` | 加入安全 reader 和范围校验 | 第一阶段可整体引入，后续再裁剪未用设备包逻辑 |
| ESP `PX_LiveFramework.h` | 增量合并模式、RT30、局部变换字段和 API | 保留 ESP 渲染字段与三角函数缓存 |
| ESP `PX_LiveFramework.c` | 合并初始化、Reset/Free、更新、层级传播、RT30 尾部导入 | 保留快速采样与 `renderScale` 路径 |
| `live2d_engine.h/.c` | 增加实时轴封装、批量提交和安全替换状态机 | 业务层不直接访问 `PX_LiveFramework` 内部 |
| `components/live2d_engine/CMakeLists.txt` | 编译新增源文件 | 不增加外部组件依赖 |
| `game_services.c` | 使用模型资源配置和替换接口 | 初期仍可保持 `/sdcard/release.live` |
| `test_live2d.c` | 增加轴、内存、状态切换测试 | 测试池与正式 5 MiB 池必须分开记录 |

PC 动画帧的局部变换字段复用了旧 payload 的 `reserve[32]` 区域，payload 总大小没有改变。ESP 合并时必须保持该二进制布局不变，只把 reserve 中对应位置解释为 `localTranslation`、`localRotation`、`localScaleOffset` 和 magic。不能改变 `.live` v1 主体结构大小。

## 6. 框架更新实施步骤

### 阶段 A：冻结基线

1. 记录 ESP 当前 `PX_LiveFramework`、`PX_MemoryPool`、`live2d_engine` 和 CMake 文件的哈希。
2. 记录现有 `/sdcard/release.live` 的文件大小、SHA-256、加载耗时、5 MiB 池剩余量和渲染 FPS。
3. 固定 `esp.live` 的上述审计值作为移植验收输入。
4. 不先改渲染器；先证明同一个模型在框架更新前后基础画面一致。

### 阶段 B：移植 RT30 独立求值器

1. 加入 `PX_LiveRealtime.h/.c`。
2. 加入小端 reader、加法/乘法溢出检查、对齐和范围检查。
3. CMake 增加源文件，保持 `-O2`，不引入 C++、STL 或第三方库。
4. 在 ESP 上保持以下预算：最多 32 轴、每轴 30 样本、静态 RT30 640 KiB、运行累加器 128 KiB。
5. 先通过纯内存测试验证离散取样和 Q15 融合，再接入框架层级。

### 阶段 C：增量合并框架状态与层级

1. 在 `PX_LiveFramework` 中加入 `animationMode` 和 `realtime`。
2. 在 `PX_LiveLayer` 中加入局部平移、局部旋转、局部缩放的 begin/current/end 状态，同时保留 ESP 的 sin/cos 缓存。
3. `Initialize`、`Import`、`Reset`、`Free` 和 mirror/clone 路径都初始化或释放 RT30，避免只改一个构造路径。
4. 将更新拆成明确的 Timeline、Realtime30 和 Neutral 分支。
5. 合入父子层级规则：父层变换影响后代；子层局部变换只影响自己和自己的后代，不反向影响父层。
6. 顶点、关键点、旋转、缩放、局部变换均保持模型/图层局部坐标；ESP 渲染缩放只在最终屏幕投影阶段生效。
7. `PlayAnimation` 在进入 Timeline 前退出实时模式；`EnterRealtime30` 在求值前 Reset Timeline 状态。

### 阶段 D：导入 `RT30` v1 尾部

1. 基础 `.live` 主体仍按现有 v1 导入。
2. 主体末尾如无剩余字节，视为合法旧模型，轴数为 0。
3. 有剩余字节时，仅接受合法 `RT30` magic、版本 1、24 B header、68 B axis entry。
4. 对轴 ID、哈希、样本数、默认点、中间点、量化格式、binding、顶点索引、offset、stride 和 chunk 总长度逐项校验。
5. 先完整校验，再向 5 MiB 模型池深拷贝，禁止让运行时指针直接指向即将释放的文件缓冲。
6. 四轴全部安装后一次性分配约 2.4 KiB runtime block。
7. 任一轴失败时释放已导入内容并 Reset 内存池，不能留下半个模型。

### 阶段 E：保留 ESP 渲染优化

合并完成后逐项确认：

- 最近邻采样仍走 ESP 快速路径。
- 自定义快速像素着色器仍被 `live2d_engine_load` 安装。
- `renderScale` 仍限制在 0.1～1.0。
- 新局部变换和 RT30 顶点结果先在模型空间完成，再进入原有 `renderScale` 投影。
- 旋转 sin/cos 缓存由 Timeline 和 RT30 两种模式统一更新，避免 RT30 绕过缓存。
- 参数不变时 `PX_LiveRealtimeUpdate` 根据 dirty revision 直接返回，不重复遍历 30 份样本。

## 7. `live2d_engine` 对外实时接口

业务层只使用稳定封装，不直接修改 `engine->live.realtime.axes`。建议接口为：

```c
int  live2d_engine_get_realtime_axis_count(...);
int  live2d_engine_find_realtime_axis(..., const char *id);
bool live2d_engine_enter_realtime(...);
bool live2d_engine_set_axis_sample(..., int handle, uint8_t sample, uint16_t weight_q15);
bool live2d_engine_set_axes_batch(..., const live2d_axis_state_t *states, size_t count);
void live2d_engine_reset_realtime(...);
void live2d_engine_leave_realtime(...);
```

使用规则：

- 初始化时按字符串查找一次 `left_eye`、`right_eye`、`neck`、`face`，之后只缓存整数 handle。
- 高频循环不得重复哈希或比较字符串。
- `set_axes_batch` 先写所有 sample/weight，再触发一次求值；不能每写一个轴就刷新整套模型。
- sample 必须是 0～29；weight 必须是 0～32767。
- 参数输入可以按 30 Hz 更新；渲染帧率独立。参数没有变化时复用上一姿态。
- 自动眨眼可让左右眼轴按离散序列从默认点走到闭眼端再返回，具体开/闭方向必须先在 PC 预览确认，不能仅按轴名猜测。
- 摇头使用 `neck`，默认样本 15 是中性位置。
- `face` 每次读取 908 B、影响 227 个顶点，应只在表情实际变化时更新。

## 8. 模型替换方案

### 8.1 最小资源替换

当前正式路径硬编码为 `/sdcard/release.live`，因此最小部署方式是把 `esp.live` 复制到 SD 卡并命名为 `release.live`。固件不内嵌该资源，`ui_sd_seed` 也不会写回模型文件。

推荐文件布局：

```text
/sdcard/release.live         当前生效模型
/sdcard/release.live.bak     上一份已验证模型
/sdcard/release.live.tmp     正在传输、尚未生效的模型
```

替换流程：

1. 写入 `.tmp`，不要覆盖正在使用的正式文件。
2. 检查文件大小、24 B magic、base version、主体计数和 RT30 chunk 全部边界。
3. 对 `.tmp` 计算 SHA-256，并与下发清单比较。
4. 停止新的 Live2D 渲染请求，等待当前帧退出。
5. 将正式文件改名为 `.bak`，再把 `.tmp` 改名为正式文件。
6. 使用同一引擎池加载新文件。
7. 导入、RT30 准备或首帧失败时恢复 `.bak` 并重新加载。
8. 新模型通过连续帧检查后才删除旧备份。

### 8.2 运行时模型替换状态机

```text
IDLE
  -> VALIDATE_FILE
  -> QUIESCE_RENDER
  -> UNLOAD_OLD
  -> RESET_ARENA
  -> READ_NEW_TO_PSRAM
  -> IMPORT_AND_PREPARE_RT30
  -> COMMIT
  -> IDLE

任一步失败 -> RESET_ARENA -> LOAD_BACKUP -> ROLLBACK/ERROR
```

只保留一个 5 MiB PainterEngine arena，不同时保留两套解包模型。回滚依靠 SD 卡旧文件重新导入，而不是在 PSRAM 中双开两个 5 MiB 模型池。

当前 `live2d_engine_load` 在加载新模型前就释放旧模型；实施时要在释放旧模型前完成文件级格式校验。否则损坏文件会直接让当前模型丢失。

## 9. 内存和性能预算

### 9.1 常驻 PSRAM

| 项目 | 预算/已知值 |
| --- | ---: |
| PainterEngine 模型 arena | 5,242,880 B，保持当前配置 |
| `esp.live` RGBA 纹理像素下限 | 736,940 B |
| RT30 静态样本、binding、索引 | 约 29.5 KiB，加池管理开销 |
| RT30 runtime block | 约 2.4 KiB |
| 引擎上下文 | 小块，计划改为 PSRAM 分配 |

`PX_LiveFramework` 合入固定 32 轴元数据后体积会上升。若严格执行“模型相关数据全部进 PSRAM”，`live2d_engine_t` 不再用普通 `calloc`，而用带 `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` 的清零分配；控制句柄和任务栈才留在内部 SRAM。

### 9.2 加载峰值

当前 `sys_storage_read_file` 会先把完整模型读入独立 PSRAM 缓冲，再由 PainterEngine 深拷贝到 5 MiB arena。加载 `esp.live` 时至少存在：

```text
5 MiB 已预留模型 arena
+ 798,660 B 临时文件缓冲
+ live2d_engine/PainterEngine 固定上下文
+ 系统其他 PSRAM 用户
```

文件缓冲在导入成功后立即释放。第一阶段保留此流程以降低改动风险；若全系统最大连续 PSRAM 块不足，再规划流式导入，不能一开始同时改格式、导入器和实时框架。

### 9.3 每次参数更新

四轴全开时的上限工作量：

- 从 PSRAM 连续读取 960 B 选中样本；
- 处理 16 个 binding；
- 累加最多 234 个绑定顶点值；
- 写回 11 个图层和约 227 个模型顶点；
- 不扫描其余 29 个样本；
- 不进行插值；
- 不进行堆分配。

这条路径应先保持标量 C；只有板端 profile 证明其成为瓶颈后，才考虑 ESP32-P4 向量扩展。

## 10. 验证计划

### 10.1 PC 参考输出

使用当前正式 LiveEditor/预览生成参考表：

- 四轴分别在样本 0、15、29、默认样本下的图层关键点和顶点结果。
- 左右眼同时 100%、脖子 100%、脸部 100% 的组合结果。
- 50% Q15 权重和 0 权重结果。
- Timeline -> RT30 -> Timeline 模式切换后的中性姿态。

参考数据记录模型空间值，不记录窗口坐标、预览缩放或 `refer_x/refer_y`。

### 10.2 主机侧格式测试

- 正确导入 `esp.live`，轴数必须为 4。
- 校验四个 ID、默认样本、binding 数、stride 和 sampleBytes 与本文一致。
- 截断 header、axis table、binding、vertex index、sample data 时必须拒绝。
- 重复轴 ID、错误哈希、越界图层、越界顶点、错误 stride、错误 chunkSize 必须拒绝。
- 没有 RT30 尾部的旧 `.live` 仍可加载，轴数为 0。

### 10.3 ESP 编译与设备验收（未来执行）

- ESP-IDF 全量编译通过，无新增未解析符号。
- 5 MiB 正式池加载 `esp.live` 成功并打印 pool used/high-water/free。
- 首帧与 PC 中性模型一致。
- 左眼、右眼、脖子、脸部四轴分别在 0/15/29 结果与 PC 参考一致。
- 父节点旋转继续影响子节点；眼睛局部变换不反向影响脸部父层。
- 四轴并行融合时不出现状态覆盖。
- 参数不变时没有额外 RT30 求值和分配。
- Timeline 和 RT30 来回切换不保留上一模式的最后姿态。
- 替换损坏模型时自动回滚旧模型。
- 连续模型替换后 arena free/high-water 可重复，不发生逐次泄漏。
- 记录平均、P95 和最大参数求值时间，以及整体渲染 FPS。

## 11. 风险与规避

| 风险 | 后果 | 规避方式 |
| --- | --- | --- |
| 整份覆盖 ESP Framework | 丢失最近邻、缩放和旋转缓存优化 | 只做增量合并并逐项回归 |
| 只换模型不更新框架 | RT30 尾部被静默忽略 | 启动日志必须报告轴数和四个轴 ID |
| 忽略局部变换字段 | 眨眼或独立图层变换错误 | 合并 reserve 中的 local transform 解释和层级传播 |
| Timeline 与 RT30 同时运行 | 姿态互相覆盖、跳变 | 使用互斥 animationMode 状态机 |
| 高频按字符串查轴 | 不必要 CPU 和 PSRAM 访问 | 初始化缓存整数 handle |
| 每轴设置后立即整模更新 | 多轴一次输入重复求值 | 使用批量提交，一批只更新一次 |
| 新模型先销毁旧模型后才校验 | 损坏文件导致无模型可用 | 文件级预校验、双文件、失败回滚 |
| 同时保留两套解包模型 | 超过连续 PSRAM 能力 | 单 5 MiB arena，回滚时重新导入备份 |
| 误把 `face` 当嘴部轴 | 参数语义错误 | 轴 ID 固定映射；需要嘴部时重新制作模型 |
| 将屏幕缩放写入 RT30 | PC/设备坐标不一致 | RT30 永远保存模型/图层局部坐标 |
| 直接信任 `.live` 原始 C 布局 | 损坏或跨架构数据越界 | RT30 尾部字段级读取；基础 v1 继续做尺寸和范围审计 |

## 12. 交付顺序与完成标准

建议按以下顺序实施：

1. **格式层**：ESP 工程可识别并严格校验 `esp.live` 的四轴 RT30 尾部。
2. **求值层**：纯内存测试通过 30 点离散取样和多轴 Q15 融合。
3. **框架层**：局部变换、父子传播、顶点更新及模式仲裁与 PC 一致。
4. **封装层**：`live2d_engine` 提供整数 handle 和批量参数 API。
5. **资源层**：`esp.live` 以 `/sdcard/release.live` 部署，具备 `.tmp/.bak` 校验和回滚。
6. **性能层**：确认 5 MiB arena 足够，RT30 更新无帧循环分配，记录 high-water 和 P95。
7. **可选优化**：在直接 `.live + RT30` 完全正确后，再决定是否导出剥离 Timeline 的 `PXR3` 设备包。

任务完成判定：

- `esp.live` 在 ESP 框架中加载后明确报告 4 个轴，而不是静默忽略尾部。
- 设备离散样本和多轴融合结果与 PC 参考模型空间数据一致。
- 当前 ESP 快速渲染路径没有回退。
- 5 MiB 模型池加载、运行和反复替换均无泄漏。
- 模型替换失败可恢复旧模型。
- 整个运行热路径无新增第三方库、无插值、无动态分配。

