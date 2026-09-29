# PainterEngine / LiveEditor 新对话交接上下文

> 生成日期：2026-07-14  
> 用途：在新的 Codex 对话，尤其是 WSL 环境中，继续优化 PainterEngine、LiveEditor 和 RT30 框架。  
> 状态原则：本文区分“用户运行确认”“确定性测试通过”“仅编译成功”和“计划未实施”，不能把编译成功写成运行验证通过。

## 0. 新对话的第一条指令

在新对话中发送以下内容：

```text
这是 PainterEngine / LiveEditor 的续接任务。

请先完整读取：
/mnt/d/live2d/project/PainterEngine_新对话交接上下文.md
/mnt/d/live2d/project/PainterEngine_多轴离散参数动画扩展计划.md
/mnt/d/live2d/project/PainterEngine_esp模型替换与框架更新计划.md

如果当前环境可以访问 Windows 用户目录，再读取：
/mnt/c/Users/20756/.codex/skills/painterengine-liveeditor-maintenance/references/iteration-log.md
/mnt/c/Users/20756/.codex/skills/painterengine-liveeditor-maintenance/references/coordinate-debugging.md

先核对当前源码、文件哈希、运行进程和构建环境，再继续工作。不要重新实现已经完成的功能，不要把 compiled only 当成 runtime verified。
```

若新对话不能访问 Windows 挂载目录，上传本文件以及两份计划文档。

## 1. 用户的长期目标

项目目标是把 PainterEngine LiveEditor 扩展为可制作二维模型、Timeline 动画和多轴实时参数动画的编辑器，并使核心运行时方便移植到 ESP32-P4。

核心要求：

1. 编辑器支持可靠的图像缩放和平移，缩放时模型、网格、控制点和鼠标编辑不能错位。
2. Timeline 支持位移、旋转、缩放、顶点、全局旋转、纹理、冲量和 Panc 等工具。
3. 图层变换的方向必须正确：父层影响子树，子层变换不能反向影响父层。
4. 实时动画每个轴仍保存 30 个离散样本，整数档位 API 保留。自 2026-09-28 方案起，运行时另支持连续位置 `q∈[0,29]`：`i=floor(q)`，`t=q-i`，位移、旋转、缩放和顶点按 `D(q)=(1-t)D[i]+tD[i+1]` 插值。`q=29` 只读第 29 档，`t=0` 只读一档。权重在插值之后应用。纹理和可见性保持离散，不做索引插值。不要再把“运行时禁止邻近档位插值”写回实现。
5. 编辑时只制作 K0、KM、K29 三个关键姿态，其他 27 个档位只在桌面 Bake 阶段插值生成。
6. 多轴之间使用 Q15 权重融合；Timeline 和 RT30 实时状态是两个互斥系统。
7. 设备端运行强调固定内存、低分配、少依赖和连续内存访问。用户已有 ESP32-P4 示例模型使用 5 MiB PainterEngine 池成功运行的经验。
8. PC 端先完成编辑、格式和运行框架；ESP 工程用于移植参考，除非用户明确要求，否则不要自动上板测试。
9. Windows 任务的验收边界通常是 MSVC x64 编译成功。不要自行反复启动编辑器；用户负责可视化验收。
10. 编辑器正在运行时不要强制结束进程或覆盖可能含未保存工作的正式 EXE；应先编译独立输出名。

## 2. 工程路径映射

| 内容 | Windows 路径 | WSL 路径 |
| --- | --- | --- |
| PainterEngine 核心 | `D:\live2d\PainterEngine` | `/mnt/d/live2d/PainterEngine` |
| LiveEditor | `D:\live2d\project\PainterEngine_LiveEditor` | `/mnt/d/live2d/project/PainterEngine_LiveEditor` |
| 旧 make 包 | `D:\live2d\PainterEngine_make` | `/mnt/d/live2d/PainterEngine_make` |
| RT30 总体计划 | `D:\live2d\project\PainterEngine_多轴离散参数动画扩展计划.md` | `/mnt/d/live2d/project/PainterEngine_多轴离散参数动画扩展计划.md` |
| ESP 模型/框架计划 | `D:\live2d\project\PainterEngine_esp模型替换与框架更新计划.md` | `/mnt/d/live2d/project/PainterEngine_esp模型替换与框架更新计划.md` |
| 目标模型 | `D:\live2d\project\esp.live` | `/mnt/d/live2d/project/esp.live` |
| ESP32-P4 工程 | `D:\重要文件\嵌入式竞赛\aaa\live2d_demo` | `/mnt/d/重要文件/嵌入式竞赛/aaa/live2d_demo` |
| 维护技能 | `C:\Users\20756\.codex\skills\painterengine-liveeditor-maintenance` | `/mnt/c/Users/20756/.codex/skills/painterengine-liveeditor-maintenance` |

LiveEditor 的 Windows 构建脚本：

```text
D:\live2d\project\PainterEngine_LiveEditor\build_msvc.bat
```

它使用 `vswhere` 找到 Visual Studio、调用 `vcvars64.bat`、编译 LiveEditor、PainterEngine core/kernel/runtime 和 Windows 平台源文件，并链接 Windows 库。

