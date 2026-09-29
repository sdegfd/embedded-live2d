/**
 * @file live2d_renderer.h
 * @brief Live2D 渲染器模块（待集成）
 *
 * ── 渲染管线位置 ──
 *   PainterEngine -> live2d_engine（动画/模型） -> live2d_renderer（格式转换） -> 显示缓冲区
 *
 * 将 Live2D 引擎输出的 ARGB8888 像素转换为显示缓冲区所需的 RGB565 格式，
 * 支持 PPA（像素处理加速器）硬件加速转换，并自动选择最佳配置参数。
 *
 * @note 待集成：此模块尚未与 Game UI 框架完全集成，当前为独立运行的渲染器。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/ppa.h"
#include "esp_err.h"
#include "live2d_engine.h"
#include "sys_display_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x, y, w, h;
    bool valid;
} live2d_roi_t;

/** Live2D 渲染器结构体。管理渲染缓冲区和 ARGB8888 -> RGB565 格式转换。 */
typedef struct {
    sys_display_buffer_t *buffer;          /**< 显示缓冲区指针（含 ARGB8888 渲染缓冲和 RGB565 帧缓冲） */
    px_surface render_surface;             /**< PainterEngine 渲染目标 surface（包装 buffer->render_argb8888） */
    uint32_t rendered_frames;              /**< 已渲染帧数计数 */
    uint32_t last_render_us;               /**< 上一帧渲染耗时（微秒） */
    uint32_t last_clear_us;                /**< 上一帧透明清屏耗时（微秒） */
    uint32_t last_clear_fill_us;           /**< 上一帧透明填充/清零耗时（微秒） */
    uint32_t last_clear_sync_us;           /**< 上一帧 PPA 清屏后 cache M2C 同步耗时（微秒） */
    uint32_t last_engine_us;               /**< 上一帧 PainterEngine 渲染耗时（微秒） */
    uint32_t last_convert_us;              /**< 上一帧格式转换耗时（微秒） */
    uint32_t last_convert_sync_us;         /**< ARGB CPU->PPA cache sync */
    uint32_t last_ppa_convert_us;          /**< PPA SRM blocking call */
    uint32_t last_frame_id;                /**< 最近完成的 producer 帧号 */
    uint32_t last_bounds_us;               /**< 当前 dirty ROI 决策耗时 */
    uint32_t avg_render_us;                /**< 平均渲染耗时（微秒） */
    uint32_t avg_clear_us;                 /**< 平均透明清屏耗时（微秒） */
    uint32_t avg_clear_fill_us;            /**< 平均透明填充/清零耗时（微秒） */
    uint32_t avg_clear_sync_us;            /**< 平均 PPA 清屏后 cache M2C 同步耗时（微秒） */
    uint32_t avg_engine_us;                /**< 平均 PainterEngine 渲染耗时（微秒） */
    uint32_t avg_convert_us;               /**< 平均格式转换耗时（微秒） */
    uint64_t accumulated_render_us;        /**< 累计渲染耗时（微秒），用于滑动平均计算 */
    uint64_t accumulated_clear_us;         /**< 累计透明清屏耗时（微秒） */
    uint64_t accumulated_clear_fill_us;    /**< 累计透明填充/清零耗时（微秒） */
    uint64_t accumulated_clear_sync_us;    /**< 累计 PPA 清屏后 cache M2C 同步耗时（微秒） */
    uint64_t accumulated_engine_us;        /**< 累计 PainterEngine 渲染耗时（微秒） */
    uint64_t accumulated_convert_us;       /**< 累计格式转换耗时（微秒） */
    int last_origin_x;                     /**< 上一帧模型绘制 X 原点 */
    int last_origin_y;                     /**< 上一帧模型绘制 Y 原点 */
    int dirty_x;                           /**< 上一帧透明清屏/缓存同步建议区域 X */
    int dirty_y;                           /**< 上一帧透明清屏/缓存同步建议区域 Y */
    int dirty_w;                           /**< 上一帧透明清屏/缓存同步建议区域宽度 */
    int dirty_h;                           /**< 上一帧透明清屏/缓存同步建议区域高度 */
    live2d_roi_t roi_current, roi_previous, roi_union;
    bool roi_force_full;
    bool roi_history_ready;
    ppa_client_handle_t ppa_srm_handle;    /**< PPA SRM（缩放/旋转/镜像）硬件句柄 */
    ppa_client_handle_t ppa_fill_handle;   /**< PPA FILL 透明清屏硬件句柄 */
    bool convert_rgb565;                   /**< 是否需要输出 RGB565 帧缓冲（overlay 路径可关闭） */
    bool use_ppa_convert;                  /**< 是否使用 PPA 硬件加速进行格式转换 */
    bool use_ppa_clear;                    /**< 是否使用 PPA FILL 进行透明清屏 */
    bool ppa_rgb_swap;                     /**< PPA RGB 通道交换标志（R<->B） */
    bool ppa_byte_swap;                    /**< PPA 字节序交换标志（高字节<->低字节） */
    int source_block_x;                    /**< 源块 X 坐标（缩放后居中偏移） */
    int source_block_y;                    /**< 源块 Y 坐标（缩放后居中偏移） */
    int source_block_w;                    /**< 源块宽度（经性能缩放因子计算） */
    int source_block_h;                    /**< 源块高度（经性能缩放因子计算） */
} live2d_renderer_t;

/** 初始化渲染器，绑定到指定的显示缓冲区。
 *
 * @param renderer  渲染器实例指针
 * @param buffer    显示缓冲区指针（需已初始化，含 render_argb8888；overlay 路径可不含 frame_rgb565）
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t live2d_renderer_init(live2d_renderer_t *renderer, sys_display_buffer_t *buffer);

/** 释放渲染器内部 PPA client，缓冲区本身仍由 sys_display_buffer 管理。 */
void live2d_renderer_deinit(live2d_renderer_t *renderer);

/** 设置是否在 render_frame 中生成 RGB565 帧缓冲。
 *
 * LVGL overlay 融合路径直接消费 ARGB8888 + alpha，关闭后可跳过 RGB565 转换
 * 和 PPA RGB565 自检；独立 canvas/test 路径保持默认开启。
 */
void live2d_renderer_set_rgb565_conversion(live2d_renderer_t *renderer, bool enabled);

/** 渲染一帧：调用 Live2D 引擎渲染，并将结果转换到显示缓冲区中。
 *
 * 自动管理 PPA 配置选择（首次帧特殊处理）和性能统计。
 *
 * @param renderer   渲染器实例
 * @param engine     Live2D 引擎实例
 * @param elapsed_ms 距上次渲染的毫秒数，用于驱动动画
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t live2d_renderer_render_frame(live2d_renderer_t *renderer, live2d_engine_t *engine,
                                       uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif
