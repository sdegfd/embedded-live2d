/**
 * Platform services used by the shared PainterEngine-derived runtime.
 * This header is internal. It does not include ESP-IDF, FreeRTOS, or LVGL.
 *
 * l2d_port_alloc is the only system allocator the runtime may call.
 * The Live2D memory pool then serves update/render. Steady-state frames
 * must not call l2d_port_alloc again.
 *
 * Time is optional and is not an animation clock. Passing elapsed time
 * remains the caller's job. l2d_pe_time_us is only for diagnostics.
 */
#ifndef L2D_PE_PORT_H
#define L2D_PE_PORT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *l2d_port_alloc(size_t alignment, size_t size);
void l2d_port_free(void *ptr);

/** When enabled, further alloc calls fail and still increment the counter. */
void l2d_port_alloc_trap(int enable);
uint32_t l2d_port_alloc_calls(void);

void l2d_port_log(int level, const char *tag, const char *fmt, ...);
int64_t l2d_pe_time_us(void);
void l2d_pe_log_info(const char *tag, const char *fmt, ...);

#define L2D_LOG_ERROR 1
#define L2D_LOG_WARN 2
#define L2D_LOG_INFO 3

#define L2D_LOGE(tag, fmt, ...) l2d_port_log(L2D_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define L2D_LOGW(tag, fmt, ...) l2d_port_log(L2D_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define L2D_LOGI(tag, fmt, ...) l2d_port_log(L2D_LOG_INFO, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