## 3. 当前交接时的工作区状态

### 3.1 正式编译产物

```text
路径：D:\live2d\project\PainterEngine_LiveEditor\PainterEngine.exe
大小：2,823,168 B
时间：2026-07-13 21:06:19
架构：PE x64 / machine 0x8664
SHA-256：D562E72D1F9BF5DF25C5D166029BD8AE777203DB49B8AF9ECD9E922433C77071
```

该正式版的完整 MSVC x64 编译和链接退出码为 0。最后的编译告警仍是 PainterEngine 核心中已有的枚举转换告警，新预览源没有新增错误或告警。

交接检查时没有正在运行的 `PainterEngine*` 进程。

### 3.2 源码管理状态

LiveEditor 目录此前执行 `git status` 返回“not a git repository”。在没有重新确认前，把现有文件视为用户工作区和唯一真源：

- 不得使用 `git reset --hard`、`git checkout --` 等回退命令。
- 不得用另一份 PainterEngine 整目录覆盖当前目录。
- WSL 优化前建议先建立明确的版本控制或只读快照，但这需要用户授权。
- 根目录存在大量 `.obj` 和历史独立 EXE，它们是构建产物，不是源码真源。

### 3.3 两份计划文档

```text
PainterEngine_多轴离散参数动画扩展计划.md
大小：31,714 B
SHA-256：790A92AAB959725798A7FCC8B0FE1303EA0017EFB5AD30B6E4BEB64C65A2658F

PainterEngine_esp模型替换与框架更新计划.md
大小：20,541 B
SHA-256：30A9264F93AFAC4B32786C44939D20E389AC13BEEFA76C0BF35C311DC7164EC4
```

## 4. 已完成的主要框架工作

### 4.1 缩放与坐标系统

已完成：

- `PX_LiveFramework` 增加 `view_scale` 和 `view_revision`。
- 视图缩放范围为 25%～800%，鼠标位置作为缩放锚点。
- 渲染已从“先画完整离屏模型再缩放”改为直接按缩放投影到目标 surface。
- 模型顶点、关键点、动画 payload、纹理偏移、冲量和 Panc 始终保持模型空间，不写入屏幕缩放值。
- LiveEditor 增加共享视图转换头 `PainterEngine_LiveEditorModules_View.h`。
- BuildMesh、Setkey、Linkkey、VertexTransform、Impulse、GlobalRotation、KeyTransform、Panc 等输入/显示路径进行过坐标审计。

必须保持的公式：

```text
screen = refer + model * view_scale
model  = (screen - refer) / view_scale
```

运行验证情况：导入模型空白问题曾被实际确认并修复；后续大范围坐标工具主要是“编译成功，用户分阶段验收”，不能宣称完整回归矩阵全部通过。

### 4.2 Timeline 局部变换

动画帧 payload 的旧 `reserve[32]` 空间被复用于：

- `localTranslation`
- `localRotation`
- `localScaleOffset`
- `localTransformMagic`

payload 总大小保持不变，旧 `.live` 帧布局兼容。

语义：

- 局部变换作用于所选图层及其所有后代。
- 局部变换绝不向上修改祖先图层、祖先网格或父骨长度。
- 父层局部变换通过视觉仿射链传递到子树。
- 叶子眼睛的局部位移/旋转/缩放不会让脸部父层反向变化。
- Global Rotation 仍是显式层级变换，会按需要写入整个后代分支。

渲染、关键点显示、连接线、适配器和拖动逆变换必须使用同一视觉祖先链。

### 4.3 RT30 离散实时运行时

核心新增文件：

```text
D:\live2d\PainterEngine\kernel\PX_LiveRealtime.h
D:\live2d\PainterEngine\kernel\PX_LiveRealtime.c
D:\live2d\PainterEngine\kernel\PX_LiveDeviceFormat.h
D:\live2d\PainterEngine\kernel\PX_LiveDeviceFormat.c
```

主要规则：

- 每轴固定 30 样本。
- 桌面 Bake 对 K0→KM、KM→K29 两段插值。
- 整数档位 API 仍读取 `samples + sampleIndex * sampleStride`。
- 2026-09-28 起连续位置 API 在相邻两档之间插值；旋转沿烘焙展开的区间，不一律取最短路径。参数过渡按经过时间，不按渲染次数加档。详见 `\\wsl.localhost\Ubuntu\home\ubuntu\live2d\doc\windows-painterengine-live2d-optimization-plan.md`。
- 多轴连续属性使用 Q15/int32 累加。
- 纹理等分类属性使用最高权重获胜，轴顺序作为同权重稳定裁决。
- RT30 热路径无帧循环内存分配。
- 静态 RT30 预算 640 KiB，运行累加器预算 128 KiB。
- 框架状态拆分为 `NEUTRAL`、`TIMELINE`、`REALTIME30`。
- `.live` v1 主体后可附加经过校验的 `RT30` v1 trailer；旧模型仍可导入。

早期确定性测试曾完成 2268 项检查，覆盖 Bake、离散映射、Q15 多轴、层级局部坐标、clone/free、损坏格式和重复更新无分配。但后续新工具增加后仍需扩充并重新运行完整测试。

