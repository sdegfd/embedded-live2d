# 嵌入式播放库

公共入口是 [`include/l2d/l2d.h`](include/l2d/l2d.h)，不包含 ESP-IDF、LVGL 或编辑器头文件。核心为 C11；`cmake/l2d_sources.cmake` 显式列出编译源文件。

| 目录 | 职责 |
| --- | --- |
| `include/l2d/` | 模型、实例、轴、画布、内存、ROI 的公共 API |
| `src/api/`、`src/engine/` | 生命周期及播放求值 |
| `src/format/` | 有边界检查的原始 `.live` 解码 |
| `src/memory/` | 分配器和对象私有内存池 |
| `src/raster/`、`src/pipeline/` | 精确最近邻光栅、预计算三角形、条带输出、ROI |
| `src/internal/pe/` | 从 PainterEngine 裁剪的私有播放内核 |
| `ports/host/`、`ports/esp_idf/` | 分配、计时、日志及可选光栅分发 |
| `tests/`、`bench/` | 生命周期、像素对照和共享动作场景 |

## 集成约定

1. `l2d_model_load_memory()` 成功后可以释放输入文件字节。模型保留纹理、网格、Timeline、RT30 等只读数据。
2. `l2d_instance_create()` 创建私有姿态、播放游标和缓存。同一模型可共享给多个实例；销毁顺序为实例在前、模型在后。
3. RT30 先调用 `l2d_instance_enter_rt30()`，查找轴句柄，再设置 0..29 样本或连续位置。批量写轴后，下一次 update 统一求值。
4. `l2d_instance_update()` 只更新；`l2d_instance_render_current()` 只绘制；`l2d_pipeline_frame()` 完成一次更新和绘制。API 不自行读取墙上时间。
5. 常规 BGRA 画布由调用方清屏；RGB565 条带入口内部在 SRAM 清屏、以 BGRA 精度合成，最后转 RGB565。不能把 RGB565 输出用作下一层的混合目标。
6. 一个实例的更新与渲染由调用方串行调度。播放稳定阶段不调用系统分配器；创建、销毁与端口初始化不属于稳定阶段。

模型池和实例池的 `reserved` 是向系统实际申请的容量；`used` 是池内已用量。端口 SRAM scratch、帧缓冲和显示驱动内存另计。详见各公共头文件和根目录 README 的 P4 数据。

## 移植

新平台实现 `ports/host/` 中展示的分配、释放、日志和时钟钩子。内存类用于区分只读模型、实例、快速缓存和帧缓冲；平台自行映射到 SRAM/外部 RAM。基础 BGRA 播放不要求 PPA、LVGL 或双核。

可选条带模式启用 `L2D_CFG_RASTER_BATCH`，并实现光栅分发与 RGB565 能力查询；Host 提供串行参考实现，P4 提供双核实现。显示提交、文件系统和硬件缓存同步属于例程/端口层。
