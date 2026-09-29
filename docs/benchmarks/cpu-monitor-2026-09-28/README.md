# ESP32-P4 CPU 占用与帧等待复测

日期：2026-09-28。固件源码提交：`63a616b7edf43f691964d3e04b2707c676dc2a29`；ESP-IDF 5.5.2。设备模型 `/sdcard/esp.live` 为 798660 字节，SHA256 `786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b`。数据见 [cpu_windows.csv](cpu_windows.csv) 和 [runtime.log](runtime.log)。

## 测量方法

旧监控任务每次固定加 64 次计数后休眠 1 tick，得到的 3–8% CPU0 读数不能表示 CPU 空闲比例。新版本启用 FreeRTOS 运行时间统计（ESP Timer、64 位计数），每约 1 秒读取各核 Idle 任务的累计运行时间。`CPU 使用率 = 100% × (1 − 本窗口 Idle 运行时间 / 本窗口经过时间)`。监控任务固定在 CPU1，确保该核 Idle 运行段在采样前结算；渲染任务仍固定 CPU0。返回值是每核非 Idle 时间占比，包含该核上所有任务，不是 Live2D 单任务 CPU 占用，也不是 PPA 硬件占用。

主循环的 `total_us` 是帧开始到面板提交返回的耗时；`budget_remain_us = max(33333 − total_us, 0)` 是计算出的剩余预算；`actual_wait_us` 是调用节拍等待函数实际经过的时间；`period_us` 是帧开始到等待结束的时间。后三项是本次新增的每秒平均值。`vTaskDelay()` 使用整数 tick，故实际等待不一定等于预算剩余。每秒日志本身也会影响相邻帧间隔，所以 `period_us` 与根据 FPS 反推的间隔不必完全相同。

## 真机结果

串口保留了短测结束后的 14 个连续动画窗口。窗口 0 包含短测切换到持续动画的过渡，以下只统计窗口 1–13。短测最后设置的 render scale 为 0.75，持续动画沿用该值；它不是 1.0× 结果。

| 指标 | 13 个稳定窗口平均值 | 范围 |
| --- | ---: | ---: |
| CPU0 非 Idle 占比 | 68.0% | 67–69% |
| CPU1 非 Idle 占比 | 0%（整数取整） | 0% |
| 实际 FPS | 30.14 | 29.94–30.36 |
| 渲染并提交 `total_us` | 27.02 ms | 26.80–27.28 ms |
| 预算剩余 `budget_remain_us` | 6.31 ms | 6.05–6.53 ms |
| 实际等待 `actual_wait_us` | 5.46 ms | 5.36–5.60 ms |
| 帧开始到等待结束 `period_us` | 32.53 ms | 32.30–32.74 ms |

13 个窗口均无超 33.333 ms 预算的帧，串口未见 panic、Guru Meditation、Backtrace 或 Task watchdog。CPU1 的 0% 是日志的整数精度，不表示绝对没有运行任务。

FreeRTOS 运行时间统计给任务切换增加计时开销，因此本次固件与此前未启用该功能的 [原始 timing 基线](../profiling-2026-09-28/frames.csv) 属于不同构建；不要把两次的微秒数直接作为渲染算法性能差值。原始 timing 数据仍用于渲染阶段基线，本报告用于每核 CPU 占用和节拍等待观测。
