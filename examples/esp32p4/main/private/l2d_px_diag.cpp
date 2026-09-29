#include "l2d_px_diag.h"

#include <stdlib.h>
#include <string.h>

#include "PX_LiveFramework.h"
#include "live2d_engine_diag.h"
#include "live2d_engine_internal.h"

void l2d_px_detail_begin(void *overdraw, int width, int height)
{
    PX_LiveFrameworkDetailBeginFrame((px_uchar *)overdraw, width, height);
}

void l2d_px_detail_get(l2d_px_detail_frame_t *out)
{
    PX_LiveFrameworkDetailFrame frame;
    uint32_t i;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    memset(&frame, 0, sizeof(frame));
    PX_LiveFrameworkDetailGetFrame(&frame);
    out->layer_count = frame.layerCount;
    if (out->layer_count > L2D_PX_DIAG_LAYER_MAX) {
        out->layer_count = L2D_PX_DIAG_LAYER_MAX;
    }
    for (i = 0; i < out->layer_count; ++i) {
        const PX_LiveFrameworkLayerWork *layer = &frame.layers[i];
        l2d_px_layer_work_t *dst = &out->layers[i];
        dst->triangles = layer->triangles;
        dst->fragments = layer->fragments;
        dst->sampler_calls = layer->samplerCalls;
        dst->four_tap_alpha_zero = layer->fourTapAlphaZero;
        dst->alpha_zero = layer->alphaZero;
        dst->alpha_opaque = layer->alphaOpaque;
        dst->alpha_mixed = layer->alphaMixed;
    }
    out->covered_pixels = frame.coveredPixels;
    out->max_overdraw = frame.maxOverdraw;
    out->fragment_overdraw_sum = frame.fragmentOverdrawSum;
    out->scanlines = frame.scanlines;
    out->spans = frame.spans;
    out->span_pixels = frame.spanPixels;
    out->max_span_length = frame.maxSpanLength;
    out->sample_in = frame.sampleInBounds;
    out->sample_out = frame.sampleOutOfBounds;
    out->full_spans = frame.fullyInBoundsSpans;
    out->partial_spans = frame.partialOrOobSpans;
}

void l2d_px_visual_copy(l2d_instance_t *instance, uint32_t words[L2D_PX_VISUAL_WORDS])
{
    if (!words) {
        return;
    }
    memset(words, 0, sizeof(uint32_t) * L2D_PX_VISUAL_WORDS);
#if L2D_CFG_PROFILE_VISUAL
    PX_LiveVisualDiagFrame frame;
    static_assert(sizeof(frame) == L2D_PX_VISUAL_WORDS * sizeof(px_dword),
                  "visual diagnostic word count");
    live2d_engine_visual_diag_copy(l2d_instance_internal_engine(instance), &frame);
    memcpy(words, &frame, sizeof(frame));
#else
    (void)instance;
#endif
}

int l2d_px_visual_topology(l2d_instance_t *instance, l2d_px_visual_layer_t *out, int capacity)
{
#if L2D_CFG_PROFILE_VISUAL
    PX_LiveVisualDiagLayer *topo;
    int count;
    int i;
    if (!out || capacity <= 0) {
        return 0;
    }
    topo = (PX_LiveVisualDiagLayer *)calloc((size_t)capacity, sizeof(*topo));
    if (!topo) {
        return 0;
    }
    count = live2d_engine_visual_diag_topology(l2d_instance_internal_engine(instance), topo,
                                               capacity);
    if (count > capacity) {
        count = capacity;
    }
    for (i = 0; i < count; ++i) {
        out[i].index = topo[i].index;
        out[i].parent = topo[i].parent;
        out[i].depth = topo[i].depth;
        out[i].children = topo[i].children;
        out[i].rotation_bits = (uint32_t)topo[i].rotation_bits;
        out[i].local_rotation_bits = (uint32_t)topo[i].local_rotation_bits;
        out[i].scale_bits = (uint32_t)topo[i].scale_bits;
        out[i].local_tx_bits = (uint32_t)topo[i].local_tx_bits;
        out[i].local_ty_bits = (uint32_t)topo[i].local_ty_bits;
        out[i].key_x_bits = (uint32_t)topo[i].key_x_bits;
        out[i].key_y_bits = (uint32_t)topo[i].key_y_bits;
        memcpy(out[i].id, topo[i].id, L2D_PX_DIAG_ID_MAX);
    }
    free(topo);
    return count;
#else
    (void)instance;
    (void)out;
    (void)capacity;
    return 0;
#endif
}