### 4.4 LiveEditor 实时编辑界面

实时右侧栏已经支持：

- 新建、删除、选择、刷新轴。
- K0、KM、K29 三点编辑。
- M 位置和默认点规则。
- 复制默认姿态。
- 生成 30 档。
- 0～29 严格离散预览与权重。
- 选择图层和底部画布工具。

实时底部工具包括：

1. 局部位移
2. 局部旋转
3. 局部缩放
4. 全局旋转
5. 纹理切换
6. 顶点编辑
7. 冲量
8. Panc/位置跟踪

UI 使用原编辑器白底、黑框、黑字和标准 PainterEngine 控件。工具按钮保持可接收 hover/click，默认关键点不可编辑时通过状态守卫拒绝写入，而不是让整个按钮失去事件。

### 4.5 编辑状态隔离

必须区分：

- 三点 authoring key 的 `selectedKeySlot`。
- 0～29 runtime preview 的 `sampleIndex`。
- 选中轴单轴预览。
- 多轴融合预览。
- Timeline/Neutral 状态。

已修复的典型问题：

- 在 K29 使用工具后切到档位 1，再点同工具会跳回 K29。
- 切换轴后保留上一轴的最后脖子姿态。
- 工具在不同轴/关键点之间保留选择、锚点或 Panc 基线。

单轴编辑器预览会清空所有运行时轴权重，只激活当前选中轴；部署运行时的公共多轴 API 仍允许所有轴同时融合。

### 4.6 独立多轴预览窗口

顶部 `预览` 菜单采用独立进程/窗口：

1. 父编辑器把当前内存模型导出到唯一临时 `.live` 快照。
2. 启动同一 EXE，命令行为 `--rt30-preview="snapshot"`。
3. 子进程拥有自己的 runtime、framework、pose accessor、UI 池和窗口。
4. 子进程导入后删除临时快照。

该边界避免编辑器和预览共享焦点、对象树、模型状态或屏幕坐标。

预览修复历史：

- 启动失败现在报告精确阶段，并在原生窗口创建前使用 `ExitProcess`，不再留下空白窗口。
- 子预览使用轻量 pose accessor，不展开桌面三点密集 authoring 缓冲。
- UI 使用独立 8 MiB 桌面池，不与模型池竞争。
- `PX_ObjectCreateEx(..., size=0)` 必定返回空；轴面板和行已改用 `PX_ObjectCreate` 加显式 render function。
- 面板从右下 424×354 改为左下 360×250，行高 68。
- 可用轴进入预览时从默认/中性样本开始，权重为 100%。
- 预览 update 阶段调用 `PX_LiveFrameworkUpdate`，之后 `RenderCurrent` 只负责绘制。

## 5. 最新用户验证结果与剩余回归项

用户最后一次运行反馈是：

1. 多轴预览面板太大且挡住模型，需要放到左下并缩小。
2. 调整参数轴时模型完全不动。

源码已经针对这两个问题修改并成功编译正式版：

- 面板已变为左下 360×250。
- 进入/复位时有效轴使用默认样本和 100% 权重。
- 预览更新调用 `PX_LiveFrameworkUpdate(plive, elapsed)`，补上 RT30 相对值到关键点/网格物理状态的传播。

用户随后明确确认该最终修复“验证过”。因此以下两项应标记为 **user runtime verified**：

- 多轴预览面板已经缩小并显示在左下角。
- 拖动参数轴后模型能够实际变化，不再是只有滑条变化、模型不动。

没有被单独确认的扩展回归项仍建议新对话检查：

- 调整权重能衰减或关闭单轴贡献。
- 左右眼、脖子和脸部轴可同时融合。
- 窗口尺寸变化后面板仍锚定左下。
- 关闭子预览不影响主编辑器。

不要把上述未单独确认的扩展检查自动写成已验证。

## 6. `esp.live` 与 ESP32-P4 状态

`esp.live` 已做只读结构审计：

```text
路径：D:\live2d\project\esp.live
大小：798,660 B
SHA-256：786B18E342F3A7B3E67042822F138824AAAD610D6C46130F58F5C59BBB379B0B
模型 ID：esp
画布：320×320
图层：11
纹理：12
Timeline 动画：1
RT30 trailer：29,824 B
RT30 轴：4
```

四轴：

| ID | 默认样本 | binding | 顶点索引 | 单次选中样本读取 |
| --- | ---: | ---: | ---: | ---: |
| `left_eye` | 0 | 2 | 0 | 12 B |
| `right_eye` | 0 | 2 | 0 | 8 B |
| `neck` | 15 | 1 | 7 | 32 B |
| `face` | 15 | 11 | 227 | 908 B |

四轴同时激活只读取 960 B 选中样本。当前模型没有独立 `mouth` 轴。

ESP 工程当前状态：

