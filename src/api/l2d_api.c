#include "l2d/l2d.h"

#include "live2d_engine.h"
#include "live2d_engine_internal.h"
#include "l2d_allocator.h"
#include "l2d_pe_port.h"

#include <string.h>

struct l2d_model {
    l2d_allocator_t allocator;
    live2d_engine_t *engine;
};

struct l2d_instance {
    l2d_model_t *model;
    live2d_engine_t *engine;
};

#define L2D_MODEL_POOL_BYTES (16u * 1024u * 1024u)

static void l2d_copy_share_view(l2d_share_view_t *out, const live2d_engine_share_view_t *src)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!src) {
        return;
    }
    out->texture_pixels = src->texture_pixels;
    out->triangle_indices = src->triangle_indices;
    out->rt30_samples = src->rt30_samples;
    out->animation_frame = src->animation_frame;
    out->mutable_vertices = src->mutable_vertices;
    out->pool_used = src->pool_used;
}

static void l2d_fill_used_bound(l2d_bytes_bound_t *bound, size_t used, size_t reserved)
{
    memset(bound, 0, sizeof(*bound));
    bound->minimum = used;
    bound->recommended = used;
    bound->upper_bound = reserved > used ? reserved : used;
}

l2d_status_t l2d_model_load_memory(const l2d_model_config_t *config, const void *bytes, size_t size,
                                   l2d_model_t **out)
{
    l2d_model_t *model;
    l2d_status_t status;
    const l2d_allocator_t *given = config ? config->allocator : NULL;
    if (!out) {
        return L2D_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!bytes || size == 0) {
        return L2D_ERR_INVALID_ARG;
    }
    model = (l2d_model_t *)l2d_heap_alloc(given, L2D_MEM_MODEL, sizeof(void *), sizeof(*model));
    if (!model) {
        return L2D_ERR_NO_MEM;
    }
    memset(model, 0, sizeof(*model));
    l2d_allocator_resolve(given, &model->allocator);
    status = live2d_engine_create(&model->allocator, L2D_MEM_MODEL, L2D_MODEL_POOL_BYTES,
                                  &model->engine);
    if (status != L2D_OK) {
        l2d_heap_free(&model->allocator, L2D_MEM_MODEL, model);
        return status;
    }
    status = live2d_engine_load(model->engine, bytes, size);
    if (status != L2D_OK) {
        live2d_engine_destroy(model->engine);
        l2d_heap_free(&model->allocator, L2D_MEM_MODEL, model);
        return status;
    }
    *out = model;
    return L2D_OK;
}

void l2d_model_destroy(l2d_model_t *model)
{
    l2d_allocator_t allocator;
    if (!model) {
        return;
    }
    allocator = model->allocator;
    live2d_engine_destroy(model->engine);
    l2d_heap_free(&allocator, L2D_MEM_MODEL, model);
}

void l2d_model_memory_requirements(const l2d_model_t *model, l2d_memory_requirements_t *out)
{
    live2d_engine_info_t info;
    size_t used = 0;
    size_t reserve = 0;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!model || !model->engine) {
        return;
    }
    live2d_engine_get_info(model->engine, &info);
    if (info.pool_size >= info.pool_free) {
        used = info.pool_size - info.pool_free;
    }
    reserve = live2d_engine_mutable_reserve(model->engine);
    l2d_fill_used_bound(&out->persistent_immutable, used, info.pool_size);
    out->per_instance_mutable.minimum = 0;
    out->per_instance_mutable.recommended = reserve;
    out->per_instance_mutable.upper_bound = reserve;
}

void l2d_model_info(const l2d_model_t *model, l2d_model_info_t *out)
{
    live2d_engine_info_t info;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!model) {
        return;
    }
    live2d_engine_get_info(model->engine, &info);
    memcpy(out->id, info.id, sizeof(out->id));
    out->width = info.width;
    out->height = info.height;
    out->layer_count = info.layer_count;
    out->texture_count = info.texture_count;
    out->animation_count = info.animation_count;
    out->axis_count = live2d_engine_get_realtime_axis_count(model->engine);
    out->pool_bytes = info.pool_size;
    out->pool_free = info.pool_free;
}

