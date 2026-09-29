# Live2D RT30 实时多轴基线验证工程

## 当前 profiling 版本

本工程以已移植的 PC RT30 连续位置、纹理滞回、增量轴累加和姿态 revision 路径为渲染基线。Phase 0–2 增加计时、CRC、细分诊断和 PPA/CPU 后端 A/B；保留 REALTIME30 跳过无用弹性方向计算，child stretch 预计算实验已回退，最终仍使用 PPA clear + PPA convert。结果见仓库 `doc/optimization/HANDOFF_PHASE0_2.md`。

CPU 占用由 FreeRTOS 每核 Idle 任务的运行时间增量计算，约每秒更新一次，不再使用固定计数后休眠的空闲测量任务。持续动画日志另给出每帧平均 `total`（渲染并提交）、`budget_remain`（33.333 ms 预算剩余）、`actual_wait`（节拍函数实际等待）和 `period`（帧开始到等待结束），单位均为微秒。真机数据见仓库 [CPU 占用与帧等待复测](../../../doc/cpu-monitor-2026-09-28/README.md)。

默认 `CONFIG_L2D_PROFILE_TIMING=y`：按 `CONFIG_L2D_PROFILE_STAGE` 选择测试场景；每场景 scale 1.0、预热 5 帧、记录 50 帧、只跑 1 轮。Stage 0 测 STATIC、EYE_L_SWEEP、MULTI_AXIS；Stage 1 增加 NECK_SWEEP、FACE_SWEEP；Stage 2 测 STATIC、MULTI_AXIS。每组采集结束才通过串口输出原始帧记录和 summary，热路径不打印；PC 端运行仓库 `tools/capture_l2d_profile.py` 持续捕获到 CSV。板端不把测试结果写入 SD。独立的 `CONFIG_L2D_PROFILE_CORRECTNESS` 模式记录固定姿态 ARGB8888/RGB565 CRC。

`CONFIG_L2D_PROFILE_DETAIL=y` 运行独立工作量统计，从串口输出 `L2D_DETAIL` 记录；可选 `CONFIG_L2D_PROFILE_OVERDRAW=y`。详细模式的耗时不能用于正式 benchmark。

实际主路径是同步 `ARGB8888 → PPA SRM → RGB565 → sys_display_flush → panel`。本例程无异步 producer/consumer 帧槽，亦无 PPA blend；相关计数字段为 0，应按“不适用”解释。模型由使用者部署到设备 SD，性能数据仅对应报告记录的设备模型 SHA256。

当前分阶段真机计时与 CPU 核心分配见 [2026-09-28 渲染基线记录](doc/render-baseline-2026-09-28.md)。

独立基线工程，用于在 ESP32-P4 上验证 PainterEngine 的 **RT30 实时多轴**动画能力（离散 30 样本 + Q15 多轴融合 + 局部变换 + 模式仲裁），验证通过后再把合并后的内核移植回成熟工程 `../live2d_demo`。

依据：`/home/ubuntu/projects/project/PainterEngine_esp模型替换与框架更新计划.md`（阶段 B/C/D/E）。
模型运行时路径固定为 `/sdcard/esp.live`。正式测试只使用工作区 `project/esp.live` 这一份五轴模型（`left_eye`/`right_eye`/`neck`/`face`/`mouth`）。开测前核对两边 SHA256；不一致就停止，不改用其他 `.live`。规则见 `docs/refactor/MODEL_BASELINE.md`。

## 目录结构

