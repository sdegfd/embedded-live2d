/**
 * @file sys_display_buffer.c
 * @brief 显示缓冲区管理实现
 *
 * 本模块管理显示系统使用的帧缓冲区，包括：
 * - ARGB8888渲染缓冲区（PSRAM分配）
 * - RGB565帧缓冲区（PSRAM分配）
 * - DMA bounce缓冲区（当PSRAM不支持DMA时，在内部SRAM中分配）
 */

#include "sys_display_buffer.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

static const char *TAG = "sys_display_buf"; /**< 日志标签 */

/* ── PSRAM日志 ──────────────────────────────────────────── */

/** 记录PSRAM使用情况日志。 */
void sys_display_buffer_log_psram(const char *stage)
{
    size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "PSRAM %s: total=%u  free=%u largest=%u",
             stage ? stage : "",
             (unsigned)total, (unsigned)free, (unsigned)largest);
}

static esp_err_t sys_display_buffer_create_common(int width, int height,
                                                  sys_display_buffer_t *out,
                                                  bool alloc_rgb565)
{
    if (!out || width <= 0 || height <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

#ifndef CONFIG_SPIRAM
    ESP_LOGE(TAG, "PSRAM not enabled; framebuffer allocation refused");
    return ESP_ERR_INVALID_STATE;
#endif

    memset(out, 0, sizeof(*out));
    out->width  = width;
    out->height = height;
    out->render_bytes = (size_t)width * (size_t)height * SYS_DISPLAY_BUFFER_ARGB_BPP;
    out->frame_bytes  = alloc_rgb565 ? (size_t)width * (size_t)height * sizeof(uint16_t) : 0;

#if SOC_PSRAM_DMA_CAPABLE
    out->psram_dma_capable = true;
#else
    out->psram_dma_capable = false;
#endif

    sys_display_buffer_log_psram("before alloc");

    out->render_argb8888 = heap_caps_aligned_alloc(
        64, out->render_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  /* 64字节对齐，满足DMA要求 */
    if (!out->render_argb8888) {
        ESP_LOGE(TAG, "render buffer alloc failed: %u bytes", (unsigned)out->render_bytes);
        sys_display_buffer_destroy(out);
        return ESP_ERR_NO_MEM;
    }

    if (alloc_rgb565) {
        out->frame_rgb565 = heap_caps_aligned_alloc(
            64, out->frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  /* 64字节对齐，满足DMA要求 */
        if (!out->frame_rgb565) {
            ESP_LOGE(TAG, "frame buffer alloc failed: %u bytes", (unsigned)out->frame_bytes);
            sys_display_buffer_destroy(out);
            return ESP_ERR_NO_MEM;
        }
    }

    if (alloc_rgb565 && !out->psram_dma_capable) {
        size_t rows = 16;
        out->dma_bounce_bytes = (size_t)width * rows * sizeof(uint16_t);
        out->dma_bounce = heap_caps_aligned_alloc(
            64, out->dma_bounce_bytes,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (!out->dma_bounce) {
            ESP_LOGE(TAG, "DMA bounce buffer alloc failed: %u bytes",
                     (unsigned)out->dma_bounce_bytes);
            sys_display_buffer_destroy(out);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "PSRAM !DMA -> SRAM bounce buffer: %u bytes (16 rows)",
                 (unsigned)out->dma_bounce_bytes);
    }

    ESP_LOGI(TAG, "buffers created: %dx%d  ARGB=%u  RGB565=%u bytes (PSRAM DMA=%s)",
             width, height,
             (unsigned)out->render_bytes, (unsigned)out->frame_bytes,
             out->psram_dma_capable ? "yes" : "no");
    sys_display_buffer_log_psram("after alloc");
    return ESP_OK;
}

/** 创建显示缓冲区：在PSRAM中分配ARGB8888渲染缓冲区和RGB565帧缓冲区，
 *  若PSRAM不支持DMA则额外在内部SRAM分配DMA bounce缓冲区。 */
esp_err_t sys_display_buffer_create(int width, int height,
                                     sys_display_buffer_t *out)
{
    return sys_display_buffer_create_common(width, height, out, true);
}

/** 仅创建 ARGB8888 渲染缓冲区，用于 PPA alpha overlay 消费路径。 */
esp_err_t sys_display_buffer_create_argb_only(int width, int height,
                                              sys_display_buffer_t *out)
{
    return sys_display_buffer_create_common(width, height, out, false);
}

/** 销毁显示缓冲区，释放所有分配的内存。 */
void sys_display_buffer_destroy(sys_display_buffer_t *buf)
{
    if (!buf) return;
    if (buf->render_argb8888) heap_caps_free(buf->render_argb8888);
    if (buf->frame_rgb565)    heap_caps_free(buf->frame_rgb565);
    if (buf->dma_bounce)      heap_caps_free(buf->dma_bounce);
    memset(buf, 0, sizeof(*buf));
}
