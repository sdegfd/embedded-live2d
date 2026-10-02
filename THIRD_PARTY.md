# 上游与第三方声明

本项目自身代码及改动采用根目录 [MIT 协议](LICENSE)。已有第三方文件保留各自的版权、协议和文件内声明。

| 部分 | 来源 | 协议与本地声明 |
| --- | --- | --- |
| `PainterEngine/`、嵌入式私有内核 `Live2D/src/internal/pe/` | [PainterEngine](https://github.com/matrixcascade/PainterEngine)，DBinary | MIT；[原始声明](PainterEngine/LICENSE)、[运行时副本声明](Live2D/src/internal/pe/LICENSE) |
| 编辑器 PSD/PSB 导入 `Live2D_Editor/third_party/psd_sdk/` | [Molecular Matters PSD SDK](https://github.com/MolecularMatters/psd_sdk) | BSD-2-Clause；[原始声明](Live2D_Editor/third_party/psd_sdk/LICENSE)；其中 miniz 的公共领域声明保留在源文件末尾 |
| PC JPEG 导入 `PainterEngine/third_party/stb/` | [stb_image](https://github.com/nothings/stb)，v2.30，具体提交见目录 README | 按 MIT 选项使用；完整许可保留在 `stb_image.h` 末尾。原有标注来源无许可证的 JPEG 实现已替换 |
| P4 本地显示适配器 `espressif__esp_lvgl_adapter` | [Espressif esp-iot-solution](https://github.com/espressif/esp-iot-solution/tree/master/components/display/tools/esp_lvgl_adapter)，0.5.1；组件 manifest 记录上游提交 | Apache-2.0；[原始声明](examples/esp32p4/components/espressif__esp_lvgl_adapter/LICENSE)。本地修改涉及 PPA 对齐、可选依赖和 LVGL/FreeRTOS 配置兼容 |
| P4 板级组件中带 Espressif 版权和 SPDX 标识的文件 | Espressif BSP 代码及本地板卡适配 | 遵循文件头中的 Apache-2.0；[协议全文](licenses/Apache-2.0.txt) |
| `models/esp.live`、`models/light.live` 及对应 JSON | 经提供方确认可作为本项目示例再分发 | [示例资源许可](models/LICENSE)；不自动套用代码的 MIT 协议 |

PainterEngine 中嵌入的其他库保留其文件内许可。不要删除原作者署名，也不要把根目录 MIT 声明当作对第三方文件的重新授权。

ESP-IDF、LVGL、FreeType、LCD/触摸等由 ESP-IDF Component Manager 下载，未将生成的 `managed_components/` 收进仓库；对应组件的协议随下载的源码提供。分发固件时也应保留适用的第三方声明。Host 测试使用系统提供的 OpenSSL、zlib，播放库核心不依赖它们。
