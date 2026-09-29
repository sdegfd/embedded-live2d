#include "l2d_pe_port.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static uint32_t l2d_port_caps(l2d_memory_class_t cls)
{
    uint32_t caps = MALLOC_CAP_8BIT;
#if CONFIG_SPIRAM
    /* FAST is the only class that prefers internal RAM. Model, instance,
     * large, scratch, and framebuffer stay on external RAM so this change
     * does not move hot playback state. */
    if (cls == L2D_MEM_FAST) {
        return caps | MALLOC_CAP_INTERNAL;
    }
    (void)cls;
    return caps | MALLOC_CAP_SPIRAM;
#else
    (void)cls;
    return caps;
#endif
}

void *l2d_port_alloc(void *user, l2d_memory_class_t cls, size_t alignment, size_t size)
{
    void *ptr;
    (void)user;
    if (size == 0) {
        return NULL;
    }
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }
    ptr = heap_caps_aligned_alloc(alignment, size, l2d_port_caps(cls));
#if CONFIG_SPIRAM
    if (!ptr && cls == L2D_MEM_FAST) {
        ptr = heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    }
#endif
    return ptr;
}

void l2d_port_free(void *user, l2d_memory_class_t cls, void *ptr)
{
    (void)user;
    (void)cls;
    if (ptr) {
        heap_caps_free(ptr);
    }
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
