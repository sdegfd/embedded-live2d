# live2d_pc_preview

Windows PC 端 Live2D 预览工程。

这个工程复用了官方 Cubism Native D3D11 sample 的窗口、输入和渲染路径，并把模型资源整理成当前工作区自己的预览资源目录，用来做两件事：

- 在 PC 上先验证 moc3 / model3 / motion3 / physics3 是否能正常运行
- 作为后续 ESP32 运行框架移植前的桌面基线

## 当前预览模型

构建后会自动整理这两个模型到运行目录下的 `Resources/`：

- `wyz_preview`
- `mark_preview`

应用启动后默认会载入 `wyz_preview`。点击右上角齿轮可以切换模型，拖拽可查看视线跟随，点击命中区域可触发表情或动作。

键盘调试入口：

- `1` 触发当前模型可用的随机动作
- `2` 触发当前模型可用的随机表情
- `P` 打印当前模型的参数快照、动作组和表情列表
- `M` 切换到下一个模型
- `H` 重新打印快捷键说明

说明：`wyz_preview` 本身不包含 motion 和 expression 资源，因此按 `1` / `2` 时会明确打印“当前模型没有可用资源”；切到 `mark_preview` 后可直接验证动作和表情入口。

## 工程特点

- 不复制整套官方 sample 源码，直接在 CMake 中引用 SDK 自带 D3D11 sample 源文件
- 只复制 sample 运行必需的公共 PNG、shader，以及工作区里的目标模型资源
- 不依赖 SDK 缺失的第三方子模块；纹理加载改为工程内置的最小 WIC 兼容实现
- 导出器 `live2d_demo` 和预览器 `live2d_pc_preview` 分离，便于后续把“数据提取”和“运行时/平台层”分别演进

## 构建要求

- Windows
- CMake 3.16+
- Visual Studio 2017/2019/2022/2026 工具链
- 官方 Cubism Core / Framework 已存在于当前工作区

注意：当前使用的是官方提供的 Windows Core `.lib`，因此这里要求 MSVC 工具链。

## 构建示例

```powershell
cd D:\live2d\live2d_pc_preview
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

## 运行

生成的可执行文件位于：

```text
build/bin/Live2DPreview/Release/
```

运行时会在同目录下自动准备：

- `Resources/`
- `SampleShaders/`
- `FrameworkShaders/`

## 与导出器的关系

建议工作流：

1. 先用 `live2d_demo` 导出完整 JSON，确认 model3 / motion3 / physics3 等数据齐全
2. 再用 `live2d_pc_preview` 运行同一套模型资源，确认桌面端行为正确
3. 最后把参数更新、网格更新、渲染和资源加载逐步替换成适合 ESP32P4 的实现

当前这个预览工程的意义不是直接替代 MCU runtime，而是先把 PC 端可视化验证链路建立起来。