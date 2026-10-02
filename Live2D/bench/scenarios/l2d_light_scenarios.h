#ifndef L2D_LIGHT_SCENARIOS_H
#define L2D_LIGHT_SCENARIOS_H

#include "l2d/l2d.h"
#include "l2d_preset.h"
#include <stdio.h>

#define L2D_LIGHT_SHA256 "4fe6e75270f59033a5be07073878ad0210e1e88cc3759fd16a2f9a56eadc7bda"
#define L2D_LIGHT_FILE_BYTES 1248800u
#define L2D_LIGHT_AXES 13
#define L2D_LIGHT_BASE_CRC_POSES (1 + L2D_LIGHT_AXES * 3 + 4)
#define L2D_LIGHT_CRC_POSES (L2D_LIGHT_BASE_CRC_POSES + 12)
#define L2D_LIGHT_CANVAS_W 480
#define L2D_LIGHT_CANVAS_H 600
#define L2D_LIGHT_SCALE 0.9375f

static const char *const l2d_light_axis_ids[L2D_LIGHT_AXES] = {
    "left_eye", "right_eye", "mouth", "head_yaw", "head_raw", "head_pitch",
    "left_arm", "right_arm", "left_leg", "right_leg", "body_sway",
    "front_hair_sway", "back_hair_sway"
};

enum { L2D_LIGHT_STATIC, L2D_LIGHT_HEAD_YAW, L2D_LIGHT_ALL_AXES,
       L2D_LIGHT_TALK_LOOK, L2D_LIGHT_SWAY_WAVE, L2D_LIGHT_STEP };

/* Deterministic smooth periodic wave. A cubic smoothstep rounds both ends;
 * its time comes from the caller, so action speed is independent of FPS. */
static inline float l2d_light_wave(unsigned ms, unsigned period, unsigned phase)
{
    unsigned p = (ms % period + phase) % period, half = period / 2u;
    float u = p < half ? (float)p / half : (float)(period-p) / (period-half);
    return 2.f * (u*u*(3.f-2.f*u)) - 1.f;
}

static inline float l2d_light_blink(unsigned ms)
{
    unsigned p = ms % 3700u;
    if (p >= 180u) return 0.f;
    float u = p < 90u ? (float)p/90.f : (float)(180u-p)/90.f;
    return 15.f * u*u*(3.f-2.f*u);
}

/* Coordinated demonstrations use the actual axis semantics. head_raw is the
 * file's roll/tilt axis. This is visual talk motion without an audio source;
 * STEP is an in-place 2D stepping gesture, not a locomotion controller. */
static inline void l2d_light_semantic_samples(int scene, unsigned ms, float values[13])
{
    values[0] = values[1] = l2d_light_blink(ms);
    values[2] = 0.f;
    for (int i = 3; i < 13; ++i) values[i] = 15.f;
    if (scene == L2D_LIGHT_TALK_LOOK) {
        values[2] = 6.f + 5.f*l2d_light_wave(ms, 420, 105);
        values[3] = 15.f + 9.f*l2d_light_wave(ms, 6000, 1500);
        values[4] = 15.f + 3.f*l2d_light_wave(ms, 6000, 1200);
        values[5] = 15.f + 4.f*l2d_light_wave(ms, 3000, 750);
        values[6] = 15.f + 2.f*l2d_light_wave(ms, 8000, 2000);
        values[7] = 15.f - 2.f*l2d_light_wave(ms, 8000, 2000);
        values[8] = 15.f + l2d_light_wave(ms, 8000, 2000);
        values[9] = 15.f - l2d_light_wave(ms, 8000, 2000);
        values[10] = 15.f + 2.5f*l2d_light_wave(ms, 8000, 2000);
        values[11] = 15.f + 4.f*l2d_light_wave(ms, 6000, 1000);
        values[12] = 15.f + 5.f*l2d_light_wave(ms, 6000, 800);
    } else if (scene == L2D_LIGHT_SWAY_WAVE) {
        values[2] = 2.f + l2d_light_wave(ms, 3500, 875);
        values[3] = 15.f + 4.f*l2d_light_wave(ms, 3500, 700);
        values[4] = 15.f + 5.f*l2d_light_wave(ms, 3500, 650);
        values[5] = 15.f + 3.f*l2d_light_wave(ms, 3500, 500);
        values[6] = 15.f + 4.f*l2d_light_wave(ms, 3500, 875);
        values[7] = 20.f + 6.f*l2d_light_wave(ms, 1800, 450);
        values[8] = 15.f + 2.f*l2d_light_wave(ms, 3500, 875);
        values[9] = 15.f - 2.f*l2d_light_wave(ms, 3500, 875);
        values[10] = 15.f + 6.f*l2d_light_wave(ms, 3500, 875);
        values[11] = 15.f + 7.f*l2d_light_wave(ms, 3500, 450);
        values[12] = 15.f + 7.f*l2d_light_wave(ms, 3500, 300);
    } else if (scene == L2D_LIGHT_STEP) {
        float step = l2d_light_wave(ms, 1400, 350);
        values[2] = 2.f + 1.5f*step;
        values[3] = 15.f + 3.f*step;
        values[4] = 15.f + 2.f*step;
        values[5] = 15.f + 2.f*l2d_light_wave(ms, 700, 175);
        values[6] = 15.f - 7.f*step;
        values[7] = 15.f + 7.f*step;
        values[8] = 15.f + 9.f*step;
        values[9] = 15.f - 9.f*step;
        values[10] = 15.f + 3.f*l2d_light_wave(ms, 700, 175);
        values[11] = 15.f + 4.f*l2d_light_wave(ms, 1400, 200);
        values[12] = 15.f + 4.f*l2d_light_wave(ms, 1400, 100);
    }
}

