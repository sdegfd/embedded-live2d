/**
 * Private bridge from a public instance to the playback engine.
 * Firmware application code does not include this header.
 */
#ifndef LIVE2D_ENGINE_INTERNAL_H
#define LIVE2D_ENGINE_INTERNAL_H

#include "l2d/l2d_instance.h"
#include "live2d_engine.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *texture_pixels;
    const void *triangle_indices;
    const void *rt30_samples;
    const void *animation_frame;
    const void *mutable_vertices;
    size_t pool_used;
} l2d_share_view_t;

live2d_engine_t *l2d_instance_internal_engine(l2d_instance_t *instance);
void l2d_model_share_view(const l2d_model_t *model, l2d_share_view_t *out);
void l2d_instance_share_view(const l2d_instance_t *instance, l2d_share_view_t *out);

#ifdef __cplusplus
}
#endif

#endif
