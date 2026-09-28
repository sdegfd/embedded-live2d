/**
 * @file live2d_engine.h
 * @brief Live2D 引擎封装层（待集成）
 *
 * ── 渲染管线位置 ──
 *   PainterEngine -> live2d_engine（动画/模型管理） -> live2d_renderer（格式转换） -> 显示缓冲区
 *
 * 基于 PainterEngine 实现 Live2D 模型的加载、动画控制和渲染。
 *
 * @note 待集成：此模块尚未与 Game UI 框架完全集成，当前为独立运行的 Live2D 演示模式。
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "PX_LiveFramework.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Live2D 引擎不透明类型。 */
typedef struct live2d_engine live2d_engine_t;

/** 最近一帧 PainterEngine 内核各阶段耗时，单位微秒。 */
typedef PX_LiveFrameworkFrameProfile live2d_engine_frame_profile_t;

/** 渲染器源块缩放比例。测试链路按模型 ROI 刷新，保持 1:1 转换避免无意义裁剪/缩放。 */
#define LIVE2D_ENGINE_PERF_RENDER_SCALE 1.0f

/** Live2D 引擎信息结构体。 */
typedef struct {
    char id[PX_LIVE_ID_MAX_LEN];  /**< 模型标识符（字符串） */
    int width;                     /**< 模型宽度（像素） */
    int height;                    /**< 模型高度（像素） */
    int layer_count;               /**< 图层数量 */
    int texture_count;             /**< 纹理数量 */
    int animation_count;           /**< 动画数量 */
    size_t pool_size;              /**< 内存池总大小（字节） */
    size_t pool_free;              /**< 内存池剩余大小（字节） */
} live2d_engine_info_t;

/** 模型几何规模，用于 benchmark metadata；仅在加载完成后查询。 */
typedef struct {
    uint32_t vertices;
    uint32_t triangles;
    uint32_t texture_pixels;
} live2d_engine_geometry_t;

void live2d_engine_get_geometry(const live2d_engine_t *engine,
                                live2d_engine_geometry_t *out);

/** 创建 Live2D 引擎实例。
 *
 * 在 PSRAM 中分配指定大小的内存池供 PainterEngine 使用。
 *
 * @param pool_size   内存池大小（字节）
 * @param out_engine  输出参数，接收引擎指针
 * @return ESP_OK 成功；ESP_ERR_NO_MEM 内存不足；ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t live2d_engine_create(size_t pool_size, live2d_engine_t **out_engine);

/** 销毁 Live2D 引擎实例并释放资源。
 *
 * @param engine  引擎实例（可为 NULL）
 */
void live2d_engine_destroy(live2d_engine_t *engine);

/** 从内存加载 Live2D 模型数据。
 *
 * @param engine      引擎实例
 * @param model_data  模型二进制数据指针
 * @param model_size  模型数据大小（字节）
 * @return ESP_OK 成功；ESP_FAIL 导入失败；ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t live2d_engine_load(live2d_engine_t *engine, const void *model_data, size_t model_size);

/** 播放默认动画。
 *
 * 查找第一个可播放动画并开始播放；若无可播放动画则渲染静态模型。
 *
 * @param engine  引擎实例
 */
void live2d_engine_play_default(live2d_engine_t *engine);

/** 播放指定索引的动画（0-based，中断当前动画）。索引无效返回 false。 */
bool live2d_engine_play_index(live2d_engine_t *engine, int animation_index);

/** 播放指定索引作为默认动画；索引无效则回退到首个可播放动画。 */
void live2d_engine_play_default_index(live2d_engine_t *engine, int animation_index);

/** 当前动画是否播放完毕（STOP 状态）。 */
bool live2d_engine_is_animation_finished(const live2d_engine_t *engine);

/** 在需要时循环到下一个动画。
 *
 * 若当前动画已停止，自动播放下一个可用的动画，实现无缝循环播放。
 *
 * @param engine  引擎实例
 * @return true 动画已切换；false 未切换
 */
bool live2d_engine_cycle_animation_if_needed(live2d_engine_t *engine);

/** 获取当前正在播放的动画索引。
 *
 * @param engine  引擎实例
 * @return 当前动画索引，无动画时返回 -1
 */
int live2d_engine_get_current_animation_index(const live2d_engine_t *engine);