static inline int l2d_light_semantic_drive(l2d_instance_t *instance, const int *handles,
                                          int scene, unsigned ms)
{
    float values[13];
    l2d_light_semantic_samples(scene, ms, values);
    for (int i = 0; i < L2D_LIGHT_AXES; ++i)
        if (!l2d_instance_set_axis_position(instance, handles[i], values[i], 32767)) return 0;
    return 1;
}

static inline int l2d_light_find_axes(l2d_instance_t *instance, int *handles)
{
    if (l2d_instance_axis_count(instance) != L2D_LIGHT_AXES) return 0;
    for (int i = 0; i < L2D_LIGHT_AXES; ++i) {
        handles[i] = l2d_instance_find_axis(instance, l2d_light_axis_ids[i]);
        if (handles[i] < 0) return 0;
    }
    return 1;
}

/* The sample advances once per frame; all-axis motion deliberately exercises
 * every binding, including arms, legs, body and hair. */
static inline int l2d_light_drive(l2d_instance_t *instance, const int *handles,
                                 int scene, unsigned frame)
{
    if (scene == L2D_LIGHT_STATIC) return 1;
    if (scene == L2D_LIGHT_HEAD_YAW)
        return l2d_instance_set_axis_sample(instance, handles[3],
            l2d_preset_triangle_sample(frame, 0, 0, 29), 32767);
    l2d_axis_state_t states[L2D_LIGHT_AXES];
    for (int i = 0; i < L2D_LIGHT_AXES; ++i) {
        states[i].handle = handles[i];
        states[i].sample = l2d_preset_triangle_sample(frame, (unsigned)i * 7u, 0, 29);
        states[i].weight_q15 = 32767;
    }
    return l2d_instance_set_axes_batch(instance, states, L2D_LIGHT_AXES);
}

/* Shared ordered correctness sequence: rest, each axis's endpoints/middle,
 * four stress poses and twelve semantic poses. Run outside the timed pass. */
static inline int l2d_light_correctness_pose(l2d_instance_t *instance,
                                            const int *handles, int index,
                                            char *name, size_t name_bytes)
{
    l2d_instance_reset_rt30(instance);
    if (index == 0) {
        snprintf(name, name_bytes, "STATIC");
        return 1;
    }
    if (index >= L2D_LIGHT_BASE_CRC_POSES) {
        static const unsigned times[] = {0, 800, 1600, 3200};
        static const char *const scenes[] = {"TALK_LOOK", "SWAY_WAVE", "STEP"};
        int offset = index - L2D_LIGHT_BASE_CRC_POSES;
        int scene = offset / 4;
        unsigned ms = times[offset % 4];
        snprintf(name, name_bytes, "%s_%u", scenes[scene], ms);
        return l2d_light_semantic_drive(instance, handles, L2D_LIGHT_TALK_LOOK+scene, ms);
    }
    if (index <= L2D_LIGHT_AXES * 3) {
        static const unsigned samples[] = {0, 15, 29};
        int axis = (index - 1) / 3;
        unsigned sample = samples[(index - 1) % 3];
        snprintf(name, name_bytes, "%s_%u", l2d_light_axis_ids[axis], sample);
        return l2d_instance_set_axis_sample(instance, handles[axis], (uint8_t)sample, 32767);
    }
    static const unsigned frames[] = {0, 14, 29, 58};
    unsigned frame = frames[index - 1 - L2D_LIGHT_AXES * 3];
    snprintf(name, name_bytes, "ALL_AXES_%u", frame);
    return l2d_light_drive(instance, handles, L2D_LIGHT_ALL_AXES, frame);
}
#endif