int l2d_model_animation_count(const l2d_model_t *model)
{
    live2d_engine_info_t info;
    if (!model) {
        return 0;
    }
    live2d_engine_get_info(model->engine, &info);
    return info.animation_count;
}

int l2d_model_axis_count(const l2d_model_t *model)
{
    return model ? live2d_engine_get_realtime_axis_count(model->engine) : 0;
}

int l2d_model_find_axis(const l2d_model_t *model, const char *id)
{
    if (!model) {
        return -1;
    }
    return live2d_engine_find_realtime_axis(model->engine, id);
}

void l2d_model_share_view(const l2d_model_t *model, l2d_share_view_t *out)
{
    live2d_engine_share_view_t view;
    if (!model) {
        l2d_copy_share_view(out, NULL);
        return;
    }
    live2d_engine_share_view(model->engine, &view);
    l2d_copy_share_view(out, &view);
}

void l2d_instance_share_view(const l2d_instance_t *instance, l2d_share_view_t *out)
{
    live2d_engine_share_view_t view;
    if (!instance) {
        l2d_copy_share_view(out, NULL);
        return;
    }
    live2d_engine_share_view(instance->engine, &view);
    l2d_copy_share_view(out, &view);
}

static int l2d_surface_tight_bgra(const l2d_surface_t *surface)
{
    size_t row;
    size_t need;
    if (!surface || !surface->data || surface->width <= 0 || surface->height <= 0) {
        return 0;
    }
    if (surface->format != L2D_PIXEL_BGRA8888_LE) {
        return 0;
    }
    if (surface->stride_bytes != surface->width * 4) {
        return 0;
    }
    row = (size_t)surface->stride_bytes;
    if (row == 0 || (size_t)surface->height > (size_t)-1 / row) {
        return 0;
    }
    need = row * (size_t)surface->height;
    return surface->buffer_size_bytes >= need;
}

l2d_status_t l2d_instance_create(l2d_model_t *model, const l2d_instance_config_t *config,
                                 l2d_instance_t **out)
{
    l2d_instance_t *instance;
    l2d_status_t status;
    const l2d_allocator_t *given = (config && config->allocator) ? config->allocator : NULL;
    const l2d_allocator_t *allocator;
    if (!out) {
        return L2D_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!model) {
        return L2D_ERR_INVALID_ARG;
    }
    allocator = given ? given : &model->allocator;
    instance = (l2d_instance_t *)l2d_heap_alloc(allocator, L2D_MEM_INSTANCE, sizeof(void *),
                                                sizeof(*instance));
    if (!instance) {
        return L2D_ERR_NO_MEM;
    }
    memset(instance, 0, sizeof(*instance));
    status = live2d_engine_clone_shared(model->engine, allocator, &instance->engine);
    if (status != L2D_OK) {
        l2d_heap_free(allocator, L2D_MEM_INSTANCE, instance);
        return status;
    }
    instance->model = model;
    *out = instance;
    return L2D_OK;
}

void l2d_instance_destroy(l2d_instance_t *instance)
{
    l2d_allocator_t allocator;
    if (!instance) {
        return;
    }
    allocator = *live2d_engine_allocator(instance->engine);
    live2d_engine_destroy(instance->engine);
    l2d_heap_free(&allocator, L2D_MEM_INSTANCE, instance);
}

void l2d_instance_memory_requirements(const l2d_instance_t *instance, l2d_memory_requirements_t *out)
{
    live2d_engine_info_t info;
    size_t used = 0;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!instance || !instance->engine) {
        return;
    }
    live2d_engine_get_info(instance->engine, &info);
    if (info.pool_size >= info.pool_free) {
        used = info.pool_size - info.pool_free;
    }
    l2d_fill_used_bound(&out->per_instance_mutable, used, info.pool_size);
}