- 正式游戏从 `/sdcard/release.live` 读取模型。
- `game_services.c` 使用 5 MiB PainterEngine PSRAM 池。
- ESP 分支保留 `fastNearestSampling`、`renderScale`、快速像素着色器和旋转缓存。
- ESP 分支目前没有 `PX_LiveRealtime` 和 RT30 trailer 解析，因此只替换文件会静默忽略实时轴。
- 不得用 PC `PX_LiveFramework.*` 整份覆盖 ESP 版本；必须增量合并并保留设备优化。

详细实施顺序见：

```text
D:\live2d\project\PainterEngine_esp模型替换与框架更新计划.md
```

该计划尚未实施，ESP 工程没有因该计划被修改或编译。

## 7. 必须保持的技术不变量

### 7.1 坐标域

- 顶点、关键点、纹理偏移、动画 payload、RT30 样本、冲量和 Panc：模型/图层局部空间。
- 鼠标、控件、提示、TransformAdapter、窗口布局、`refer_x/y`：屏幕空间。
- `view_scale` 只用于投影和输入逆投影，不能写入模型或导出文件。

### 7.2 层级方向

- 父层变换影响该父层和全部后代。
- 子层变换只影响子层和它自己的后代。
- 子层局部变换不能修改父层网格、父关键点或父骨长度。
- Global Rotation 与独立局部旋转是不同工具，不能混为一个 property。

### 7.3 RT30

- 每轴仍保存 30 个样本；Bake 仍只在桌面完成。
- 整数档位路径保持离散读取。连续位置路径按 2026-09-28 方案在相邻档之间插值，不要改回“运行时禁止插值”。
- 时间轴与 RT30 仍然互斥。眼睛轴方向以具体模型核对，不能假定 0 是闭、29 是睁。
- 多轴只在轴之间做 Q15 融合。
- 高频路径缓存整数 handle，不能每帧按字符串查轴。
- 参数批量写完后只求值一次。
- 热路径不分配内存。

### 7.4 更新与渲染

- `PX_LiveFrameworkUpdate` 推进 Timeline/RT30 和层级/顶点物理状态。
- `PX_LiveFrameworkRenderCurrent` 不推进状态，只画当前结果。
- 专用 viewport 如果使用 `RenderCurrent`，必须明确拥有 update 步骤。

### 7.5 PainterEngine 对象 API

当前 `PX_ObjectCreateEx` 只有 `size != 0` 才返回对象。无 descriptor 的自定义对象必须使用：

```c
PX_ObjectCreate(...);
PX_ObjectSetRenderFunction(...);
PX_ObjectSetUpdateFunction(...);
```

不要再次用 `PX_ObjectCreateEx(..., NULL, 0)`。

## 8. 关键源码文件

### 8.1 核心

```text
D:\live2d\PainterEngine\kernel\PX_LiveFramework.h
D:\live2d\PainterEngine\kernel\PX_LiveFramework.c
D:\live2d\PainterEngine\kernel\PX_LiveRealtime.h
D:\live2d\PainterEngine\kernel\PX_LiveRealtime.c
D:\live2d\PainterEngine\kernel\PX_LiveDeviceFormat.h
D:\live2d\PainterEngine\kernel\PX_LiveDeviceFormat.c
D:\live2d\PainterEngine\core\PX_MemoryPool.h
D:\live2d\PainterEngine\core\PX_MemoryPool.c
D:\live2d\PainterEngine\kernel\PX_Object.c
```

### 8.2 LiveEditor

```text
main.c
PainterEngine_LiveEditorModules_View.h
PainterEngine_LiveEditorModules_Display.c
PainterEngine_LiveEditorModules_AnimationController.c
PainterEngine_LiveEditorModules_KeyTransform.c
PainterEngine_LiveEditorModules_GlobalRotation.c
PainterEngine_LiveEditorModules_VertexTransform.c
PainterEngine_LiveEditorModules_Panc.c

PainterEngine_LiveEditorModules_RealtimeController.*
PainterEngine_LiveEditorModules_RealtimeAxisEditor.*
PainterEngine_LiveEditorModules_RealtimeAnimationController.*
PainterEngine_LiveEditorModules_RealtimePoseAccessor.*
PainterEngine_LiveEditorModules_RealtimeMixer.*
PainterEngine_LiveEditorModules_RealtimeTransform.*
PainterEngine_LiveEditorModules_RealtimeVertexTransform.*
PainterEngine_LiveEditorModules_RealtimeTexture.*
PainterEngine_LiveEditorModules_RealtimeImpulse.*
PainterEngine_LiveEditorModules_RealtimePanc.*
PainterEngine_LiveEditorModules_RealtimeBlendPreview.*

assets/language.json
tests/rt30_tests.c
build_msvc.bat
```

## 9. Windows 构建与操作约束

正式构建：

```bat
cd /d D:\live2d\project\PainterEngine_LiveEditor
build_msvc.bat
```

正式 EXE 被占用时：

```bat
cd /d D:\live2d\project\PainterEngine_LiveEditor
set OUTPUT_NAME=PainterEngine_wsl_check.exe&& build_msvc.bat
```

从 WSL 调用 Windows MSVC：

```bash
cmd.exe /c "cd /d D:\live2d\project\PainterEngine_LiveEditor && build_msvc.bat"
```

