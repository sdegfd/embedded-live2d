#pragma once
#include <stddef.h>
#include <stdint.h>
#include "live2d_engine.h"
#include "live2d_renderer.h"
#include "sys_display_flush.h"

typedef struct { int eye_l, eye_r, neck, face, mouth; } l2d_axis_handles_t;
void l2d_run_profile_suite(live2d_engine_t *engine, live2d_renderer_t *renderer,
                           sys_display_buffer_t *buffer, sys_display_flush_t *flush,
                           l2d_axis_handles_t handles, int64_t load_us,
                           const char *model_sha256, const void *model_data,
                           size_t model_size);
