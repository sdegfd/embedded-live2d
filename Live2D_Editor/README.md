# PainterEngine Live2D 编辑器

用于制作本框架的 `.live` 资源，基于 PainterEngine LiveEditor。包含图层、网格、层级、Timeline、RT30 关键姿态与烘焙，以及 PSD/PSB 导入。

## 构建

在 Windows 安装 Visual Studio C++ 工具链，然后执行：

```bat
Live2D_Editor\build_msvc.bat
```

生成 GUI `PainterEngine.exe`。源码入库，exe 和中间文件不入库。Linux 回归验证播放库与 PC JPEG 导入适配；本轮未重新构建 Windows GUI。

PC JPEG 导入使用具有明确 MIT 声明的 `stb_image`，支持基线及渐进 JPEG。

## 资源导入

File → Import PSD 读取边长最多 1280 像素的 PSD/PSB；每个像素图层生成纹理和根图层。组被展开，不直接生成骨骼。导入逻辑由编辑器实现，嵌入式播放库不解析 PSD。

不要把 GUI 预览姿态重新导出覆盖测试基准；示例身份见根目录 `models/manifest.json`。

PSD SDK 使用 BSD-2-Clause，PainterEngine 使用 MIT，相关声明见 [THIRD_PARTY.md](../THIRD_PARTY.md)。
