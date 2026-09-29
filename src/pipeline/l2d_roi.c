#include "l2d/l2d_roi.h"

#include "l2d_pe_port.h"

#include <math.h>
#include <string.h>

struct l2d_output {
    int history_ready;
    l2d_roi_rect_t current;
    l2d_output_view_t view;
};

static l2d_roi_rect_t l2d_roi_full(int canvas_w, int canvas_h)
{
    l2d_roi_rect_t r;
    r.x = 0;
    r.y = 0;
    r.w = canvas_w > 0 ? canvas_w : 0;
    r.h = canvas_h > 0 ? canvas_h : 0;
    r.valid = r.w > 0 && r.h > 0;
    return r;
}

l2d_roi_rect_t l2d_roi_from_geometry(const l2d_geometry_bounds_t *bounds, int canvas_w,
                                    int canvas_h)
{
    l2d_roi_rect_t r;
    float left, top, right, bottom;
    int x1, y1;
    memset(&r, 0, sizeof(r));
    if (!bounds || !bounds->valid || bounds->unsafe || canvas_w <= 0 || canvas_h <= 0) {
        return r;
    }
    /* Same expansion as the Phase 3B P4 converter: floor-2 and ceil+3. */
    left = floorf(bounds->min_x) - 2.f;
    top = floorf(bounds->min_y) - 2.f;
    right = ceilf(bounds->max_x) + 3.f;
    bottom = ceilf(bounds->max_y) + 3.f;
    if (right <= 0.f || bottom <= 0.f || left >= (float)canvas_w || top >= (float)canvas_h) {
        return r;
    }
    r.x = left < 0.f ? 0 : (int)left;
    r.y = top < 0.f ? 0 : (int)top;
    x1 = right > (float)canvas_w ? canvas_w : (int)right;
    y1 = bottom > (float)canvas_h ? canvas_h : (int)bottom;
    r.w = x1 - r.x;
    r.h = y1 - r.y;
    r.valid = r.w > 0 && r.h > 0;
    return r;
}

l2d_roi_rect_t l2d_roi_union(l2d_roi_rect_t a, l2d_roi_rect_t b)
{
    l2d_roi_rect_t r;
    int x0, y0, x1, y1;
    if (!a.valid) {
        return b;
    }
    if (!b.valid) {
        return a;
    }
    x0 = a.x < b.x ? a.x : b.x;
    y0 = a.y < b.y ? a.y : b.y;
    x1 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    y1 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    r.x = x0;
    r.y = y0;
    r.w = x1 - x0;
    r.h = y1 - y0;
    r.valid = r.w > 0 && r.h > 0;
    return r;
}

static int l2d_output_discontinuity(const l2d_output_t *output, const l2d_output_view_t *view)
{
    if (!output->history_ready) {
        return 0;
    }
    return output->view.origin_x != view->origin_x ||
           output->view.origin_y != view->origin_y ||
           output->view.canvas_w != view->canvas_w ||
           output->view.canvas_h != view->canvas_h ||
           output->view.target != view->target ||
           output->view.backend_tag != view->backend_tag ||
           output->view.conversion_enabled != view->conversion_enabled ||
           output->view.content_revision != view->content_revision;
}

l2d_status_t l2d_output_create(l2d_output_t **out)
{
    l2d_output_t *output;
    if (!out) {
        return L2D_ERR_INVALID_ARG;
    }
    output = (l2d_output_t *)l2d_port_alloc(sizeof(void *), sizeof(*output));
    if (!output) {
        return L2D_ERR_NO_MEM;
    }
    memset(output, 0, sizeof(*output));
    *out = output;
    return L2D_OK;
}

void l2d_output_destroy(l2d_output_t *output)
{
    l2d_port_free(output);
}

void l2d_output_invalidate(l2d_output_t *output)
{
    if (!output) {
        return;
    }
    output->history_ready = 0;
    memset(&output->current, 0, sizeof(output->current));
}

l2d_status_t l2d_output_plan(const l2d_output_t *output, const l2d_output_view_t *view,
                             const l2d_geometry_bounds_t *bounds, l2d_roi_rect_t *current,
                             l2d_roi_rect_t *conversion, int *force_full)
{
    l2d_roi_rect_t cur;
    l2d_roi_rect_t uni;
    int force;
    if (!output || !view || !bounds || !current || !conversion || !force_full) {
        return L2D_ERR_INVALID_ARG;
    }
    if (view->canvas_w <= 0 || view->canvas_h <= 0) {
        return L2D_ERR_INVALID_ARG;
    }
    cur = l2d_roi_from_geometry(bounds, view->canvas_w, view->canvas_h);
    force = !output->history_ready || l2d_output_discontinuity(output, view) || bounds->unsafe;
    uni = force ? l2d_roi_full(view->canvas_w, view->canvas_h)
                : l2d_roi_union(output->current, cur);
    if (!uni.valid) {
        force = 1;
        uni = l2d_roi_full(view->canvas_w, view->canvas_h);
    }
    *current = cur;
    *conversion = uni;
    *force_full = force;
    return L2D_OK;
}

void l2d_output_commit(l2d_output_t *output, const l2d_output_view_t *view,
                       l2d_roi_rect_t current)
{
    if (!output || !view) {
        return;
    }
    output->current = current;
    output->view = *view;
    output->history_ready = 1;
}
