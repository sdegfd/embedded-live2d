/**
 * Diagnostic-only entry points. Present when the matching L2D_CFG flag is set
 * on both this header's includer and the runtime library.
 */
#ifndef LIVE2D_ENGINE_DIAG_H
#define LIVE2D_ENGINE_DIAG_H

#include "live2d_engine.h"

#if L2D_CFG_PROFILE_VISUAL
#include "PX_LiveFramework.h"

#ifdef __cplusplus
extern "C" {
#endif

void live2d_engine_visual_diag_copy(live2d_engine_t *engine, PX_LiveVisualDiagFrame *out);
int live2d_engine_visual_diag_topology(live2d_engine_t *engine, PX_LiveVisualDiagLayer *out,
                                       int capacity);

#ifdef __cplusplus
}
#endif
#endif

#endif
