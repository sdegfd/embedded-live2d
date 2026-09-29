/**
 * Immutable Live2D model.
 *
 * A model owns texture pixels, mesh topology, animation payloads, and baked
 * RT30 tables. File bytes passed to l2d_model_load_memory may be freed after
 * L2D_OK. The model does not render and has no playback cursor.
 *
 * Several instances may share one model. Destroy every instance before the
 * model. There is no reference count. Destroying the model first makes the
 * remaining instances invalid.
 *
 * Load is transactional. Failure leaves *out NULL and does not destroy any
 * model the caller already holds. Unload-first is the caller's choice.
 */
#ifndef L2D_MODEL_H
#define L2D_MODEL_H

#include "l2d_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l2d_model l2d_model_t;

typedef struct {
    char id[L2D_ID_MAX];
    int width;
    int height;
    int layer_count;
    int texture_count;
    int animation_count;
    int axis_count;
    size_t pool_bytes;
    size_t pool_free;
} l2d_model_info_t;

l2d_status_t l2d_model_load_memory(const void *bytes, size_t size, l2d_model_t **out);
void l2d_model_destroy(l2d_model_t *model);
void l2d_model_info(const l2d_model_t *model, l2d_model_info_t *out);
int l2d_model_animation_count(const l2d_model_t *model);
int l2d_model_axis_count(const l2d_model_t *model);
int l2d_model_find_axis(const l2d_model_t *model, const char *id);

#ifdef __cplusplus
}
#endif

#endif
