# Live2D / PainterEngine 进度仓库

仓库根目录是 `D:\live2d`。用 Git 保存进度时，只提交源码、模型和说明。编译出来的程序、中间文件和旧的 `PainterEngine_make` 整包留在本机，不进入仓库。

当前要改的是这两处：

- `PainterEngine`：引擎。Windows 编辑器链接这里。
- `project/PainterEngine_LiveEditor`：正在使用的编辑器。`build_msvc.bat` 会编译它。

不要用 `PainterEngine_LiveEditor_source` 覆盖 `project/PainterEngine_LiveEditor`。前者是更早的、只有时间轴的副本。

编辑器编译：

```text
D:\live2d\project\PainterEngine_LiveEditor\build_msvc.bat
```

无窗口性能程序：

```text
D:\live2d\project\PainterEngine_LiveEditor\tests\build_live_bench.bat
D:\live2d\project\PainterEngine_LiveEditor\tests\bench_build\live_bench.exe D:\live2d\project\esp.live
```

进度说明在 `project/PainterEngine_新对话交接上下文.md`。
