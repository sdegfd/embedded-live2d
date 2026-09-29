/**
 * @file sys_display_flush.h
 * @brief 显示刷新模块 - 将 RGB565 帧缓冲通过 MIPI-DSI 或 LVGL 推送到硬件显示
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"
#include "sys_display_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 显示刷新器结构体。管理 LVGL canvas 和帧提交。 */
typedef struct {
    lv_obj_t             *canvas;       /**< LVGL canvas 对象，用于显示渲染输出 */
    sys_display_buffer_t *buffer;       /**< 显示缓冲区指针，提供 RGB565 数据 */
    esp_lcd_panel_handle_t panel;       /**< LCD 面板句柄，用于直接 MIPI-DSI 绘图 */
    int canvas_x;                        /**< Canvas 在屏幕上的 X 坐标 */
    int canvas_y;                        /**< Canvas 在屏幕上的 Y 坐标 */
    uint32_t submitted_frames;           /**< 已提交帧数统计 */
    uint32_t consumed_frame_id;          /**< 本次消费的 producer 帧号 */
    uint32_t panel_frame_id;             /**< 最近一次面板提交的帧号 */
    uint32_t last_cache_sync_us;
    uint32_t last_lock_wait_us;
    uint32_t last_panel_submit_us;
    uint32_t lock_skip_count;
    uint32_t submit_fail_count;
    int64_t consume_begin_us;
    int64_t panel_end_us;
} sys_display_flush_t;

/** 创建基于缓冲区 RGB565 数据的 LVGL canvas。如果提供了面板句柄，
 *  submit() 将使用直接 MIPI-DSI 绘图；否则回退到 LVGL 刷新。 */
esp_err_t sys_display_flush_init(sys_display_flush_t *f,
                                  lv_obj_t *parent,
                                  sys_display_buffer_t *buf,
                                  int canvas_x, int canvas_y);

/** 将当前 RGB565 帧缓冲推送到显示设备。 */
esp_err_t sys_display_flush_submit(sys_display_flush_t *f);

#ifdef __cplusplus
}
#endif