若正式 EXE 正在运行，先指定独立输出名，不结束用户进程：

```bash
cmd.exe /c "cd /d D:\live2d\project\PainterEngine_LiveEditor && set OUTPUT_NAME=PainterEngine_wsl_check.exe&& build_msvc.bat"
```

不要自行打开编译产物。用户曾明确表示验收边界只需编译成功，由用户运行检查。

## 10. WSL 工作方式建议

### 10.1 真源与性能

当前真源位于 `/mnt/d`。直接在 `/mnt/d` 构建通常比 WSL ext4 慢，但最不容易产生双份源码漂移。

在没有 Git 仓库前建议：

- 先直接编辑 `/mnt/d/live2d/...`。
- Linux 构建产物放到单独目录，例如 `/tmp/painterengine-build` 或 `~/build/painterengine`。
- 不在源码目录生成与 Windows 同名 `.obj`。
- 不用 `rsync --delete` 从 WSL ext4 覆盖 D 盘。
- 若需要把源码复制到 `~/src` 提升编译速度，先建立 Git/补丁同步流程并得到用户授权。

### 10.2 Linux 与 Windows 验证分工

WSL 适合：

- 编译 PainterEngine core/kernel 的无窗口测试。
- 使用 GCC/Clang 的 `-Wall -Wextra -Wconversion`。
- 使用 ASan、UBSan、LSan 检查导入、RT30、内存池和层级求值。
- 运行 deterministic RT30/格式测试。
- 做性能基准和 profile。

Windows MSVC 仍负责：

- LiveEditor 全量 Windows GUI 编译。
- Direct2D/窗口/输入/原生预览进程链接。
- 用户最终可视化验收。

Linux 编译通过不能替代最终 MSVC x64 编译。

### 10.3 换行、大小写和路径

- Windows 文件可能是 CRLF；不要做与功能无关的全文件换行重写。
- WSL 文件系统大小写敏感，include 文件名大小写必须与磁盘一致。
- 不把反斜杠路径写进跨平台核心代码。
- 新的测试构建应从相对路径或显式根变量取源文件。
- 中文路径可以访问，但脚本必须正确引用；尽量不要拼接未加引号的路径。

## 11. 建议的框架优化点

下面按优先级排列，建议先完成可测试性，再做结构和性能改动。

### P0-1：建立 WSL 无窗口核心构建与自动测试

问题：当前 Windows `build_msvc.bat` 每次编译全部源文件，RT30 测试也存在多个手工编译产物，难以稳定复现和增量运行。

建议：

- 为 `core + PX_LiveFramework + PX_LiveRealtime + PX_LiveDeviceFormat` 建立独立 CMake/Ninja 测试目标。
- 将 `tests/rt30_tests.c` 拆成格式、Bake、融合、层级、持久化和错误输入测试。
- 增加 `gcc-debug`、`clang-asan-ubsan`、`release-benchmark` 三个 preset。
- Windows 最终仍运行 `build_msvc.bat`，不替换现有正式构建。

完成标准：一次命令能从空 build 目录编译并执行全部核心测试；ASan/UBSan 无错误；测试不依赖 GUI 和 Windows API。

这是最优先项，因为后面的优化都需要确定性回归保护。

### P0-2：把视觉祖先链变换缓存为每层 world affine

问题：当前局部变换、子树继承、关键点投影和编辑器逆变换已经统一了语义，但多个路径仍可能分别遍历祖先链。层级深、顶点多时会重复计算，也容易出现渲染和编辑使用不同公式。

建议：

- 每次 pose 变化后按拓扑顺序为每层计算一次 `local affine` 和 `world visual affine`。
- 缓存正向矩阵、必要时缓存逆矩阵和有效 revision。
- 顶点循环、关键点显示、linker、TransformAdapter 和鼠标逆投影都使用同一缓存。
- 父 revision 未变化且本层 local 未变化时复用 world affine。

收益：从“每个工具/每层/每批顶点重复遍历祖先”收敛为“每层一次组合”，同时降低坐标漂移风险。

完成标准：现有层级语义测试全部通过；深层模型 profile 中祖先遍历次数显著下降；25%～800% 编辑器坐标不回退。

### P0-3：导入过程事务化，消除半初始化框架

问题：`PX_LiveFrameworkImport` 会清空目标结构后逐步分配。失败路径必须非常谨慎地释放部分对象；调用者还容易在 import 前错误地 Initialize 同一个结构。预览启动阶段已经出现过这类分配域问题。

建议：

- 引入 `PX_LiveFrameworkImportIntoTemporary` 或 arena checkpoint。
- 完整解析和验证成功后再把临时结果提交给目标对象。
- 失败只回滚 checkpoint，不让目标框架处于半初始化状态。
- 统一 Initialize/Import/Clone/Free 的所有字段清单，避免新增字段只在一条路径初始化。

完成标准：对每一个截断字节位置和模拟分配失败点测试，目标对象要么保留旧模型，要么保持干净空状态；内存池 used/high-water 可重复。

### P1-1：RT30 只更新受影响图层和顶点的并集

