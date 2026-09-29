/**
 * One Live2D playback instance.
 *
 * Ownership: l2d_instance_create allocates the instance and one aligned pool
 * through the linked port allocator. The caller owns the object and destroys
 * it with l2d_instance_destroy. load_memory copies what it needs into the pool;
 * the caller may free the file bytes after a successful load.
 *
 * Load is not transactional. If a model is already loaded, a failed reload
 * leaves the instance empty. Load a replacement into a second instance and
 * switch only after that load returns L2D_OK.
 *
 * Threading: one instance must not be updated and rendered concurrently.
 * Two instances share no mutable state. The caller serializes each instance.
 *
 * l2d_instance_update only advances pose. l2d_instance_render_current only
 * draws. Neither reads a clock. l2d_pipeline_frame is the compatible order
 * used by the ESP32-P4 example: one update plus one draw, matching
 * PX_LiveFrameworkRender. Calling render_current does not advance time.
 *
 * Steady state: after a successful load, update, render_current, and
 * pipeline_frame do not call the system allocator.
 *
 * Errors: a failed call leaves the previous pose in place, except load,
 * which may have already released the previous model.
 */
#ifndef L2D_INSTANCE_H
#define L2D_INSTANCE_H

#include "l2d_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l2d_instance l2d_instance_t;

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
} l2d_instance_info_t;

/**
 * pool_bytes is the single arena used for the model and instance state.
 * 16 MiB matches the ESP32-P4 example. Smaller pools fail at load with
 * L2D_ERR_FAIL or L2D_ERR_NO_MEM instead of failing inside a frame.
 */
l2d_status_t l2d_instance_create(size_t pool_bytes, l2d_instance_t **out);
void l2d_instance_destroy(l2d_instance_t *instance);

l2d_status_t l2d_instance_load_memory(l2d_instance_t *instance, const void *bytes,
                                      size_t size);
bool l2d_instance_is_loaded(const l2d_instance_t *instance);
void l2d_instance_info(const l2d_instance_t *instance, l2d_instance_info_t *out);

bool l2d_instance_enter_rt30(l2d_instance_t *instance);
void l2d_instance_reset_rt30(l2d_instance_t *instance);
int l2d_instance_find_axis(l2d_instance_t *instance, const char *id);
bool l2d_instance_set_axis_position(l2d_instance_t *instance, int handle,
                                    float position, uint16_t weight_q15);

void l2d_instance_update(l2d_instance_t *instance, uint32_t elapsed_ms);
l2d_status_t l2d_instance_render_current(l2d_instance_t *instance,
                                         const l2d_surface_t *surface, int x, int y);
l2d_status_t l2d_pipeline_frame(l2d_instance_t *instance, const l2d_surface_t *surface,
                                int x, int y, uint32_t elapsed_ms);

void l2d_instance_geometry_bounds(const l2d_instance_t *instance,
                                  l2d_geometry_bounds_t *out);
void l2d_instance_frame_profile(const l2d_instance_t *instance, l2d_frame_profile_t *out);
void l2d_instance_trig_cache(const l2d_instance_t *instance, uint32_t *hit, uint32_t *miss);
uint32_t l2d_instance_roi_revision(const l2d_instance_t *instance);

#ifdef __cplusplus
}
#endif

#endif
