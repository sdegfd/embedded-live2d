/**
 * @file sys_monitor.h
 * @brief 系统监控模块 - 提供 CPU 使用率和 FPS 统计功能
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 启动系统监控。创建每个核心的空闲计数任务并开始校准。在繁重任务饱和核心之前调用一次。 */
esp_err_t sys_monitor_start(void);

/** 关联 LVGL 显示设备用于 FPS 追踪。注册 LV_EVENT_RENDER_READY 回调以统计实际显示刷新次数。 */
void sys_monitor_attach_display(lv_display_t *disp);

/** 获取指定核心（0 或 1）的最新 CPU 使用率。返回 -1 表示尚无读数，否则返回 0-100（%）。 */
int sys_monitor_get_cpu_usage(int core);

/** 获取最新 FPS 值（乘以 10，例如 245 表示 24.5 fps）。返回 -1 表示尚未计算出 FPS。 */
int sys_monitor_get_fps_x10(void);

/**
 * @brief 注册一个任务到栈水位监测表。
 *
 * 监控任务每轮打印时会解引用 handle_ptr，对非空句柄调用 uxTaskGetStackHighWaterMark
 * 打印其历史最小剩余栈（字节）。句柄可为 NULL（任务尚未创建或已销毁），此时跳过该条目。
 *
 * 用于 internal SRAM 调优：观测各任务真实栈峰值，为降栈提供依据。
 * handle_ptr 指向各组件 static 的 FreeRTOS 任务句柄（TaskHandle_t*）；以 void* 传递以避免
 * 在本头文件中引入 freertos/task.h。句柄指针必须指向长期存活（至少任务存活期）的存储。
 *
 * @param name 任务显示名（会被复制，调用方无需保活）
 * @param handle_ptr 指向任务句柄的指针（实际类型为 TaskHandle_t*）
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 表已满或内存不足，ESP_ERR_INVALID_ARG 参数非法
 */
esp_err_t sys_monitor_register_task(const char *name, void *handle_ptr);

/**
 * @brief 立即打印一次内存余量与各注册任务的栈水位。
 *
 * 可在页面切换等关键点主动调用，捕捉瞬时峰值。monitor_log_task 也会周期性调用。
 */
void sys_monitor_log_task_watermarks(void);

#ifdef __cplusplus
}
#endif
