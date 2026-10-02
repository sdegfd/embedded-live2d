/**
 * One Live2D playback instance.
 *
 * l2d_instance_create borrows an immutable model. The model must outlive the
 * instance. Destroy the instance first, then the model. Two instances of one
 * model share texture pixels, triangle indices, animation payloads, and baked
 * RT30 tables. Vertices, parameters, trig caches, and RT30 accumulators are
 * private to each instance.
 *
 * Threading: one instance must not be updated and rendered concurrently.
 * Different instances share no mutable runtime state. The caller serializes
 * each instance. The model may be read by those instances concurrently only
 * when no instance is being created or destroyed.
 *
 * l2d_instance_update only advances pose. l2d_instance_render_current only
 * draws. Neither reads a clock. l2d_pipeline_frame is the compatible order
 * used by the ESP32-P4 example: one update plus one draw.
 * Calling render_current does not advance time.
 *
 * Steady state: after create, update, render_current, and pipeline_frame do
 * not call the system allocator.
 *
 * Errors: a failed parameter write leaves the previous pose in place.
 */
#ifndef L2D_INSTANCE_H
#define L2D_INSTANCE_H

#include "l2d_types.h"
#include "l2d_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l2d_instance l2d_instance_t;

typedef enum {
    L2D_PLAYBACK_NEUTRAL = 0,
    L2D_PLAYBACK_TIMELINE = 1,
    L2D_PLAYBACK_RT30 = 2
} l2d_playback_mode_t;

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

/** One RT30 axis write. sample is a key index, not a continuous position. */
typedef struct {
    int handle;
    uint8_t sample;
    uint16_t weight_q15;
} l2d_axis_state_t;

typedef struct {
    uint32_t vertices;
    uint32_t triangles;
    uint32_t texture_pixels;
} l2d_mesh_counts_t;

typedef struct {
    int axis_count;
    uint32_t static_bytes;
    uint32_t runtime_bytes;
    uint32_t selected_sample_bytes;
} l2d_rt30_stats_t;

/**
 * Create a playback instance that shares model's immutable resources.
 * pool bytes reported by l2d_instance_info are the mutable arena only.
 */
l2d_status_t l2d_instance_create(l2d_model_t *model, const l2d_instance_config_t *config,
                                 l2d_instance_t **out);
void l2d_instance_memory_requirements(const l2d_instance_t *instance,
                                      l2d_memory_requirements_t *out);
void l2d_instance_destroy(l2d_instance_t *instance);

bool l2d_instance_is_loaded(const l2d_instance_t *instance);
void l2d_instance_info(const l2d_instance_t *instance, l2d_instance_info_t *out);

/** Timeline playback uses the model's imported animation frames. */
bool l2d_instance_play_animation(l2d_instance_t *instance, int animation_index);
void l2d_instance_pause_animation(l2d_instance_t *instance);
bool l2d_instance_resume_animation(l2d_instance_t *instance);
void l2d_instance_stop_animation(l2d_instance_t *instance);
l2d_playback_mode_t l2d_instance_playback_mode(const l2d_instance_t *instance);
int l2d_instance_current_animation(const l2d_instance_t *instance);

bool l2d_instance_enter_rt30(l2d_instance_t *instance);
void l2d_instance_reset_rt30(l2d_instance_t *instance);
int l2d_instance_find_axis(l2d_instance_t *instance, const char *id);
bool l2d_instance_set_axis_position(l2d_instance_t *instance, int handle,
                                    float position, uint16_t weight_q15);
bool l2d_instance_set_axis_sample(l2d_instance_t *instance, int handle,
                                  uint8_t sample, uint16_t weight_q15);
/**
 * Validate every handle, then write sample indices without evaluating.
 * The next update/pipeline_frame evaluates once. count above 64 is rejected.
 * Does not allocate.
 */
bool l2d_instance_set_axes_batch(l2d_instance_t *instance, const l2d_axis_state_t *states,
                                 size_t count);
void l2d_instance_set_render_scale(l2d_instance_t *instance, float render_scale);
int l2d_instance_axis_count(const l2d_instance_t *instance);
void l2d_instance_rt30_stats(l2d_instance_t *instance, l2d_rt30_stats_t *out);
void l2d_instance_mesh_counts(const l2d_instance_t *instance, l2d_mesh_counts_t *out);

void l2d_instance_update(l2d_instance_t *instance, uint32_t elapsed_ms);
l2d_status_t l2d_instance_render_current(l2d_instance_t *instance,
                                         const l2d_surface_t *surface, int x, int y);
/** Terminal RGB565 output is optional: requires prepared ordered mesh jobs and
 * port scratch support. Normal BGRA output remains available independently. */
bool l2d_instance_can_render_rgb565(const l2d_instance_t *instance);
/** Render current pose using full BGRA precision within each scratch band,
 * then pack exactly the same RGB565 pixels as the normal software converter.
 * Clears to transparent black internally. Does not update animation or allocate.
 * target: tight RGB565, 2-byte aligned, width <=8192. Optional capture: distinct
 * tight BGRA buffer of identical dimensions, for untimed correctness/debugging.
 * Unsupported models/ports are rejected before writing either buffer. */
l2d_status_t l2d_instance_render_rgb565(l2d_instance_t *instance,
    const l2d_surface_t *target,int x,int y,const l2d_surface_t *capture);

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
