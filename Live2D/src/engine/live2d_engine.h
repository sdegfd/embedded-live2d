/**
 * Internal playback facade over the shared runtime.
 * Application code uses include/l2d. PainterEngine types stay in the .c file.
 */
#ifndef LIVE2D_ENGINE_H
#define LIVE2D_ENGINE_H

#include "l2d/l2d_types.h"
#include "l2d/l2d_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct live2d_engine live2d_engine_t;

#define LIVE2D_ENGINE_ID_MAX L2D_ID_MAX
#define LIVE2D_ENGINE_PERF_RENDER_SCALE 1.0f

/** Kernel phase times, microseconds. Field names match the historical logs. */
typedef struct {
    uint32_t poseUs;
    uint32_t physicalUs;
    uint32_t keypointUs;
    uint32_t visualTransformUs;
    uint32_t stretchUs;
    uint32_t vertexTransformUs;
    uint32_t uvUpdateUs;
    uint32_t sortUs;
    uint32_t drawUs;
} live2d_engine_frame_profile_t;

typedef struct {
    char id[LIVE2D_ENGINE_ID_MAX];
    int width;
    int height;
    int layer_count;
    int texture_count;
    int animation_count;
    size_t pool_size;
    size_t pool_free;
} live2d_engine_info_t;

typedef struct {
    uint32_t vertices;
    uint32_t triangles;
    uint32_t texture_pixels;
} live2d_engine_geometry_t;

typedef struct {
    int handle;
    uint8_t sample;
    uint16_t weight_q15;
} live2d_axis_state_t;

typedef struct {
    int axis_count;
    uint32_t static_bytes;
    uint32_t runtime_bytes;
    uint32_t selected_sample_bytes;
} live2d_realtime_stats_t;

/**
 * Allocate one aligned pool of pool_size bytes. Does not zero the pool.
 * The instance object itself is zeroed. Returns L2D_ERR_NO_MEM on failure
 * and does not leak a partial object.
 */
l2d_status_t live2d_engine_create(const l2d_allocator_t *allocator, l2d_memory_class_t cls,
                                 size_t pool_size, live2d_engine_t **out_engine);
void live2d_engine_destroy(live2d_engine_t *engine);

/**
 * Mutable playback clone. Texture pixels, triangle indices, animation frame
 * bytes, and baked RT30 tables alias the source. Vertices and axis runtime
 * state are copied. The source must outlive the clone.
 */
l2d_status_t live2d_engine_clone_shared(const live2d_engine_t *source,
                                        const l2d_allocator_t *allocator,
                                        live2d_engine_t **out_engine);
size_t live2d_engine_mutable_reserve(const live2d_engine_t *engine);
const l2d_allocator_t *live2d_engine_allocator(const live2d_engine_t *engine);

typedef struct {
    const void *texture_pixels;
    const void *triangle_indices;
    const void *rt30_samples;
    const void *animation_frame;
    const void *mutable_vertices;
    size_t pool_used;
} live2d_engine_share_view_t;

void live2d_engine_share_view(const live2d_engine_t *engine,
                              live2d_engine_share_view_t *out);

/**
 * Import a .live image. A failed import leaves a previously loaded model in
 * place and does not reset its pool.
 */
l2d_status_t live2d_engine_load(live2d_engine_t *engine, const void *model_data,
                                size_t model_size);

void live2d_engine_play_default(live2d_engine_t *engine);
bool live2d_engine_play_index(live2d_engine_t *engine, int animation_index);
void live2d_engine_pause_animation(live2d_engine_t *engine);
bool live2d_engine_resume_animation(live2d_engine_t *engine);
void live2d_engine_stop_animation(live2d_engine_t *engine);
int live2d_engine_playback_mode(const live2d_engine_t *engine);
void live2d_engine_play_default_index(live2d_engine_t *engine, int animation_index);
bool live2d_engine_is_animation_finished(const live2d_engine_t *engine);
bool live2d_engine_cycle_animation_if_needed(live2d_engine_t *engine);
int live2d_engine_get_current_animation_index(const live2d_engine_t *engine);
void live2d_engine_set_render_scale(live2d_engine_t *engine, float render_scale);

/**
 * Update and draw one frame into a tight BGRA buffer (width * 4 bytes per row).
 * Alignment is left-top, which is the only alignment the P4 renderer uses.
 * elapsed_ms advances the pose once inside this call.
 */
void live2d_engine_render(live2d_engine_t *engine, void *pixels, int width, int height,
                          int x, int y, uint32_t elapsed_ms);
void live2d_engine_update(live2d_engine_t *engine, uint32_t elapsed_ms);
void live2d_engine_render_current(live2d_engine_t *engine, void *pixels, int width,
                                  int height, int x, int y);

void live2d_engine_get_frame_profile(const live2d_engine_t *engine,
                                     live2d_engine_frame_profile_t *out_profile);
void live2d_engine_get_geometry_bounds(const live2d_engine_t *engine,
                                       l2d_geometry_bounds_t *out);
uint32_t live2d_engine_get_roi_revision(const live2d_engine_t *engine);
void live2d_engine_get_trig_cache(const live2d_engine_t *engine, uint32_t *hit,
                                  uint32_t *miss);
void live2d_engine_get_geometry(const live2d_engine_t *engine,
                                live2d_engine_geometry_t *out);
void live2d_engine_get_info(live2d_engine_t *engine, live2d_engine_info_t *out_info);
bool live2d_engine_is_loaded(const live2d_engine_t *engine);

bool live2d_engine_enter_realtime(live2d_engine_t *engine);
void live2d_engine_leave_realtime(live2d_engine_t *engine);
void live2d_engine_reset_realtime(live2d_engine_t *engine);
int live2d_engine_get_realtime_axis_count(live2d_engine_t *engine);
int live2d_engine_find_realtime_axis(live2d_engine_t *engine, const char *id);
bool live2d_engine_set_axis_sample(live2d_engine_t *engine, int handle, uint8_t sample,
                                   uint16_t weight_q15);
bool live2d_engine_set_axis_position(live2d_engine_t *engine, int handle, float position,
                                     uint16_t weight_q15);
bool live2d_engine_set_axes_batch(live2d_engine_t *engine, const live2d_axis_state_t *states,
                                  size_t count);
void live2d_engine_get_realtime_stats(live2d_engine_t *engine, live2d_realtime_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif
