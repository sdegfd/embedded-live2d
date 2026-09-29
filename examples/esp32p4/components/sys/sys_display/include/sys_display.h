/**
 * @file sys_display.h
 * @brief 系统显示模块 - 封装 MIPI-DSI 显示初始化和 LVGL 图形库适配
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 MIPI-DSI 显示并启动 LVGL（使用默认配置）。返回 LVGL 显示句柄，失败返回 NULL。 */
lv_display_t *sys_display_init(void);

/** 使用自定义配置初始化显示（缓冲区大小、双缓冲等）。 */
lv_display_t *sys_display_init_with_config(const bsp_display_cfg_t *cfg);

/** 反初始化显示并停止 LVGL。 */
void sys_display_stop(lv_display_t *disp);

/* ── 背光控制 ─────────────────────────────────────────── */

/** 打开显示背光。 */
esp_err_t sys_display_backlight_on(void);

/** 关闭显示背光。 */
esp_err_t sys_display_backlight_off(void);

/** 设置显示背光亮度。brightness 范围为 0-100。 */
esp_err_t sys_display_backlight_set(int brightness);

/* ── LVGL 互斥锁 ───────────────────────────────────────── */

/** 获取 LVGL 互斥锁。timeout_ms 为超时时间（毫秒）。返回 true 表示锁定成功。 */
bool sys_display_lock(uint32_t timeout_ms);

/** 释放 LVGL 互斥锁。 */
void sys_display_unlock(void);

/* ── 面板句柄（用于直接绘制） ─────────────────────────── */

/** 获取 LCD 面板句柄，用于底层直接绘制操作。 */
esp_lcd_panel_handle_t sys_display_get_panel_handle(void);

/* ── Live2D Overlay（PPA ARGB8888→RGB565 融合） ────────── */

/**
 * @brief 设置当前 Live2D ARGB8888 帧为显示 overlay。
 *
 * 在 LVGL flush 的 custom_draw_bitmap 回调中，PPA 硬件将 ARGB8888 前景
 * （带逐像素 Alpha 透明度）混合到 LVGL 渲染的 RGB565 帧缓冲上，然后统一推送面板。
 * 解决 Live2D 边缘割裂和画布背景透明问题。
 *
 * @param disp      LVGL 显示句柄
 * @param argb_data ARGB8888 像素缓冲（4字节/像素，A,R,G,B 内存顺序，PSRAM）
 * @param x,y,w,h   Overlay 在屏幕上的区域（像素坐标和尺寸）
 * @return ESP_OK 成功
 */
esp_err_t sys_display_set_live2d_overlay(lv_display_t *disp,
                                          const void *argb_data,
                                          int x, int y, int w, int h);

/* ── 屏幕录制捕获回调（渲染完成点） ───────────────────── */

/**
 * @brief 帧捕获回调 — 在 LVGL flush 完成、PPA Live2D 融合后、面板推送前调用。
 *
 * 此时 frame_buffer 是一帧完整的 RGB565 数据（含 Live2D overlay），是抓帧录制的
 * 安全点：无撕裂、无双缓冲前后帧跳变。回调在 LVGL 任务上下文执行，应只做轻量
 * 复制（如 memcpy 到录制缓冲），重活（编码）交由录制任务异步处理。
 *
 * @param frame_buffer 完整帧缓冲指针（RGB565，PSRAM）
 * @param ctx           用户上下文
 */
typedef void (*sys_display_capture_cb_t)(void *frame_buffer, void *ctx);

/**
 * @brief 注册帧捕获回调。传 NULL 取消。回调在 LVGL flush 上下文调用。
 */
void sys_display_set_capture_cb(sys_display_capture_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
