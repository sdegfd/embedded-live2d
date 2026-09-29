/**
 * Private PainterEngine diagnostic adapter.
 * Application code includes this header only. The .cpp file is the only
 * firmware translation unit allowed to see internal engine headers.
 */
#pragma once

#include "l2d/l2d.h"

#include <stdint.h>

#define L2D_PX_DIAG_LAYER_MAX 256
#define L2D_PX_DIAG_ID_MAX 32
#define L2D_PX_VISUAL_WORDS 34

typedef struct {
    uint32_t triangles;
    uint32_t fragments;
    uint32_t sampler_calls;
    uint32_t four_tap_alpha_zero;
    uint32_t alpha_zero;
    uint32_t alpha_opaque;
    uint32_t alpha_mixed;
} l2d_px_layer_work_t;

typedef struct {
    l2d_px_layer_work_t layers[L2D_PX_DIAG_LAYER_MAX];
    uint32_t layer_count;
    uint32_t covered_pixels;
    uint32_t max_overdraw;
    uint64_t fragment_overdraw_sum;
    uint32_t scanlines;
    uint32_t spans;
    uint32_t span_pixels;
    uint32_t max_span_length;
    uint32_t sample_in;
    uint32_t sample_out;
    uint32_t full_spans;
    uint32_t partial_spans;
} l2d_px_detail_frame_t;

typedef struct {
    int index;
    int parent;
    int depth;
    int children;
    uint32_t rotation_bits;
    uint32_t local_rotation_bits;
    uint32_t scale_bits;
    uint32_t local_tx_bits;
    uint32_t local_ty_bits;
    uint32_t key_x_bits;
    uint32_t key_y_bits;
    char id[L2D_PX_DIAG_ID_MAX];
} l2d_px_visual_layer_t;

#ifdef __cplusplus
extern "C" {
#endif

void l2d_px_detail_begin(void *overdraw, int width, int height);
void l2d_px_detail_get(l2d_px_detail_frame_t *out);
void l2d_px_visual_copy(l2d_instance_t *instance, uint32_t words[L2D_PX_VISUAL_WORDS]);
int l2d_px_visual_topology(l2d_instance_t *instance, l2d_px_visual_layer_t *out, int capacity);

#ifdef __cplusplus
}
#endif
