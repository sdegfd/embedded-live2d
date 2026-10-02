# 模型部署

esp 例程将原始 `models/esp.live` 复制到 SD 卡根目录，设备路径 `/sdcard/esp.live`。
light 例程默认内嵌原始 `models/light.live` 到 Flash；启用 `CONFIG_L2D_LIGHT_FROM_SD` 时，将它复制到 SD 卡根目录，路径 `/sdcard/light.live`。

固件会核对模型大小、SHA256 和轴数。模型清单见根目录 `models/manifest.json`；不一致时停止测试，不自动选择其他文件。两个例程各只加载一个模型。
