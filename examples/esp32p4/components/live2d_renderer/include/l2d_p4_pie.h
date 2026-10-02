#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact BGRA8888 -> RGB565, with scalar alignment prefix and tail. */
void l2d_p4_pie_rgb565(const uint32_t *source, uint16_t *target, size_t count);
void l2d_p4_pie_zero(void *target, size_t bytes);
bool l2d_p4_pie_selftest(void);
#ifdef __cplusplus
}
#endif