/** 设置渲染缩放比例。
 *
 * render_scale 范围为 0.1 ~ 1.0，用于性能/质量权衡。
 * 值越小性能越高，但画质越低。
 *
 * @param engine       引擎实例
 * @param render_scale 缩放比例
 */
void live2d_engine_set_render_scale(live2d_engine_t *engine, float render_scale);

/** 渲染模型到指定 surface。
 *
 * @param engine      引擎实例
 * @param surface     目标表面
 * @param x,y         绘制坐标
 * @param align       对齐方式（通常为 PX_ALIGN_LEFTTOP）
 * @param elapsed_ms  距上次渲染的毫秒数，驱动动画进度
 */
void live2d_engine_render(live2d_engine_t *engine, px_surface *surface, int x, int y,
                          PX_ALIGN align, uint32_t elapsed_ms);

/** 读取最近一次 render 的内核阶段耗时；未加载模型时返回全零。 */
void live2d_engine_get_frame_profile(const live2d_engine_t *engine,
                                     live2d_engine_frame_profile_t *out_profile);

/** 获取引擎信息（模型尺寸、图层数、内存使用等）。
 *
 * @param engine    引擎实例
 * @param out_info  输出参数，接收引擎信息
 */
void live2d_engine_get_info(live2d_engine_t *engine, live2d_engine_info_t *out_info);

/** 检查模型是否已加载。
 *
 * @param engine  引擎实例
 * @return true 模型已加载；false 未加载
 */
bool live2d_engine_is_loaded(const live2d_engine_t *engine);

/* ── RT30 实时多轴接口 ──────────────────────────
 * 业务层只使用以下稳定封装，不直接访问 engine->live.realtime.axes。
 * sample 范围 0~29，weight 范围 0~32767（Q15）。
 * 参数输入可按 30Hz 更新，渲染帧率独立；参数不变时复用上一姿态。
 */

/** 单轴状态：用于批量提交。 */
typedef struct {
    int handle;          /**< 轴句柄（find_realtime_axis 返回值） */
    uint8_t sample;      /**< 选中样本索引 0~29 */
    uint16_t weight_q15; /**< Q15 权重 0~32767 */
} live2d_axis_state_t;

/** RT30 内存与运行统计。 */
typedef struct {
    int axis_count;          /**< 已安装轴数 */
    uint32_t static_bytes;   /**< 静态样本/binding/索引字节数 */
    uint32_t runtime_bytes;  /**< 运行累加器字节数 */
    uint32_t selected_sample_bytes; /**< 一次求值读取的选中样本字节数 */
} live2d_realtime_stats_t;

/** 进入 RT30 实时模式（先 Reset Timeline，再切 REALTIME30）。 */
bool live2d_engine_enter_realtime(live2d_engine_t *engine);

/** 退出 RT30 实时模式，恢复 NEUTRAL。 */
void live2d_engine_leave_realtime(live2d_engine_t *engine);

/** 复位所有实时轴到默认样本、权重 0。 */
void live2d_engine_reset_realtime(live2d_engine_t *engine);

/** 获取已安装实时轴数量（模型无 RT30 尾部时为 0）。 */
int live2d_engine_get_realtime_axis_count(live2d_engine_t *engine);

/** 按字符串 ID 查找实时轴，返回整数 handle（未找到返回 -1）。
 *  建议初始化时查找一次并缓存 handle，高频循环不要重复字符串比较。 */
int live2d_engine_find_realtime_axis(live2d_engine_t *engine, const char *id);

/** 设置单轴选中样本与 Q15 权重。 */
bool live2d_engine_set_axis_sample(live2d_engine_t *engine, int handle, uint8_t sample,
                                   uint16_t weight_q15);

/** 设置连续 RT30 位置 q∈[0,29]；相邻烘焙样本在求值时插值。 */
bool live2d_engine_set_axis_position(live2d_engine_t *engine, int handle, float position,
                                     uint16_t weight_q15);

/** 批量提交多轴：先写全部 sample/weight，求值在下次渲染时统一触发一次。
 *  不能每写一个轴就刷新整套模型。 */
bool live2d_engine_set_axes_batch(live2d_engine_t *engine, const live2d_axis_state_t *states,
                                  size_t count);

/** 获取 RT30 内存统计。 */
void live2d_engine_get_realtime_stats(live2d_engine_t *engine, live2d_realtime_stats_t *out);

#ifdef __cplusplus
}
#endif
