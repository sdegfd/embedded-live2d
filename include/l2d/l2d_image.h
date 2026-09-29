/**
 * CPU image helpers. These match the ESP32-P4 software RGB565 formula.
 * Alpha is ignored. Do not use this as a direct-to-RGB565 raster.
 */
#ifndef L2D_IMAGE_H
#define L2D_IMAGE_H

#include "l2d_types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t l2d_bgra8888_to_rgb565(uint32_t bgra)
{
    return (uint16_t)(((bgra >> 8) & 0xF800u) |
                      ((bgra >> 5) & 0x07E0u) |
                      ((bgra >> 3) & 0x001Fu));
}

/**
 * Convert a BGRA8888 rectangle to RGB565. Rows may have padding.
 * src_stride and dst_stride are byte strides. Does not allocate.
 * Returns L2D_ERR_RANGE if a row would leave the caller-provided size.
 */
static inline l2d_status_t l2d_convert_bgra_to_rgb565(const void *src, int src_stride,
                                                     size_t src_size, void *dst,
                                                     int dst_stride, size_t dst_size,
                                                     int width, int height)
{
    int y;
    if (!src || !dst || width <= 0 || height <= 0 || src_stride < width * 4 ||
        dst_stride < width * 2) {
        return L2D_ERR_INVALID_ARG;
    }
    if ((size_t)src_stride > src_size || (size_t)dst_stride > dst_size) {
        return L2D_ERR_RANGE;
    }
    if ((size_t)height > (src_size / (size_t)src_stride) ||
        (size_t)height > (dst_size / (size_t)dst_stride)) {
        return L2D_ERR_RANGE;
    }
    for (y = 0; y < height; ++y) {
        const uint8_t *srow = (const uint8_t *)src + (size_t)y * (size_t)src_stride;
        uint8_t *drow = (uint8_t *)dst + (size_t)y * (size_t)dst_stride;
        int x;
        for (x = 0; x < width; ++x) {
            uint32_t bgra;
            uint16_t px;
            memcpy(&bgra, srow + (size_t)x * 4u, 4u);
            px = l2d_bgra8888_to_rgb565(bgra);
            memcpy(drow + (size_t)x * 2u, &px, 2u);
        }
    }
    return L2D_OK;
}

#ifdef __cplusplus
}
#endif

#endif