bool l2d_instance_is_loaded(const l2d_instance_t *instance)
{
    return instance && live2d_engine_is_loaded(instance->engine);
}

bool l2d_instance_play_animation(l2d_instance_t *instance, int animation_index)
{
    return instance && live2d_engine_play_index(instance->engine, animation_index);
}

void l2d_instance_pause_animation(l2d_instance_t *instance)
{
    if (instance) live2d_engine_pause_animation(instance->engine);
}

bool l2d_instance_resume_animation(l2d_instance_t *instance)
{
    return instance && live2d_engine_resume_animation(instance->engine);
}

void l2d_instance_stop_animation(l2d_instance_t *instance)
{
    if (instance) live2d_engine_stop_animation(instance->engine);
}

l2d_playback_mode_t l2d_instance_playback_mode(const l2d_instance_t *instance)
{
    return instance ? (l2d_playback_mode_t)live2d_engine_playback_mode(instance->engine)
                    : L2D_PLAYBACK_NEUTRAL;
}

int l2d_instance_current_animation(const l2d_instance_t *instance)
{
    return instance ? live2d_engine_get_current_animation_index(instance->engine) : -1;
}

void l2d_instance_info(const l2d_instance_t *instance, l2d_instance_info_t *out)
{
    live2d_engine_info_t info;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!instance) {
        return;
    }
    live2d_engine_get_info(instance->engine, &info);
    memcpy(out->id, info.id, sizeof(out->id));
    out->width = info.width;
    out->height = info.height;
    out->layer_count = info.layer_count;
    out->texture_count = info.texture_count;
    out->animation_count = info.animation_count;
    out->axis_count = live2d_engine_get_realtime_axis_count(instance->engine);
    out->pool_bytes = info.pool_size;
    out->pool_free = info.pool_free;
}

bool l2d_instance_enter_rt30(l2d_instance_t *instance)
{
    return instance && live2d_engine_enter_realtime(instance->engine);
}

void l2d_instance_reset_rt30(l2d_instance_t *instance)
{
    if (instance) {
        live2d_engine_reset_realtime(instance->engine);
    }
}

int l2d_instance_find_axis(l2d_instance_t *instance, const char *id)
{
    if (!instance) {
        return -1;
    }
    return live2d_engine_find_realtime_axis(instance->engine, id);
}

bool l2d_instance_set_axis_position(l2d_instance_t *instance, int handle, float position,
                                    uint16_t weight_q15)
{
    return instance &&
           live2d_engine_set_axis_position(instance->engine, handle, position, weight_q15);
}

bool l2d_instance_set_axis_sample(l2d_instance_t *instance, int handle, uint8_t sample,
                                  uint16_t weight_q15)
{
    return instance && live2d_engine_set_axis_sample(instance->engine, handle, sample, weight_q15);
}

bool l2d_instance_set_axes_batch(l2d_instance_t *instance, const l2d_axis_state_t *states,
                                 size_t count)
{
    live2d_axis_state_t tmp[64];
    size_t i;
    if (!instance || (count > 0 && !states) || count > 64) {
        return false;
    }
    for (i = 0; i < count; ++i) {
        tmp[i].handle = states[i].handle;
        tmp[i].sample = states[i].sample;
        tmp[i].weight_q15 = states[i].weight_q15;
    }
    return live2d_engine_set_axes_batch(instance->engine, tmp, count);
}

void l2d_instance_set_render_scale(l2d_instance_t *instance, float render_scale)
{
    if (instance) {
        live2d_engine_set_render_scale(instance->engine, render_scale);
    }
}

int l2d_instance_axis_count(const l2d_instance_t *instance)
{
    return instance ? live2d_engine_get_realtime_axis_count(instance->engine) : 0;
}

void l2d_instance_rt30_stats(l2d_instance_t *instance, l2d_rt30_stats_t *out)
{
    live2d_realtime_stats_t stats;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!instance) {
        return;
    }
    live2d_engine_get_realtime_stats(instance->engine, &stats);
    out->axis_count = stats.axis_count;
    out->static_bytes = stats.static_bytes;
    out->runtime_bytes = stats.runtime_bytes;
    out->selected_sample_bytes = stats.selected_sample_bytes;
}