```
live2d_rt30_baseline/
├── CMakeLists.txt                 # 顶层（EXTRA_COMPONENT_DIRS: components, components/sys）
├── sdkconfig.defaults            # 精简：LVGL/显示/PSRAM/板子/FATFS/FreeRTOS/分区/Flash/编译
├── partitions.csv                # nvs/phy/factory 9M/storage 6M
├── main/main.cpp                 # RT30 五轴逐帧连续驱动压力测试
├── sdcard/README.md              # 模型部署说明；模型文件由使用者拷贝
└── components/
    ├── live2d_engine/            # PainterEngine 内核 + 业务封装（已合并 RT30）
    │   ├── live2d_engine.{h,c}   # 业务层 + realtime API 封装
    │   └── painterengine/kernel/
    │       ├── PX_LiveFramework.{h,c}   # ESP 基线 + 增量合入 RT30/局部变换/模式仲裁
    │       ├── PX_LiveRealtime.{h,c}    # RT30 求值器（从 PC 移植，未改）
    │       └── PX_LiveDeviceFormat.{h,c} # 安全小端 reader/范围校验（从 PC 移植）
    ├── live2d_renderer/          # PPA 清屏 + PPA ARGB8888->RGB565
    ├── wt99p4c5_s1_board/        # BSP：MIPI-DSI 显示 / SDMMC SD 卡 / 触摸（原样复制）
    ├── espressif__esp_lvgl_adapter/  # vendored LVGL 适配层（原样复制）
    └── sys/                      # sys_display / sys_display_buffer / sys_display_flush
                                   # sys_monitor / sys_storage / sys_sdio（原样复制）
```

**未纳入**（隔离成熟工程复杂度）：game / agent / audio / music / web / ui / app_verify / sys_audio / sys_network / sys_recorder / sys_usb_audio。

## 合并要点（PX_LiveFramework）

以 **ESP 版本为基线增量合入** PC 的 RT30 能力，**禁止整份覆盖**，保留 ESP 优化：
- 保留：`fastNearestSampling`、`renderScale`、sin/cos 缓存（`rel_currentRotationSin/Cos` + `PX_LiveFrameworkSetLayerCurrentRotationAngle` + `PX_LiveFrameworkRotatePointCached`）、快速像素着色器、ESP 性能 profiling。
- 合入：`PX_LIVE_ANIMATION_MODE`(NEUTRAL/TIMELINE/REALTIME30) 互斥状态机、`PX_LiveRealtime realtime` 字段、图层局部变换字段（`rel_*Local*`）、payload 局部变换字段（复用 `reserve` 区，体积不变）、RT30 尾部解析（`PX_LiveFrameworkImportRealtimeTrailer`）、`PX_LiveFrameworkUpdate`/`RenderCurrent` 拆分与模式仲裁、局部变换层级传播（`PX_LiveFramework_GetLayerVisualTransform`）、realtime API 包装。
- ESP `PX_LiveFrameworkRender` 签名保持 `px_int x,y`（PC 用 `px_float`，未采纳）。`view_scale`/`view_revision` 为编辑器视图用，设备不合入。

2026-07-14 真机修正：

- 修复叶子图层未计算 visual transform 的错误；该错误会让眼睛等叶子图层使用未初始化变换，造成错位、眨眼异常和栅格范围异常放大。
- 快速最近邻 span 增加纹理坐标边界检查，避免变形极值时越界读取。
- 图层视觉旋转改为每图层每帧计算一次 sin/cos，不再对每个顶点重复建旋转矩阵。
- UV 更新把纹理宽高倒数移出顶点循环；快速最近邻栅格器把仿射 UV 梯度改为每三角形计算一次，消除顶点循环和逐扫描线的重复浮点除法。
- RGB565 转换移除重复 alpha 预乘；PainterEngine 画布 RGB 已经是合成后的颜色，二次预乘会加深半透明边缘。
- CPU 回退转换按 ESP32-P4 小端 BGRA 布局使用单次 32 位源像素读取；正常路径由 Live2D 独占 PPA SRM 完成格式转换。
- 关闭零旋转、DOUBLE_DIRECT 配置下未使用的 LVGL adapter SRM 客户端，解除它与 Live2D SRM 的争用；连续硬件转换不再在第二帧阻塞。

## 五轴连续驱动验证（main.cpp）