问题：RT30 样本本身是稀疏 binding，但当前求值最终会清零并写回全部 runtime layer/vertex accumulator。小模型影响不大，大模型的眼睛轴也可能扫描所有顶点。

建议：

- 加载/Bake 时构建固定受影响 layer bitset 和 vertex range/index union。
- 参数批次改变时，取“上一批受影响并集 ∪ 当前批受影响并集”进行清零和写回。
- 对连续顶点使用范围，对离散点使用固定索引；不在帧循环分配。
- 全脸等密集轴仍可走顺序连续路径。

完成标准：眼睛单轴更新不再遍历整模顶点；全轴结果逐值等于当前实现；无新增热路径分配。

### P1-2：内存池增加 checkpoint、标签和分配域统计

问题：当前已经增加 current/peak 统计，但预览 UI、模型、authoring、runtime block 等分配域仍靠人工区分。低内存失败时容易先误判为“总内存不足”。

建议：

- 增加轻量 checkpoint/rollback，优先用于事务导入和临时 Bake。
- Debug 构建支持 allocation tag：MODEL、RT30_STATIC、RT30_RUNTIME、AUTHORING、UI、TEMP。
- 输出最大连续块、当前使用、峰值、失败大小和调用域。
- Release/ESP 可编译掉字符串，仅保留数值 tag 和计数器。

完成标准：任何模型导入/UI 创建失败都能报告申请大小、剩余量、最大连续块和分配域；正常热路径开销为零或常数级。

### P1-3：拆分运行时模型状态与编辑器视图状态

问题：`view_scale`、`view_revision` 和部分显示标志位于 `PX_LiveFramework` 中，导入会 `memset` 整个框架，历史上已经导致导入模型后缩放为零。设备运行时也不需要桌面视图字段。

建议：

- 新建 `PX_LiveViewState` 或由 LiveEditor Display/viewport 独立持有视图参数。
- `PX_LiveFramework` 只保留模型、Timeline、RT30 和渲染所需数据。
- Render 接口显式接收 view/projection，不把屏幕状态存入模型对象。

收益：减少导入器和编辑器状态耦合，方便 WSL 头less测试和 ESP 移植。

风险：涉及调用面较大，必须在 P0 测试和 transform cache 完成后做。

### P1-4：修正 PainterEngine 对象构造 API 语义

问题：`PX_ObjectCreateEx` 在 descriptor size 为 0 时先创建对象，随后仍返回 `NULL`，调用者非常容易误用，且失败表象像内存不足。

建议二选一：

1. 让 `PX_ObjectCreateEx` 合法支持 size 0，并仍绑定 update/render/free；或
2. 保持行为但重命名为 `PX_ObjectCreateWithDesc`，对 size 0 添加断言/错误码，并提供 `PX_ObjectCreateCallbacks`。

完成标准：API 文档和编译期/调试期诊断明确；全局搜索不再存在 size 0 的 `CreateEx` 调用；对象创建失败能区分参数错误和分配失败。

### P2-1：把 `.live` 基础格式逐步迁移到显式字段格式

问题：RT30 trailer 已使用固定宽度小端读取，但 `.live` v1 基础主体仍大量依赖原始 C 结构布局和 `sizeof`。跨编译器、32/64 位、对齐和损坏输入的可靠性较弱。

建议：

- 新增 chunked v2/PXR3 writer，所有整数和 float 明确定长、小端、offset 和长度。
- PC 保留 v1 reader，Editor 默认可选输出 v2。
- 设备端先验证完整布局，再一次性布置到 arena。
- 不引入 protobuf/FlatBuffers 等第三方库。

完成标准：GCC/Clang/MSVC 对同一模型生成一致布局；fuzz/截断输入不能越界；v1 模型仍可读。

### P2-2：设备包零拷贝样本与纹理格式分级

问题：ESP 当前先把完整 `.live` 读入 PSRAM，再把纹理和 RT30 深拷贝到 5 MiB 池，加载峰值包含约 0.8 MiB 文件缓冲。RGBA8888 也不是所有图层都必须使用的最省格式。

建议：

- 在 PXR3 设备包中让 RT30 samples 可直接指向只读、对齐的模型块，避免二次复制。
- 纹理按层选择 RGBA8888、RGB565+A8 或其他 PainterEngine 自实现格式。
- 保持 sample-major 连续布局和 16 字节对齐。
- 第一阶段先保证正确性；格式压缩必须有画质和 FPS 对照。

完成标准：加载峰值下降，5 MiB arena high-water 明确；四轴结果与 PC 参考一致；无第三方解码依赖。

### P2-3：可见区域裁剪和层级脏标记

问题：直接缩放渲染已经避免全模型离屏 surface，但仍可进一步减少窗口外三角形和未变化层的工作。

建议：

- 为每层维护模型空间/屏幕空间 AABB 和 revision。
- viewport 外层直接跳过。
- 变换未变、纹理未变时复用边界计算。
- 对 ESP 只做粗粒度裁剪，避免复杂空间结构抵消收益。

完成标准：大模型平移到窗口外时 raster 工作量明显下降；小模型不出现性能倒退。

## 12. 推荐的 WSL 首轮执行顺序

