/**
 * Conversion ROI. This is the previous/current geometry union used to shrink
 * a later color conversion. It is not dirty rasterization and it does not
 * change the clear or the software raster.
 *
 * Rectangles are half-open. The geometry expansion matches the verified
 * ESP32-P4 path: left/top = floor(min) - 2, right/bottom exclusive = ceil(max) + 3.
 *
 * History belongs to one output target. Commit it only after that target's
 * conversion has produced valid pixels. Invalidate it if conversion fails
 * without a successful fallback, or when the target/model/view changes.
 */
#ifndef L2D_ROI_H
#define L2D_ROI_H

#include "l2d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l2d_output l2d_output_t;

typedef struct {
    int origin_x;
    int origin_y;
    int canvas_w;
    int canvas_h;
    const void *target;
    int backend_tag;
    int conversion_enabled;
    uint32_t content_revision;
} l2d_output_view_t;

l2d_status_t l2d_output_create(l2d_output_t **out);
void l2d_output_destroy(l2d_output_t *output);
void l2d_output_invalidate(l2d_output_t *output);

l2d_roi_rect_t l2d_roi_from_geometry(const l2d_geometry_bounds_t *bounds, int canvas_w,
                                    int canvas_h);
l2d_roi_rect_t l2d_roi_union(l2d_roi_rect_t a, l2d_roi_rect_t b);

/**
 * Plan the conversion rectangle. Does not commit history.
 * force_full is set for the first use, a view discontinuity, an unsafe bound,
 * or an empty union that still needs a defined target.
 */
l2d_status_t l2d_output_plan(const l2d_output_t *output, const l2d_output_view_t *view,
                             const l2d_geometry_bounds_t *bounds, l2d_roi_rect_t *current,
                             l2d_roi_rect_t *conversion, int *force_full);

/** Record current geometry after the output target was successfully written. */
void l2d_output_commit(l2d_output_t *output, const l2d_output_view_t *view,
                       l2d_roi_rect_t current);

#ifdef __cplusplus
}
#endif

#endif
