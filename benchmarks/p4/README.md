# P4 性能与像素基准

此目录保留可复查的精简结果；开发日志和原始串口归档不入库。根目录 README 汇总数据与内存口径。

| 文件 | 条件 / 含义 |
| --- | --- |
| `esp.csv` | 整理后 esp / PPA SRM ROI 开启，short-v1、scale 1.0；三个场景各预热 5 帧、测量 50 帧；摘录帧耗时及光栅时间 |
| `esp-steady.csv` | 短时场景之后的 12 个约一秒的五轴连续播放窗口；首个窗口含过渡，不计入 README 汇总。时间列单位为微秒，CPU 为每核总占用 |
| `light.csv` | 整理后 light、480×600 / scale 0.9375、每核 64 KiB SRAM 条带；7 个场景各预热 2 秒、测量 30 秒 |
| `light-pixels.csv` | 56 个固定/组合/语义姿态的 Host BGRA 与 RGB565 CRC；原始模型及固定布局，整理前后 P4 对照均通过 |
| `esp-baseline.csv` | 既有 esp 短时基线；耗时倒数只表示吞吐估算，未同步采样 CPU |
| `light-baseline.csv` | 整理前恢复条带版本；同硬件，7 场景各预热 2 秒、测量 10 秒；13 轴 25.172 FPS |
| `environment.json` | 本轮硬件、工具链、模型身份、固件 SHA256、参与播放的源码指纹（换行统一为 LF） |

本轮测试日期 2026-10-02；WT99P4C5-S1 / ESP32-P4 rev 1.3，CPU 360 MHz、32 MiB PSRAM / 200 MHz、256 KiB L2 / 128 B cache line；ESP-IDF v5.5.2、LVGL 9.5.0。两个例程分别构建、加载一个模型并实板运行。

light 的 FPS = 成功显示提交 / 墙上时间；CPU = 100% - 每核 Idle 比例。光栅项含条带清屏和 RGB565 转换，单独 clear/convert 项为 0。33.333 ms 超时帧不等于提交失败；errors 列表示实际错误。测试完成后在同一串口连接记录 52 秒，确认 13 轴全动、说话转头、摇摆挥手、踏步四种动作循环。

esp 的持续 FPS 按每窗口帧数和墙上时间统计；平均总耗时不包含帧率限制等待。README 使用第 1..11 窗口按帧数加权的平均值。硬件显示刷新率不能由这两个例程的 FPS 推断。

运行新测试写到被忽略的 `captures/`；不要覆盖已发布基准。复现、抓取与 CRC 比较命令见 [P4 例程](../../examples/esp32p4/README.md) 和 [light 测试](../../examples/esp32p4/light-example.md)。Host 速度不代表 P4 速度。
