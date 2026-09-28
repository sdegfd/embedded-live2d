/**
 * @file sys_display_flush.c
 * @brief 显示刷新模块实现
 *
 * 本模块实现将RGB565帧缓冲区数据刷新到显示面板的功能。
 * 支持两种刷新路径：
 *   1. 直接MIPI-DSI面板绘制（优先）
 *   2. LVGL canvas刷新（回退方案）
 * 在提交前执行cache写回操作以确保DMA可见性。
 */

#include "sys_display_flush.h"

#include <string.h>

#include "esp_cache.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sys_display.h"

static const char *TAG = "sys_display_flush"; /**< 日志标签 */

#define SYS_DISPLAY_FLUSH_LOG_PERIOD 300  /**< 帧提交日志输出间隔（每N帧输出一次） */

/* ── 初始化 ──────────────────────────────────────────── */

/** 初始化显示刷新器：创建LVGL canvas并绑定RGB565帧缓冲区。 */
esp_err_t sys_display_flush_init(sys_display_flush_t *f,
                                  lv_obj_t *parent,
                                  sys_display_buffer_t *buf,
                                  int canvas_x, int canvas_y)
{
    if (!f || !parent || !buf || !buf->frame_rgb565) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(f, 0, sizeof(*f));
    f->buffer   = buf;
    f->panel    = sys_display_get_panel_handle();
    f->canvas_x = canvas_x;
    f->canvas_y = canvas_y;

    f->canvas = lv_canvas_create(parent);
    if (!f->canvas) {
        ESP_LOGE(TAG, "lv_canvas_create failed");
        return ESP_FAIL;
    }

    lv_canvas_set_buffer(f->canvas, buf->frame_rgb565,
                         buf->width, buf->height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(f->canvas, canvas_x, canvas_y);
    lv_obj_set_size(f->canvas, buf->width, buf->height);
    lv_obj_move_background(f->canvas);

    const char *path = f->panel ? "panel direct draw" : "LVGL canvas";
    ESP_LOGI(TAG, "Flush path: %dx%d at (%d,%d) -> %s",
             buf->width, buf->height, canvas_x, canvas_y, path);
    return ESP_OK;
}

/* ── 帧提交 ──────────────────────────────────────────── */

/** 提交一帧显示数据：执行cache写回、获取显示锁、通过面板或canvas刷新到屏幕。 */
esp_err_t sys_display_flush_submit(sys_display_flush_t *f)
{
    if (!f || !f->canvas || !f->buffer || !f->buffer->frame_rgb565) {
        return ESP_ERR_INVALID_ARG;
    }

    f->consume_begin_us = esp_timer_get_time();
    f->consumed_frame_id = f->buffer->frame_id;
    f->last_cache_sync_us = 0;
    f->last_lock_wait_us = 0;
    f->last_panel_submit_us = 0;
    int64_t stage_begin_us = esp_timer_get_time();

    /* 写回cache，确保DMA能读到最新的帧数据 */
    esp_cache_msync(f->buffer->frame_rgb565, f->buffer->frame_bytes,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    f->last_cache_sync_us = (uint32_t)(esp_timer_get_time() - stage_begin_us);

    stage_begin_us = esp_timer_get_time();
    if (!sys_display_lock(100)) {
        f->last_lock_wait_us = (uint32_t)(esp_timer_get_time() - stage_begin_us);
        f->lock_skip_count++;
        ESP_LOGW(TAG, "submit skipped: display lock timeout");
        return ESP_ERR_TIMEOUT;
    }
    f->last_lock_wait_us = (uint32_t)(esp_timer_get_time() - stage_begin_us);
    stage_begin_us = esp_timer_get_time();

    if (f->panel) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(
            f->panel,
            f->canvas_x,
            f->canvas_y,
            f->canvas_x + f->buffer->width,
            f->canvas_y + f->buffer->height,
            f->buffer->frame_rgb565);
        sys_display_unlock();
        f->last_panel_submit_us = (uint32_t)(esp_timer_get_time() - stage_begin_us);
        if (ret != ESP_OK) {
            f->submit_fail_count++;
            ESP_LOGW(TAG, "panel draw failed: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        lv_display_t *disp = lv_obj_get_display(f->canvas);
        lv_obj_invalidate(f->canvas);
        if (disp) lv_refr_now(disp);
        sys_display_unlock();
        f->last_panel_submit_us = (uint32_t)(esp_timer_get_time() - stage_begin_us);
    }

    f->panel_frame_id = f->consumed_frame_id;
    f->panel_end_us = esp_timer_get_time();
    f->submitted_frames++;
    if ((f->submitted_frames % SYS_DISPLAY_FLUSH_LOG_PERIOD) == 0) {
        ESP_LOGI(TAG, "frame submitted: %u", (unsigned)f->submitted_frames);
    }
    return ESP_OK;
}
