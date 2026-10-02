# ESP32-P4 例程

硬件：WT99P4C5-S1、ESP32-P4 rev 1.x、16 MB Flash、32 MiB PSRAM、1024×600 MIPI 显示。验证工具链为 ESP-IDF v5.5.2。

例程链接根目录 `Live2D/` 的唯一播放库；板卡、PPA、缓存同步、显示、SD 和 CPU 统计留在本目录。显示适配器从 `components/espressif__esp_lvgl_adapter` 的相对路径加载，其余依赖由 Component Manager 下载。LVGL、FreeType、显示及触摸版本在 manifest 中固定，生成的依赖锁保存在各 build 目录。

## esp.live

将仓库 `models/esp.live` 复制到 SD 卡根目录，设备路径为 `/sdcard/esp.live`。固件检查大小、SHA256 和轴数。缺少文件时不会改用其他模型。

```sh
. "$IDF_PATH/export.sh"
cd examples/esp32p4
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build_esp -D SDKCONFIG=sdkconfig.release.esp -p /dev/ttyUSB0 flash
cd ../..
python3 examples/esp32p4/tools/capture_l2d_profile.py \
  --port /dev/ttyUSB0 --reset --out captures/esp-p4
```

默认是 short-v1 计时场景：stage 0、预热 5 帧、测量 50 帧、scale 1.0、33.333 ms 截止时间。设置 `CONFIG_L2D_PROFILE_CORRECTNESS=y` 可单独运行固定姿态 CRC；正确性模式不用于性能测量。

## light.live

见 [13 轴连续测试说明](light-example.md)。在独立 build/config 中叠加 `sdkconfig.light.defaults`，默认内嵌原始模型，并开启双核 64 KiB/核 SRAM 条带。只有单个 light 模型及一个播放实例参与正常播放；Host 中的第二实例用于隔离测试。

计时日志按场景结束后输出，CPU 由每核 Idle 运行时间差计算。原始串口与本机抓取输出写入被忽略的 `captures/`，发布结果见根目录 [benchmarks/p4](../../benchmarks/p4/README.md)。

## 预设与限制

`sdkconfig.defaults` 固定 CPU 360 MHz、PSRAM 200 MHz、L2 256 KiB / 128 B line、内部 DMA 保留 128 KiB、性能编译及运行时间统计。修改预设后，已有 sdkconfig 会覆盖 defaults；要复现请使用新的 build/config 路径。

PIE、BitScrambler、复制式 SRAM scratch、异步清屏等试验选项默认关闭。实测约 25 FPS 的 light 预设使用双核 RGB565 条带，保留原始纹理、最近邻采样及完整 BGRA 混合。当前例程包含验证/性能测试功能，并非所有板卡通用的 BSP。