- `left_eye`/`right_eye`：样本 0→29→0 逐渲染帧连续往返。
- `neck`：样本 1→29→1 逐渲染帧连续往返。
- `face`：样本 5→25→5 逐渲染帧连续往返。
- `mouth`：样本 0→29→0 逐渲染帧连续往返。
- 全轴权重 Q15 = 32767，`live2d_engine_set_axes_batch` 一批提交，求值由 `PX_LiveFrameworkRender` 的 REALTIME30 分支统一触发一次（dirty revision，参数不变不重算）。
- 驱动只使用成功渲染帧号，不设置动作时长，不经过停止/回 Idle 状态；五轴保持共同驱动并无限循环。
- 每秒性能日志额外给出 `over_budget`（超过 33.3 ms 的帧数）、`max_total`（窗口最慢帧）和 `batch_fail`，用于直接判断连续多轴路径是否出现卡顿或提交失败。
- 测试开始后关闭 `PX_LiveFramework`、renderer、flush 与 monitor 的周期 INFO 日志，避免 UART 同步打印在渲染任务中制造 80–95 ms 假慢帧；保留 `l2d_baseline` 单行汇总。

以上仍是严格离散 RT30：每轴每帧只选择一个预烘焙样本，不在相邻样本间插值；连续感来自逐渲染帧遍历样本，而不是缩放或模糊。

## 真机性能基线（ESP32-P4 Rev 1.3，360 MHz）

- 模型与渲染 ROI 均为原始 `320x320`，`renderScale=1.0`，未缩放。
- PPA 分别用于透明清屏和 ARGB8888→RGB565；Live2D 使用独立 SRM 客户端，LVGL adapter 在零旋转模式下不再注册冗余 SRM 客户端。
- 优化前：约 12–13 FPS；修复叶子图层变换后约 18–19.5 FPS。
- 当前：摇头/侧身的重姿态窗口约 28.1–28.7 FPS，常态约 29.7–30.0 FPS；整帧约 33–35 ms。
- 当前分项均值：RT30 求值约 0.7 ms，物理/顶点约 4.9 ms，图层绘制约 18.6 ms，PPA RGB565 转换约 6.7 ms，flush 约 0.2–0.4 ms。
- 单核渲染任务固定在 CPU0；实测 CPU0 约 52–57%，CPU1 保持 0%。连续运行覆盖左右极限姿态，无 panic、abort、看门狗、内存下降或栈水位异常。

2026-08-10 五轴无限驱动复测（当前 `/sdcard/release.live`）：

- `left_eye/right_eye/neck/face/mouth` 每帧共同批量提交，连续循环，不设置动作时长、不停止、不回 Idle。
- 真机连续记录 45 个约 1 秒窗口，稳定 29.87–30.08 FPS，CPU0 约 53–57%，全部 `batch_fail=0`。
- 稳态整帧均值约 33.0 ms；关闭渲染热路径的多行 INFO profiling 后，稳态窗口最慢帧约 33.5–34.5 ms，不再出现周期性的 80–95 ms 串口打印假慢帧。
- 肉眼观察全程流畅。因此可排除“RT30 五轴共同求值/渲染本身”是小智工程在语音结束或禁用输入时卡顿的根因；下一步应检查小智状态切换、动作控制和 UI 管线的额外工作。

## 构建与运行

```bash
cd /home/ubuntu/esp/project/live2d_rt30_baseline
. /home/ubuntu/esp/esp-idf/export.sh
idf.py set-target esp32p4      # 首次
idf.py build
# 使用者将当前模型拷到设备 SD 卡的 /sdcard/esp.live
idf.py -p /dev/ttyUSB0 flash monitor
```

启动日志验收：
- `RT30 realtime axes: 5`（不是 0——0 表示尾部被静默忽略）
- 五个 `axis[i] id='...'`（left_eye/right_eye/neck/face/mouth）
- `axis handles: left_eye=0 right_eye=1 neck=2 face=3 mouth=4`（或对应 handle）
- 渲染循环 `perf ... eye=.. neck=.. face=.. mouth=..` 持续变化，`batch_fail=0`，无 panic/abort/backtrace。

## 后续（不在本工程范围）

把合并后的 `PX_LiveFramework.{h,c}` + `PX_LiveRealtime.*` + `PX_LiveDeviceFormat.*` + `live2d_engine` realtime API 移植回 `../live2d_demo`（增量合并，同样保留 ESP 优化）。运行时模型替换状态机（`.tmp/.bak` 回滚）属资源层，本基线用静态 `/sdcard/release.live`。
