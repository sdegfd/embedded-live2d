#include "l2d/l2d.h"

#include "live2d_engine.h"
#include "live2d_engine_internal.h"
#include "l2d_pe_port.h"

#include <string.h>

struct l2d_instance {
    live2d_engine_t *engine;
};

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

l2d_status_t l2d_instance_create(size_t pool_bytes, l2d_instance_t **out)
{
    l2d_instance_t *instance;
    l2d_status_t status;
    if (!out || pool_bytes == 0) {
        return L2D_ERR_INVALID_ARG;
    }
    instance = (l2d_instance_t *)l2d_port_alloc(sizeof(void *), sizeof(*instance));
    if (!instance) {
        return L2D_ERR_NO_MEM;
    }
    memset(instance, 0, sizeof(*instance));
    status = live2d_engine_create(pool_bytes, &instance->engine);
    if (status != L2D_OK) {
        l2d_port_free(instance);
        return status;
    }
    *out = instance;
    return L2D_OK;
}

void l2d_instance_destroy(l2d_instance_t *instance)
{
    if (!instance) {
        return;
    }
    live2d_engine_destroy(instance->engine);
    l2d_port_free(instance);
}

l2d_status_t l2d_instance_load_memory(l2d_instance_t *instance, const void *bytes, size_t size)
{
    if (!instance) {
        return L2D_ERR_INVALID_ARG;
    }
    return live2d_engine_load(instance->engine, bytes, size);
}

bool l2d_instance_is_loaded(const l2d_instance_t *instance)
{
    return instance && live2d_engine_is_loaded(instance->engine);
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
