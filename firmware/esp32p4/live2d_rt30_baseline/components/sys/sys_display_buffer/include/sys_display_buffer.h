/**
 * @file sys_display_buffer.h
 * @brief 显示缓冲区模块 - 管理 PSRAM 中的 ARGB8888 渲染目标和 RGB565 帧缓冲
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ARGB8888 每个像素的字节数。 */
#define SYS_DISPLAY_BUFFER_ARGB_BPP 4

/** 显示缓冲区结构体。包含渲染目标、帧缓冲和 DMA bounce buffer。 */
typedef struct {
    uint32_t frame_id;            /**< Producer 写入并随缓冲传递到显示提交的帧号 */
    int width;                    /**< 缓冲区宽度（像素） */
    int height;                   /**< 缓冲区高度（像素） */
    void     *render_argb8888;    /**< ARGB8888 渲染目标缓冲区（PSRAM 中） */
    uint16_t *frame_rgb565;       /**< RGB565 帧缓冲区（PSRAM 中） */
    void     *dma_bounce;         /**< SRAM bounce 缓冲区（PSRAM 不支持 DMA 时使用） */
    size_t    render_bytes;       /**< 渲染缓冲区字节数 */
    size_t    frame_bytes;        /**< 帧缓冲区字节数 */
    size_t    dma_bounce_bytes;   /**< DMA bounce 缓冲区字节数 */
    bool      psram_dma_capable;  /**< PSRAM 是否支持 DMA 访问 */
} sys_display_buffer_t;

/** 在 PSRAM 中分配 ARGB8888 + RGB565 双缓冲区。如果芯片 PSRAM 不支持 DMA，则回退到 SRAM bounce 缓冲区。 */
esp_err_t sys_display_buffer_create(int width, int height,
                                    sys_display_buffer_t *out);

/** 仅分配 ARGB8888 渲染缓冲区，适用于直接消费 alpha overlay 的路径。 */
esp_err_t sys_display_buffer_create_argb_only(int width, int height,
                                              sys_display_buffer_t *out);

/** 销毁显示缓冲区并释放内存。 */
void sys_display_buffer_destroy(sys_display_buffer_t *buf);

/** 记录 PSRAM 使用情况日志，stage 为标识阶段名称。 */
void sys_display_buffer_log_psram(const char *stage);

#ifdef __cplusplus
}
#endif