void l2d_instance_mesh_counts(const l2d_instance_t *instance, l2d_mesh_counts_t *out)
{
    live2d_engine_geometry_t geometry;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!instance) {
        return;
    }
    live2d_engine_get_geometry(instance->engine, &geometry);
    out->vertices = geometry.vertices;
    out->triangles = geometry.triangles;
    out->texture_pixels = geometry.texture_pixels;
}

live2d_engine_t *l2d_instance_internal_engine(l2d_instance_t *instance)
{
    return instance ? instance->engine : NULL;
}

void l2d_instance_update(l2d_instance_t *instance, uint32_t elapsed_ms)
{
    if (instance) {
        live2d_engine_update(instance->engine, elapsed_ms);
    }
}

l2d_status_t l2d_instance_render_current(l2d_instance_t *instance, const l2d_surface_t *surface,
                                         int x, int y)
{
    if (!instance || !l2d_instance_is_loaded(instance)) {
        return L2D_ERR_INVALID_ARG;
    }
    if (!surface || surface->format != L2D_PIXEL_BGRA8888_LE) {
        return L2D_ERR_UNSUPPORTED;
    }
    if (surface->stride_bytes != surface->width * 4) {
        return L2D_ERR_UNSUPPORTED;
    }
    if (!l2d_surface_tight_bgra(surface)) {
        return L2D_ERR_RANGE;
    }
    live2d_engine_render_current(instance->engine, surface->data, surface->width,
                                 surface->height, x, y);
    return L2D_OK;
}

l2d_status_t l2d_pipeline_frame(l2d_instance_t *instance, const l2d_surface_t *surface, int x,
                                int y, uint32_t elapsed_ms)
{
    if (!instance || !l2d_instance_is_loaded(instance)) {
        return L2D_ERR_INVALID_ARG;
    }
    if (!surface || surface->format != L2D_PIXEL_BGRA8888_LE) {
        return L2D_ERR_UNSUPPORTED;
    }
    if (surface->stride_bytes != surface->width * 4) {
        return L2D_ERR_UNSUPPORTED;
    }
    if (!l2d_surface_tight_bgra(surface)) {
        return L2D_ERR_RANGE;
    }
    live2d_engine_render(instance->engine, surface->data, surface->width, surface->height, x, y,
                         elapsed_ms);
    return L2D_OK;
}

void l2d_instance_geometry_bounds(const l2d_instance_t *instance, l2d_geometry_bounds_t *out)
{
    if (!out) {
        return;
    }
    live2d_engine_get_geometry_bounds(instance ? instance->engine : NULL, out);
}

void l2d_instance_frame_profile(const l2d_instance_t *instance, l2d_frame_profile_t *out)
{
    live2d_engine_frame_profile_t profile;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!instance) {
        return;
    }
    live2d_engine_get_frame_profile(instance->engine, &profile);
    out->pose_us = profile.poseUs;
    out->physical_us = profile.physicalUs;
    out->keypoint_us = profile.keypointUs;
    out->visual_transform_us = profile.visualTransformUs;
    out->stretch_us = profile.stretchUs;
    out->vertex_transform_us = profile.vertexTransformUs;
    out->uv_update_us = profile.uvUpdateUs;
    out->sort_us = profile.sortUs;
    out->draw_us = profile.drawUs;
}

void l2d_instance_trig_cache(const l2d_instance_t *instance, uint32_t *hit, uint32_t *miss)
{
    if (!instance) {
        if (hit) {
            *hit = 0;
        }
        if (miss) {
            *miss = 0;
        }
        return;
    }
    live2d_engine_get_trig_cache(instance->engine, hit, miss);
}

uint32_t l2d_instance_roi_revision(const l2d_instance_t *instance)
{
    return instance ? live2d_engine_get_roi_revision(instance->engine) : 0;
}
