#ifndef L2D_BENCH_SCENARIOS_H
#define L2D_BENCH_SCENARIOS_H

#include "l2d/l2d.h"
#include "l2d_preset.h"

/* Scenario ids and input generation are shared by host and board runners. */
enum {
    L2D_BENCH_STATIC,
    L2D_BENCH_ALL_14_5,
    L2D_BENCH_ALL_29,
    L2D_BENCH_EYE_L,
    L2D_BENCH_EYE_R,
    L2D_BENCH_NECK,
    L2D_BENCH_FACE,
    L2D_BENCH_MOUTH,
    L2D_BENCH_MULTI,
    L2D_BENCH_COUNT
};

static inline int l2d_bench_drive(l2d_instance_t *instance, const int handles[5],
                                  int scene, int frame)
{
    int ok = 1;
    if (scene == L2D_BENCH_STATIC) return 1;
    for (int i = 0; i < 5; ++i) {
        if (handles[i] < 0) continue;
        float sample = 14.5f;
        if (scene == L2D_BENCH_ALL_29) sample = 29.0f;
        else if (scene >= L2D_BENCH_EYE_L && scene <= L2D_BENCH_MOUTH) {
            if (i != scene - L2D_BENCH_EYE_L) continue;
            sample = (float)l2d_preset_triangle_sample(
                (unsigned)((frame * 58) /
                           (L2D_PRESET_SHORT_WARMUP + L2D_PRESET_SHORT_MEASURE - 1)),
                0, 0, 29);
        } else if (scene == L2D_BENCH_MULTI) {
            sample = (float)l2d_preset_triangle_sample((unsigned)frame,
                                                        (unsigned)(i * 7), 0, 29);
        }
        ok = l2d_instance_set_axis_position(instance, handles[i], sample, 32767) && ok;
    }
    return ok;
}

#endif
