/**
 * Public types for the Live2D embedded runtime.
 * No platform SDK, RTOS, UI toolkit, or editor-engine headers.
 *
 * Pixel contract for L2D_PIXEL_BGRA8888_LE:
 *   memory bytes [B, G, R, A]
 *   uint32 little-endian value = B | (G<<8) | (R<<16) | (A<<24)
 *   RGB is already composited by the software raster. Alpha is coverage
 *   of the destination pixel, not a second multiply at RGB565 conversion.
 *
 * L2D_PIXEL_RGB565_LE:
 *   uint16 bits RRRRRGGGGGGBBBBB, stored little-endian.
 *   Conversion from BGRA does not multiply by alpha again.
 *
 * Embedded-nearest currently requires a tight BGRA stride
 * (stride_bytes == width * 4). A looser stride returns L2D_ERR_UNSUPPORTED.
 */
#ifndef L2D_TYPES_H
#define L2D_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    L2D_OK = 0,
    L2D_ERR_INVALID_ARG = 1,
    L2D_ERR_NO_MEM = 2,
    L2D_ERR_FAIL = 3,
    L2D_ERR_UNSUPPORTED = 4,
    L2D_ERR_RANGE = 5
} l2d_status_t;

typedef enum {
    L2D_PIXEL_BGRA8888_LE = 1,
    L2D_PIXEL_RGB565_LE = 2
} l2d_pixel_format_t;

/** Numeric profile implemented by this runtime. Not a claim of pure float32. */
#define L2D_NUMERIC_PROFILE_EMBEDDED_COMPAT_NEAREST 1

#define L2D_ID_MAX 32

typedef struct {
    void *data;
    int width;
    int height;
    int stride_bytes;
    size_t buffer_size_bytes;
    l2d_pixel_format_t format;
} l2d_surface_t;

/**
 * Screen-space geometry of vertices submitted to the raster.
 * valid=0 and unsafe=0 is a legal empty frame.
 * unsafe means a non-finite coordinate; callers must treat the range as unknown.
 */
typedef struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    int valid;
    int unsafe;
} l2d_geometry_bounds_t;

/** Half-open integer rect [x, x+w) x [y, y+h). valid=0 means empty. */
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int valid;
} l2d_roi_rect_t;

typedef struct {
    uint32_t pose_us;
    uint32_t physical_us;
    uint32_t keypoint_us;
    uint32_t visual_transform_us;
    uint32_t stretch_us;
    uint32_t vertex_transform_us;
    uint32_t uv_update_us;
    uint32_t sort_us;
    uint32_t draw_us;
} l2d_frame_profile_t;

#ifdef __cplusplus
}
#endif

#endif
