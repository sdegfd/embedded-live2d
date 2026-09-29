#include "l2d_pe_port.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static uint32_t g_alloc_calls;
static int g_alloc_trap;

void *l2d_port_alloc(size_t alignment, size_t size)
{
    uint32_t caps = MALLOC_CAP_8BIT;
    g_alloc_calls++;
    if (g_alloc_trap || size == 0) {
        return NULL;
    }
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }
#if CONFIG_SPIRAM
    /* P4 production pools are larger than internal RAM. S3 builds without
     * SPIRAM stay on internal RAM and must not invent an external size. */
    caps |= MALLOC_CAP_SPIRAM;
#endif
    return heap_caps_aligned_alloc(alignment, size, caps);
}

void l2d_port_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

void l2d_port_alloc_trap(int enable)
{
    g_alloc_trap = enable ? 1 : 0;
}

uint32_t l2d_port_alloc_calls(void)
{
    return g_alloc_calls;
}

void l2d_port_log(int level, const char *tag, const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (level <= L2D_LOG_ERROR) {
        ESP_LOGE(tag ? tag : "l2d", "%s", line);
    } else if (level == L2D_LOG_WARN) {
        ESP_LOGW(tag ? tag : "l2d", "%s", line);
    } else {
        ESP_LOGI(tag ? tag : "l2d", "%s", line);
    }
}

void l2d_pe_log_info(const char *tag, const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    ESP_LOGI(tag ? tag : "l2d", "%s", line);
}

int64_t l2d_pe_time_us(void)
{
    return esp_timer_get_time();
}
