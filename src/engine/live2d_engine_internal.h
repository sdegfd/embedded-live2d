/**
 * Private bridge from a public instance to the playback engine.
 * Firmware application code does not include this header.
 */
#ifndef LIVE2D_ENGINE_INTERNAL_H
#define LIVE2D_ENGINE_INTERNAL_H

#include "l2d/l2d_instance.h"
#include "live2d_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

live2d_engine_t *l2d_instance_internal_engine(l2d_instance_t *instance);

#ifdef __cplusplus
}
#endif

#endif