建议新对话不要立刻重构大量框架，而按以下顺序：

1. 只读核对本文路径、正式 EXE 哈希和关键源码。
2. 建立独立的 WSL headless 测试构建目录，不污染源码目录。
3. 先让现有 `rt30_tests.c` 在 GCC 和 Clang 下编译运行。
4. 开启 ASan/UBSan，修复确定性越界、未初始化和算术问题。
5. 为最新的局部变换、纹理、冲量、Panc 和 blend preview 状态补测试。
6. 再实施 P0-2 world affine cache，并用测试对比每层关键点和顶点。
7. 实施事务导入或 arena checkpoint。
8. 每个阶段都从 WSL 调用一次 Windows `build_msvc.bat` 做最终结构验证。
9. 不启动 EXE，由用户在 Windows 端验收 UI。

如果用户下一步明确要求移植 ESP，则改为遵循 `PainterEngine_esp模型替换与框架更新计划.md` 的顺序，不要把 PC 优化和 ESP 移植混成一次大提交。

## 13. 新对话工作规则

- 开始修改前先搜索所有读写路径，尤其是坐标、层级和模式状态。
- 使用 `rg` / `rg --files`，避免盲目全目录替换。
- 编辑真实文件使用补丁方式，保留用户未关联的改动。
- 每次只解决一个可证明的根因，编译后记录证据。
- Windows 正式 EXE 被占用时使用不同 `OUTPUT_NAME`。
- 不结束用户进程，不打开 EXE，不自动上板。
- 运行测试前区分：用户只要求编译，还是授权执行测试。
- 变更 `.live` 格式时必须保留旧读兼容或明确版本升级路径。
- WSL 优化后必须再做 MSVC x64 全量编译。
- 继续维护迭代日志，明确 `verified`、`compiled only` 和 `pending`。

## 14. 最短续接摘要

如果上下文空间很小，只保留以下内容：

```text
当前 PainterEngine/LiveEditor 已实现直接缩放投影、层级安全的 Timeline 局部变换、RT30 每轴30档离散运行时、Q15多轴融合、实时三点编辑和8个底部工具、独立多轴预览进程。

核心路径：
/mnt/d/live2d/PainterEngine
/mnt/d/live2d/project/PainterEngine_LiveEditor

最新正式 EXE：
/mnt/d/live2d/project/PainterEngine_LiveEditor/PainterEngine.exe
SHA256 D562E72D1F9BF5DF25C5D166029BD8AE777203DB49B8AF9ECD9E922433C77071

最新修复：预览面板缩为左下360x250；默认样本权重100%；预览 update 调 PX_LiveFrameworkUpdate 后再 RenderCurrent。MSVC编译成功，用户已确认左下布局和参数驱动模型变化正常；多轴权重等扩展回归项仍需按需检查。

坐标不变量：screen=refer+model*scale，model=(screen-refer)/scale。模型数据不含视图缩放。
层级不变量：父影响子树，子不反向影响父。
RT30不变量：桌面三点Bake，运行只取一个离散样本，多轴Q15融合，热路径不分配。

WSL先建立headless GCC/Clang+ASan/UBSan测试，再做world affine缓存和事务导入；最终仍需Windows MSVC全量编译。不要自行启动EXE或结束用户进程。
```

上面的摘要是 2026-07-14 的历史快照。2026-09-28 之后以本节第 15 节为准，不要把“运行时禁止档间插值”写回代码。

## 15. 2026-09-28 Windows 优化实施记录

规格：`\\wsl.localhost\Ubuntu\home\ubuntu\live2d\doc` 的两份方案。没有新的帧耗时 P50/P95/P99，下面不写加速倍数。

已编译产物：`D:\live2d\project\PainterEngine_LiveEditor\PainterEngine_opt_stage2.exe`（MSVC x64，`build_msvc.bat`，`OUTPUT_NAME=PainterEngine_opt_stage2.exe`）。正式 `PainterEngine.exe` 没有覆盖。

无窗口测试：`tests\rt30_tests.exe`，2305 项检查通过。其中包含拓扑 `[1,1]` 改为 `[0,2]` 后 `vertexOffsets` 重建、连续位置 0 和 29 与整数档位一致、14.5 为有限值、1000 ms 过渡在中点为 14.5。测试夹具把 `mapTexture` 设为 -1，并把烘焙掩码限定为平移/旋转/伸缩/顶点，以匹配原来的 12 字节样本布局。

阶段 1，编辑器数据安全，已改代码并完成编译：

- 导入先在独立内存池校验，成功后再替换当前工程。失败不发送确认事件。超过 64MB 明确报错。
- 导入图片先完成解码和裁切，失败不替换旧纹理、不留下空表项。
- 导出失败不写目标文件。先写 `release.live.tmp`，重新导入通过后才替换，失败时保留旧文件。未烘焙脏轴会另外提示。
- 建网格在临时顶点/三角形成功后才替换原网格。

阶段 2，框架正确性与连续性，测试已覆盖其中的拓扑和连续位置：

