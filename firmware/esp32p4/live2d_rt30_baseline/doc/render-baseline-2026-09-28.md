# ESP32-P4 Live2D RT30 渲染基线（2026-09-28）

## 运行方式

工程：`/home/ubuntu/esp/project/live2d_rt30_baseline`；ESP-IDF v5.5.2；ESP32-P4 Rev 1.3，CPU 360 MHz，PSRAM 32 MiB。模型从 SD 卡 `/sdcard/release.live` 读取：**800796 字节**，320×320、11 图层、12 纹理、1 个传统动画、5 个 RT30 实时轴。渲染比例为 1.0，目标 30 FPS，每成功渲染一帧推进离散 RT30 样本；五轴批量提交，不使用动作持续时间。

注意：工程内 `sdcard/release.live` 为 **798660 字节**，SHA-256 为 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`，与真机本次读取的文件大小不同。真机当前模型为 **800796 字节、CRC32_LE=0x5f25edc0**（随后同设备重启实测）；不能用工程内模型的哈希代表真机模型。27 个窗口采集时还没有 CRC 日志，但两次启动读到相同文件大小，期间没有执行 SD 卡写入；后续 A/B 均应记录 CRC，避免仅凭大小推断文件一致。

**芯片和 FreeRTOS 启用双核，但当前 Live2D 渲染路径按单核运行。** `main/main.cpp` 的渲染任务固定 CPU0，LVGL adapter 的任务配置也为 CPU0，SD 串行任务固定 CPU0。PPA 负责透明清屏和 ARGB8888→RGB565 转换，是硬件单元，不能称为“CPU1 渲染”。CPU1 仍由 FreeRTOS 启用，其他未固定任务可能运行于其上；本次监控估计 CPU1 负载为 0%。`sys_monitor` 的 CPU 百分比由每核空闲测量任务估算，仅用于趋势，不是精确周期计数。

## 分阶段计时口径

固件每秒打印一行 `perf`，下面的 `avg_us` 是该窗口内**成功渲染帧**的平均微秒数；`total` 从帧开始到提交结束，**不含末尾等待下一帧**。`max_total` 为该窗口最慢帧。每项使用 `esp_timer_get_time()`，包含该阶段函数自身的调度/调用开销。

| 字段 | 计时边界 | 包含关系 |
|---|---|---|
| `axis` | 计算四种样本值、构造五轴状态并调用 `live2d_engine_set_axes_batch` | 在 `total` 内，`render` 外 |
| `clear` | ARGB8888 画布清屏整体 | 在 `render` 内；`fill` 和 `sync` 是其子阶段 |
| `fill` / `sync` | PPA FILL 或 CPU 清零；PPA 清屏后的 cache M2C 同步 | 在 `clear` 内；剩余为清屏调度开销 |
| `pose` | RT30 求值；传统动画模式下为 VM | 在 `engine` 内 |
| `physical` | 物理、层级与顶点更新 | 在 `engine` 内 |
| `sort` | 构造图层绘制列表并排序 | 在 `engine` 内 |
| `draw` | 逐图层绘制和软件三角形栅格化 | 在 `engine` 内 |
| `engine` | 整个 PainterEngine 调用 | 在 `render` 内；包含上述内核阶段及少量其他开销 |
| `render` | `clear + engine` | 在 `total` 内 |
| `convert` | ARGB8888→RGB565，当前由 PPA 完成 | 在 `total` 内，`render` 外 |
| `flush` | `sys_display_flush_submit` 同步调用 | 在 `total` 内，`render` 外；不代表面板实际扫描时间 |

`axis + render + convert + flush` 与 `total` 之间允许有少量主循环计时和统计开销。`clear`、`engine` 与其子项**不能再次相加**。`over_budget` 以 `total > 33333 µs` 计数，不包含休眠；`fps` 来自窗口内完成帧数/墙上时间。

## 本次真机结果

计时版固件在 `/dev/ttyUSB0` 烧录，写后哈希校验通过；复位后新进程日志确认 `RT30 realtime axes: 5` 和五个轴 handle 均有效。排除启动的 window 0，采集 **27 个稳态窗口、837 帧**，各窗口均值的平均如下（数字已四舍五入）：

| 阶段 | 平均耗时 |
|---|---:|
| 轴参数准备与批量提交 `axis` | 46 µs |
| 清屏 `clear` | 1,564 µs |
| └ PPA 填充 `fill` | 1,538 µs |
| └ cache 同步 `sync` | 11 µs |
| RT30 求值 `pose` | 879 µs |
| 物理/顶点更新 `physical` | 4,888 µs |
| 图层列表与排序 `sort` | 82 µs |
| 图层/三角形绘制 `draw` | 18,360 µs |
| PainterEngine 整体 `engine` | 24,303 µs |
| 清屏 + 引擎 `render` | 25,868 µs |
| PPA 格式转换 `convert` | 6,452 µs |
| 显示提交 `flush` | 225 µs |
| 整帧 `total` | **32,658 µs** |

窗口 FPS 平均 **30.10**，CPU0 估算负载约 **53%**，CPU1 为 **0%**。稳态窗口合计 `batch_fail=0`，`over_budget=15/837`；27 个窗口的 `max_total` 平均约 33,373 µs。未见 panic、abort、Backtrace 或看门狗错误。启动窗口含初始化/首帧开销，不纳入稳态均值。

本次串口原始记录：`/tmp/live2d_rt30_profile_monitor.log`；模型 CRC 复核记录：`/tmp/live2d_rt30_crc_monitor.log`。这些路径是本机临时文件，后续机器不会自动保留。完整复现实验时应另存串口日志、构建配置和模型文件校验值。

## 后续 PC 优化移植的对比规则

每次移植先核对 PC 源码、接口及 `.live` 格式版本，**记录并匹配真机启动日志中的模型大小与 CRC32_LE**，再保持五轴参数序列、320×320 ROI、1.0 渲染比例、30 FPS 目标、CPU0 任务绑定和 PPA 路径不变。保留 ESP 现有的快速采样、三角函数缓存和 PPA 转换；按功能块逐步移植并在每一步记录上述全部阶段、FPS、超时帧、CPU0/CPU1 与内存。输出正确性先于耗时比较：旧模型可导入，轴样本/权重和多轴组合一致，无越界、panic 或画面退化。若新算法改变了阶段边界或把计算从制作端移到运行端，应在对比表中注明，不能把不等价阶段直接比较。
