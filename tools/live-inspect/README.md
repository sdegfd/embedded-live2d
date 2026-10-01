# live-inspect

把 PainterEngine `.live` 读成 JSON。程序是单个 exe，运行时不需要 Python、Visual Studio 或 PainterEngine。

```bat
tools\live-inspect\live-inspect.exe models\esp.live
tools\live-inspect\live-inspect.exe models\light.live -o models\light.json
```

不写 `-o` 时，JSON 生成在模型旁边，扩展名换成 `.json`。

JSON 的 `schema` 是 `live-inspect-v1`。里面有画布、纹理、控制点和父子层级、时间轴里相对静止姿态有变化的通道，以及每条实时轴 30 个采样。和静止值相同的通道不写出。每个文件里的 `notes` 说明平移、骨骼角、伸缩、画面变换、冲量和顶点偏移分别表示什么。

`models/esp.live` 仍是唯一正式测试模型。`models/light.live` 和它的 JSON 只给动作逻辑参考，不能拿去替代正式基准。

重新编译：

```bat
tools\live-inspect\build_msvc.bat
```

需要已安装的 Visual Studio C++ 工具链。编译出的 `live-inspect.exe` 和 `build\` 中间文件分开，中间文件不进 git。