- `PlayAnimation` 清零 `reg_elapsed` 和 `reg_duration`。
- 256 层绘制和图层列表不再丢掉最后一层。
- RT30 求值失败时不继续做物理更新。
- 父子关键点重合或顶点落在旋转中心时，拉伸不再做除零。
- `PX_LiveRealtimeSetAxisPosition` 在相邻档之间插值，权重在插值后应用。`SetAxisSample` 仍走整数档。纹理索引用 0.5 阈值，并在 0.42 到 0.58 之间滞回。
- `PX_LiveRealtimeTransitionEvaluate` 按经过时间求值。
- 软件光栅的扫描线 Y 限制在目标表面内。屏幕内像素公式没有改。

阶段 3：图层、纹理、动画和帧列表只在签名变化时重建。新建帧失败时对话框保持打开。拷贝帧时新 ID 写到新帧，不再改到源帧。

阶段 4 到 6 里没有测量数据支撑的项没有改热路径，按方案视为结案，不是遗漏后的默认实现：

- 没有做轴贡献减法缓存、父子仿射跨帧缓存、UV/排序版本缓存。全量求值仍是当前路径。
- 没有做局部权重、组合修正 `C_ab`、误差驱动加点。没有从模型里测到必须加的失真报告。
- 没有把 Alpha 改成预乘，没有打开局部双线性、透明块分类或参数摆动。现有混合路径保持原样。

`esp.live` 只做了哈希核对，没有改文件。SHA-256 仍是 `786B18E342F3A7B3E67042822F138824AAAD610D6C46130F58F5C59BBB379B0B`，大小 798660。`release.live` SHA-256 是 `5CFD67979FB365CC9EDBCA7B955AAC391EBB39BCA062FA855E4209FAA02498B1`，大小 3747332。编辑器里的导入导出闭环没有启动 GUI 去点，这一步仍是编译和主机测试，不是可视化验收。

后续又补了两件计划里还缺的制作端检查，没有改 `.live` 格式：

- 烘焙成功后检查档位 0、中点、29，以及和另一条已烘焙轴的一个端点组合。状态栏显示翻折、退化和 UV 越界的合计。这只报告，不自动改网格。
- 多轴预览把快照校验和放在命令行 `--rt30-sum`。子进程对不上就删除临时文件并失败。有未烘焙修改时，打开预览前说明用的是上一份烘焙结果。

### 2026-09-28 续：PC 实测后改热路径

无窗口程序：`project\PainterEngine_LiveEditor\tests\live_bench.exe`，MSVC `/O2`。模型文件没有改。`esp.live` 仍是 798660 字节。下面的时间是这一次在本机跑出来的，不是估计。

`esp.live`：320×320，11 层，227 顶点，257 三角形，4 轴。改代码前一帧大约是：参数求值 2–7 微秒，层级和顶点 19 微秒，软件光栅中位数约 1330 微秒。`release.live` 没有实时轴，光栅中位数约 3200 微秒。时间几乎全在光栅采样，不在轴求值。

已放进运行时，并用同一姿态校验和对照过：

- 参数没变、顶点弹性系数 `k` 全是 0 时，不再重算层级和顶点。静止帧这项从约 19 微秒降到约 0.2 微秒。
- 轴贡献按上次真正加进去的样本做减法更新。改动轴的样本比全体活动轴更大时仍走全量，避免大轴读两遍。`esp.live` 上 8 组连续姿态和全量重建的姿态校验和一致。脸轴单轴扫描中位数约 5.5 微秒，和改前的 5.4 微秒同一量级。
- 子节点拉伸改成单位方向上的投影，`p' = p + u * max(dot(u,p), 0) * (s-1)`。`esp.live` 各扫描末帧的姿态校验和与改前相同。
- 三角形 z 都是 1 时不再做逐像素透视除法，扫描线 x 裁到表面内。默认双线性画面的像素校验和与改前相同。光栅中位数约 1270 微秒，大约少 60 微秒。
- 直通 Alpha 用单像素核对过：源 200、Alpha 128，黑底得到 100，白底得到 228。软件画布没有先乘 Alpha 再乘一次。

测过但没有改成默认：

- 把同一套双线性改成 float 后，3181 个像素有差，最大通道差 2，单帧约 1215 微秒。省得太少，默认仍是原来的 double 双线性。
- 最近邻单帧可以到约 330 微秒，但有 19748 个像素不同，最大差 255。没有全局打开。
- 预乘空间过滤再转回直通色，9995 个像素不同，最大差 172，也不更快。没有打开。

变形：四轴同时在 14.5 没有翻折；同时在 29 有 1 个翻折，图层 `neck`，三角形 3，顶点 3、2、1，源面积 -15，当前面积 13.3。两种姿态都有 58 个 UV 越界，数量不变，是源数据而不是这次形变新造出来的。没有现成的目标组合姿态，所以没有写入组合修正，也没有改 `esp.live`。

编辑器正式 `PainterEngine.exe` 仍不覆盖。带这次运行时改动的编辑器是 `PainterEngine_LiveEditor\PainterEngine_opt_stage6.exe`，MSVC 链接退出码 0。宿主 `tests\rt30_tests.exe` 这次是 2305 项检查通过。
