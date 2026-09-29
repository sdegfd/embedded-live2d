#include "profile_benchmark.h"
#include <inttypes.h>
#include <initializer_list>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#if CONFIG_L2D_PROFILE_VISUAL || CONFIG_L2D_PROFILE_DETAIL
#include "PX_LiveFramework.h"
#endif
#if CONFIG_L2D_PROFILE_VISUAL
#include "live2d_engine_diag.h"
#endif

#ifndef L2D_ESP_COMMIT
#define L2D_ESP_COMMIT "uncommitted-worktree"
#endif
#ifndef L2D_PC_COMMIT
#define L2D_PC_COMMIT "78fc634"
#endif
#ifndef CONFIG_L2D_PROFILE_STAGE
#define CONFIG_L2D_PROFILE_STAGE 0
#endif
#ifndef CONFIG_L2D_CLEAR_CPU
#define CONFIG_L2D_CLEAR_CPU 0
#endif
#ifndef CONFIG_L2D_CONVERT_CPU
#define CONFIG_L2D_CONVERT_CPU 0
#endif
#ifndef CONFIG_L2D_PROFILE_FINE
#define CONFIG_L2D_PROFILE_FINE 0
#endif
#ifndef CONFIG_L2D_PROFILE_VISUAL
#define CONFIG_L2D_PROFILE_VISUAL 0
#endif
#ifndef CONFIG_L2D_PROFILE_ROI_DIAG
#define CONFIG_L2D_PROFILE_ROI_DIAG 0
#endif
#ifndef CONFIG_L2D_SRM_ROI
#define CONFIG_L2D_SRM_ROI 0
#endif

static const char *TAG = "l2d_profile";
static int active_backend;
#include "l2d_preset.h"
static constexpr int WARMUP = L2D_PRESET_SHORT_WARMUP;
static constexpr int MEASURE = L2D_PRESET_SHORT_MEASURE;
static constexpr int ROUNDS = L2D_PRESET_SHORT_ROUNDS;
static constexpr int FRAME_US = L2D_PRESET_SHORT_DEADLINE_US;

#if CONFIG_L2D_PROFILE_STAGE == 2
/* Both PPA clients stay registered; switching only selects existing paths. */
static bool select_backend(live2d_renderer_t *renderer, int backend) {
    bool ppa_clear = (backend & 1) == 0;
    bool ppa_convert = (backend & 2) == 0;
    if ((ppa_clear && !renderer->ppa_fill_handle) ||
        (ppa_convert && !renderer->ppa_srm_handle)) return false;
    renderer->use_ppa_clear = ppa_clear;
    renderer->use_ppa_convert = ppa_convert;
    return true;
}
#endif

#if CONFIG_L2D_PROFILE_CORRECTNESS
static uint32_t correctness_frame_id;
static void emit_correctness(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, const char *name, bool ok) {
    buffer->frame_id = ++correctness_frame_id;
    esp_err_t render_ret = ok ? live2d_renderer_render_frame(renderer, engine, 33) : ESP_FAIL;
    esp_err_t submit_ret = render_ret == ESP_OK ? sys_display_flush_submit(flush) : ESP_FAIL;
    uint32_t argb_crc = render_ret == ESP_OK
        ? esp_rom_crc32_le(0, (const uint8_t *)buffer->render_argb8888, (uint32_t)buffer->render_bytes) : 0;
    uint32_t rgb_crc = render_ret == ESP_OK
        ? esp_rom_crc32_le(0, (const uint8_t *)buffer->frame_rgb565, (uint32_t)buffer->frame_bytes) : 0;
    printf("L2D_CORRECTNESS,%s,100,%" PRIu32 ",%08" PRIx32 ",%08" PRIx32 ",%d,%d,%d\n",
           name, buffer->frame_id, argb_crc, rgb_crc,
           render_ret == ESP_OK, submit_ret == ESP_OK, active_backend);
}
static bool set_axis(live2d_engine_t *engine, int handle, float sample) {
    return handle >= 0 && live2d_engine_set_axis_position(engine, handle, sample, 32767);
}
static void run_correctness(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t h,
    const char *model_sha256, const void *model_data, size_t model_size) {
    struct pose_t { const char *name; int axis; float sample; };
    static const pose_t poses[] = {
        {"STATIC", -2, 0}, {"ALL_14_5", -1, 14.5f}, {"ALL_29", -1, 29},
        {"EYE_L_0", 0, 0}, {"EYE_L_15", 0, 15}, {"EYE_L_29", 0, 29},
        {"EYE_R_0", 1, 0}, {"EYE_R_15", 1, 15}, {"EYE_R_29", 1, 29},
        {"NECK_1", 2, 1}, {"NECK_15", 2, 15}, {"NECK_29", 2, 29},
        {"FACE_5", 3, 5}, {"FACE_15", 3, 15}, {"FACE_25", 3, 25},
    };
    static const float fractions[] = {
        0.f, 0.25f, 0.5f, 1.f, 3.25f, 7.5f, 14.5f, 14.75f, 15.25f, 22.75f, 28.5f, 29.f
    };
    static const char *const fraction_names[] = {
        "0", "0_25", "0_5", "1", "3_25", "7_5", "14_5", "14_75", "15_25", "22_75", "28_5", "29"
    };
    static const char *const axis_names[] = {"EYE_L", "EYE_R", "NECK", "FACE"};
    const int axis_handles[] = {h.eye_l, h.eye_r, h.neck, h.face};
    const int handles[] = {h.eye_l, h.eye_r, h.neck, h.face, h.mouth};
    char name[64];
    live2d_engine_set_render_scale(engine, 1.0f);
    printf("L2D_META_BEGIN\nesp_commit=%s\nmodel_sha256=%s\nmodel_size=%u\n"
           "mode=correctness\nscale_q100=100\nsrm_roi=%d\nL2D_META_END\n",
           L2D_ESP_COMMIT, model_sha256, (unsigned)model_size, CONFIG_L2D_SRM_ROI);
    fputs("L2D_CORRECTNESS_HEADER,pose,scale_q100,frame_id,argb_crc32,rgb565_crc32,render_ok,submit_ok,backend\n", stdout);
    const int backend_count = CONFIG_L2D_PROFILE_STAGE == 2 ? 4 : 1;
    for (int backend = 0; backend < backend_count; ++backend) {
#if CONFIG_L2D_PROFILE_STAGE == 2
        if (!select_backend(renderer, backend)) {
            ESP_LOGE(TAG, "backend %d unavailable", backend);
            continue;
        }
#endif
        active_backend = backend;
        correctness_frame_id = 0;
    for (const pose_t &pose : poses) {
        live2d_engine_reset_realtime(engine);
        bool ok = true;
        if (pose.axis == -1) {
            for (int i = 0; i < 5; ++i)
                if (handles[i] >= 0)
                    ok = live2d_engine_set_axis_position(engine, handles[i], pose.sample, 32767) && ok;
        } else if (pose.axis >= 0) {
            ok = set_axis(engine, handles[pose.axis], pose.sample);
        }
        emit_correctness(engine, renderer, buffer, flush, pose.name, ok);
    }
    for (int axis = 0; axis < 4; ++axis) {
        for (size_t i = 0; i < sizeof(fractions) / sizeof(fractions[0]); ++i) {
            snprintf(name, sizeof(name), "FRAC_%s_%s", axis_names[axis], fraction_names[i]);
            live2d_engine_reset_realtime(engine);
            emit_correctness(engine, renderer, buffer, flush, name,
                             set_axis(engine, axis_handles[axis], fractions[i]));
        }
    }
    const float seq_a = 10.5f, seq_b = 22.75f, seq_c = 3.25f;
    const char *hist_names[] = {"AAA_1", "AAA_2", "AAA_3", "ABA_1", "ABA_2", "ABA_3",
                                 "ABCBA_1", "ABCBA_2", "ABCBA_3", "ABCBA_4", "ABCBA_5"};
    const float hist_samples[] = {seq_a, seq_a, seq_a, seq_a, seq_b, seq_a,
                                   seq_a, seq_b, seq_c, seq_b, seq_a};
    live2d_engine_reset_realtime(engine);
    for (size_t i = 0; i < sizeof(hist_samples) / sizeof(hist_samples[0]); ++i) {
        snprintf(name, sizeof(name), "HIST_NECK_%s", hist_names[i]);
        emit_correctness(engine, renderer, buffer, flush, name, set_axis(engine, h.neck, hist_samples[i]));
    }
    live2d_engine_reset_realtime(engine);
    emit_correctness(engine, renderer, buffer, flush, "DEP_NECK_A", set_axis(engine, h.neck, 8.5f));
    emit_correctness(engine, renderer, buffer, flush, "DEP_NECK_A_EYE", set_axis(engine, h.eye_l, 12.25f));
    emit_correctness(engine, renderer, buffer, flush, "DEP_NECK_B_EYE", set_axis(engine, h.neck, 18.f));
    emit_correctness(engine, renderer, buffer, flush, "DEP_NECK_A_EYE_AGAIN", set_axis(engine, h.neck, 8.5f));
    emit_correctness(engine, renderer, buffer, flush, "DEP_NECK_A_AGAIN", set_axis(engine, h.eye_l, 0.f));
    live2d_engine_reset_realtime(engine);
    bool pose_ok = true;
    for (int i = 0; i < 4; ++i) pose_ok = set_axis(engine, axis_handles[i], 14.5f) && pose_ok;
    emit_correctness(engine, renderer, buffer, flush, "RESET_BEFORE", pose_ok);
    live2d_engine_reset_realtime(engine);
    emit_correctness(engine, renderer, buffer, flush, "RESET_STATIC", true);
    pose_ok = true;
    for (int i = 0; i < 4; ++i) pose_ok = set_axis(engine, axis_handles[i], 14.5f) && pose_ok;
    emit_correctness(engine, renderer, buffer, flush, "RESET_AFTER", pose_ok);
    live2d_engine_reset_realtime(engine);
    emit_correctness(engine, renderer, buffer, flush, "RELOAD_BEFORE", true);
    bool reloaded = model_data && model_size &&
                    live2d_engine_load(engine, model_data, model_size) == L2D_OK &&
                    live2d_engine_enter_realtime(engine);
    if (reloaded) live2d_engine_reset_realtime(engine);
    emit_correctness(engine, renderer, buffer, flush, "RELOAD_STATIC", reloaded);
    pose_ok = reloaded;
    for (int i = 0; i < 4; ++i) pose_ok = set_axis(engine, axis_handles[i], 14.5f) && pose_ok;
    emit_correctness(engine, renderer, buffer, flush, "RELOAD_POSE", pose_ok);
    /* Consecutive frames expose stale RGB565 pixels that fixed poses cannot. */
    live2d_engine_reset_realtime(engine);
    for (int i=0;i<20;++i) {
        snprintf(name,sizeof(name),"DYN_STATIC_%03d",i);
        emit_correctness(engine,renderer,buffer,flush,name,true);
    }
    for (int axis=0;axis<2;++axis) {
        int handle=axis==0?h.neck:h.eye_l;
        live2d_engine_reset_realtime(engine);
        for (int step=0;step<=116;++step) {
            float sample=step<=58?0.5f*step:0.5f*(116-step);
            snprintf(name,sizeof(name),"DYN_%s_%03d",axis==0?"NECK":"EYE_L",step);
            emit_correctness(engine,renderer,buffer,flush,name,set_axis(engine,handle,sample));
        }
    }
    live2d_engine_reset_realtime(engine);
    for (int frame=0;frame<120;++frame) {
        bool ok=true;
        for(int axis=0;axis<5;++axis) {
            if(handles[axis]<0) continue;
            int phase=(frame+axis*7)%58;
            float sample=(float)(phase<=29?phase:58-phase);
            ok=set_axis(engine,handles[axis],sample)&&ok;
        }
        snprintf(name,sizeof(name),"DYN_MULTI_%03d",frame);
        emit_correctness(engine,renderer,buffer,flush,name,ok);
    }
    }
    puts("L2D_DONE");
    fflush(stdout);
}
#endif
/* Allocated once in PSRAM, before frame loops; never resized in the hot path. */
typedef struct {
    uint32_t current_valid, current_min_x, current_min_y, current_max_x, current_max_y;
    uint32_t current_w, current_h, current_area;
    uint32_t previous_valid, previous_min_x, previous_min_y, previous_max_x, previous_max_y;
    uint32_t union_min_x, union_min_y, union_max_x, union_max_y;
    uint32_t union_w, union_h, union_area;
    uint32_t current_area_ratio_q10000, union_area_ratio_q10000, force_full;
} roi_sample_t;
typedef struct {
    uint32_t frame_id, round, frame, scenario, scale_q100;
    uint32_t slot_wait_us, controller_us, rt30_us, hierarchy_vertex_us, bounds_us;
    uint32_t clear_us, raster_us, cache_sync_us, producer_total_us;
    uint32_t publish_consume_us, ppa_wait_us, ppa_blend_us, panel_submit_us;
    uint32_t ppa_fill_us, ppa_srm_us, ppa_total_us;
    uint32_t clear_cache_sync_us, convert_cache_sync_us, flush_cache_sync_us, cache_sync_total_us;
    uint32_t display_lock_wait_us, flush_total_us, frame_total_us;
    uint32_t dirty_x, dirty_y, dirty_w, dirty_h;
    uint32_t frame_drop, slot_miss, lock_skip, deadline_miss, backend, convert_us;
    uint32_t keypoint_us, visual_transform_us, stretch_us, vertex_transform_us, uv_update_us;
    uint32_t trig_cache_hit, trig_cache_miss;
    roi_sample_t roi;
} sample_t;
static sample_t *ring;
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
static void fill_roi_sample(roi_sample_t *s, const live2d_renderer_t *r) {
    const live2d_roi_t *c=&r->roi_current,*p=&r->roi_previous,*u=&r->roi_union;
    uint32_t full=(uint32_t)r->buffer->width*r->buffer->height;
    s->current_valid=c->valid; s->previous_valid=p->valid;
    s->current_min_x=c->x; s->current_min_y=c->y;
    s->current_max_x=c->valid?c->x+c->w-1:0;
    s->current_max_y=c->valid?c->y+c->h-1:0;
    s->current_w=c->w; s->current_h=c->h; s->current_area=c->w*c->h;
    s->previous_min_x=p->x; s->previous_min_y=p->y;
    s->previous_max_x=p->valid?p->x+p->w-1:0;
    s->previous_max_y=p->valid?p->y+p->h-1:0;
    s->union_min_x=u->x; s->union_min_y=u->y;
    s->union_max_x=u->valid?u->x+u->w-1:0;
    s->union_max_y=u->valid?u->y+u->h-1:0;
    s->union_w=u->w; s->union_h=u->h; s->union_area=u->w*u->h;
    s->current_area_ratio_q10000=full?10000*s->current_area/full:0;
    s->union_area_ratio_q10000=full?10000*s->union_area/full:0;
    s->force_full=r->roi_force_full;
}
#endif
static uint32_t next_frame_id;
enum scenario_t { STATIC, ALL_14_5, ALL_29, EYE_L, EYE_R, NECK, FACE, MOUTH, MULTI, COUNT };
static const char *const names[] = {"STATIC", "ALL_14_5", "ALL_29", "EYE_L_SWEEP",
    "EYE_R_SWEEP", "NECK_SWEEP", "FACE_SWEEP", "MOUTH_SWEEP", "MULTI_AXIS"};
static float triangle(int frame, int offset) {
    int phase = (frame + offset) % 58;
    return (float)(phase <= 29 ? phase : 58 - phase);
}
static bool drive(live2d_engine_t *engine, l2d_axis_handles_t h, scenario_t scene, int frame) {
    int handles[5] = {h.eye_l, h.eye_r, h.neck, h.face, h.mouth};
    if (scene == STATIC) return true;
    bool ok = true;
    for (int i = 0; i < 5; ++i) {
        if (handles[i] < 0) continue;
        float q = 14.5f;
        if (scene == ALL_29) q = 29.0f;
        else if (scene >= EYE_L && scene <= MOUTH) {
            if (i != (int)scene - (int)EYE_L) continue;
            q = triangle((frame * 58) / (WARMUP + MEASURE - 1), 0);
        } else if (scene == MULTI) q = triangle(frame, i * 7);
        ok = live2d_engine_set_axis_position(engine, handles[i], q, 32767) && ok;
    }
    return ok;
}
static void wait_frame(int64_t begin) {
    int64_t remain = FRAME_US - (esp_timer_get_time() - begin);
    if (remain >= 2000) vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain / 1000)));
    else if (remain <= 0) vTaskDelay(pdMS_TO_TICKS(1));
}
static void run_frame(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles,
    scenario_t scene, int round, int frame, int scale, sample_t *out) {
    int64_t begin = esp_timer_get_time();
    uint32_t id = ++next_frame_id;
    buffer->frame_id = id;
    int64_t control_begin = esp_timer_get_time();
    bool input_ok = drive(engine, handles, scene, frame);
    uint32_t controller_us = (uint32_t)(esp_timer_get_time() - control_begin);
    esp_err_t render_ret = live2d_renderer_render_frame(renderer, engine, 33);
    int64_t publish = esp_timer_get_time();
    esp_err_t submit_ret = render_ret == ESP_OK ? sys_display_flush_submit(flush) : ESP_FAIL;
    int64_t end = esp_timer_get_time();
    if (out) {
        live2d_engine_frame_profile_t p;
        live2d_engine_get_frame_profile(engine, &p);
        *out = {};
        out->frame_id = id; out->round = round; out->frame = frame;
        out->scenario = scene; out->scale_q100 = scale;
        out->backend = active_backend;
        out->controller_us = controller_us;
        out->rt30_us = p.poseUs; out->hierarchy_vertex_us = p.physicalUs;
#if CONFIG_L2D_PROFILE_FINE
        out->keypoint_us = p.keypointUs;
        out->visual_transform_us = p.visualTransformUs;
        out->stretch_us = p.stretchUs;
        out->vertex_transform_us = p.vertexTransformUs;
        out->uv_update_us = p.uvUpdateUs;
#endif
        live2d_engine_get_trig_cache(engine, &out->trig_cache_hit, &out->trig_cache_miss);
        out->bounds_us = renderer->last_bounds_us;
        out->clear_us = renderer->last_clear_us; out->raster_us = p.drawUs;
        out->convert_us = renderer->last_convert_us;
        out->clear_cache_sync_us = renderer->last_clear_sync_us;
        out->convert_cache_sync_us = renderer->last_convert_sync_us;
        out->flush_cache_sync_us = flush->last_cache_sync_us;
        out->cache_sync_total_us = out->clear_cache_sync_us + out->convert_cache_sync_us + out->flush_cache_sync_us;
        out->cache_sync_us = out->cache_sync_total_us;
        out->producer_total_us = (uint32_t)(publish - begin);
        out->publish_consume_us = flush->consume_begin_us >= publish && submit_ret == ESP_OK
            ? (uint32_t)(flush->consume_begin_us - publish) : 0;
        out->ppa_fill_us = renderer->use_ppa_clear ? renderer->last_clear_fill_us : 0;
        out->ppa_srm_us = renderer->use_ppa_convert ? renderer->last_ppa_convert_us : 0;
        out->ppa_total_us = out->ppa_fill_us + out->ppa_srm_us;
        out->ppa_wait_us = out->ppa_total_us;
        out->display_lock_wait_us = flush->last_lock_wait_us;
        out->panel_submit_us = flush->last_panel_submit_us;
        out->flush_total_us = submit_ret == ESP_OK ? (uint32_t)(end - flush->consume_begin_us) : 0;
        out->frame_total_us = (uint32_t)(end - begin);
        out->dirty_x = renderer->dirty_x; out->dirty_y = renderer->dirty_y;
        out->dirty_w = renderer->dirty_w; out->dirty_h = renderer->dirty_h;
        out->frame_drop = (!input_ok || render_ret != ESP_OK || submit_ret != ESP_OK ||
                           flush->panel_frame_id != id) ? 1 : 0;
        out->lock_skip = submit_ret == ESP_ERR_TIMEOUT ? 1 : 0;
        out->deadline_miss = end - begin > FRAME_US ? 1 : 0;
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
        fill_roi_sample(&out->roi,renderer);
#endif
        /* slot wait/miss and blend are 0 because this baseline directly submits
         * one synchronous RGB565 frame; no producer queue or PPA blend exists. */
    }
    wait_frame(begin);
}
static int compare_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}
static void summary_metric(FILE *f, uint32_t *scratch, scenario_t scene, int scale,
    int round, const char *metric, size_t offset) {
    uint64_t sum = 0;
    for (int i = 0; i < MEASURE; ++i) {
        memcpy(&scratch[i], (const uint8_t *)&ring[i] + offset, sizeof(uint32_t));
        sum += scratch[i];
    }
    qsort(scratch, MEASURE, sizeof(uint32_t), compare_u32);
    double avg = (double)sum / MEASURE;
    uint32_t p50 = scratch[(MEASURE*50+99)/100-1];
    uint32_t p95 = scratch[(MEASURE*95+99)/100-1];
    uint32_t p99 = scratch[(MEASURE*99+99)/100-1];
    fprintf(f, "L2D_SUMMARY,%s,%d,%d,%s,%d,%.2f,%" PRIu32 ",%" PRIu32 ",%" PRIu32
            ",%" PRIu32 ",%" PRIu32 ",%d\n", names[scene], scale, round, metric,
            MEASURE, avg, p50, p95, p99, scratch[0], scratch[MEASURE-1], active_backend);
    if (strcmp(metric, "producer_total_us") == 0 || strcmp(metric, "raster_us") == 0)
        ESP_LOGI(TAG, "%s scale=%d round=%d %s avg=%.0f p95=%" PRIu32
                 " p99=%" PRIu32 " max=%" PRIu32, names[scene], scale, round,
                 metric, avg, p95, p99, scratch[MEASURE-1]);
}
#define SUM(field) summary_metric(summary, scratch, scene, scale, round, #field, offsetof(sample_t, field))
static void write_row(FILE *csv, const sample_t *s) {
    const uint32_t *v = (const uint32_t *)s;
    fprintf(csv, "L2D_FRAME,%" PRIu32 ",%s", s->frame_id, names[s->scenario]);
    fprintf(csv, ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, s->scale_q100, s->round, s->frame);
    for (size_t i = 5; i < offsetof(sample_t,roi)/sizeof(uint32_t); ++i)
        fprintf(csv, ",%" PRIu32, v[i]);
    fputc('\n', csv);
}
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
static void write_roi_row(FILE *out, const sample_t *s) {
    fprintf(out,"L2D_ROI,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32 ",%" PRIu32,
            s->frame_id,names[s->scenario],s->scale_q100,s->round,s->frame);
    const uint32_t *v=(const uint32_t *)&s->roi;
    for(size_t i=0;i<sizeof(roi_sample_t)/sizeof(uint32_t);++i) fprintf(out,",%" PRIu32,v[i]);
    fputc('\n',out);
}
#endif
#if CONFIG_L2D_PROFILE_DETAIL
typedef struct {
    uint32_t triangles, scanlines, spans, span_pixels, max_span;
    uint32_t sample_in, sample_out, full_spans, partial_spans;
    uint32_t alpha_zero, alpha_opaque, alpha_mixed;
} work_sample_t;
static work_sample_t work_ring[MEASURE];
static void run_detail(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles,
    const char *model_sha256, size_t model_size) {
    FILE *out = stdout;
    PX_LiveFrameworkDetailFrame *detail = (PX_LiveFrameworkDetailFrame *)
        heap_caps_calloc(1, sizeof(PX_LiveFrameworkDetailFrame), MALLOC_CAP_SPIRAM);
    px_uchar *overdraw = NULL;
#if CONFIG_L2D_PROFILE_OVERDRAW
    overdraw = (px_uchar *)heap_caps_calloc((size_t)buffer->width * buffer->height,
                                            1, MALLOC_CAP_SPIRAM);
#endif
    if (!detail) {
        ESP_LOGE(TAG, "detail allocation failed");
        free(detail); free(overdraw); return;
    }
    fprintf(out,"L2D_META_BEGIN\nesp_commit=%s\nmodel_sha256=%s\nmodel_size=%u\n"
        "profile_mode=raster_work_diagnostic\nscale_q100=100\nwarmup=%d\nmeasure=%d\nrounds=1\n"
        "note=detail counters invalidate timing benchmarks\nL2D_META_END\n",
        L2D_ESP_COMMIT,model_sha256,(unsigned)model_size,WARMUP,MEASURE);
    fputs("L2D_DETAIL_HEADER,scenario,scale_q100,layer,triangles,fragments,sampler_calls,"
          "four_tap_alpha_zero,alpha_zero,alpha_opaque,alpha_mixed,"
          "covered_pixels,avg_overdraw,max_overdraw\n", out);
    fputs("L2D_WORK_HEADER,scenario,scale_q100,frame,triangle_count,scanline_count,"
          "span_count,total_span_pixels,avg_span_length_q100,max_span_length,"
          "sample_in_bounds,sample_out_of_bounds,fully_inbounds_span_count,"
          "partial_or_oob_span_count,alpha_zero_pixels,fully_opaque_pixels,alpha_mixed_pixels\n",out);
    for (int scale : {100}) {
        live2d_engine_set_render_scale(engine, scale / 100.0f);
        for (int scene_num = 0; scene_num < COUNT; ++scene_num) {
            scenario_t scene = (scenario_t)scene_num;
            if (scene != STATIC && scene != EYE_L && scene != NECK && scene != FACE && scene != MULTI) continue;
            live2d_engine_reset_realtime(engine);
            for (int i = 0; i < WARMUP; ++i)
                run_frame(engine, renderer, buffer, flush, handles, scene, 0, i, scale, NULL);
            for (int frame=0;frame<MEASURE;++frame) {
                PX_LiveFrameworkDetailBeginFrame(overdraw, buffer->width, buffer->height);
                run_frame(engine, renderer, buffer, flush, handles, scene, 0, frame+WARMUP, scale, NULL);
                PX_LiveFrameworkDetailGetFrame(detail);
                work_sample_t *w=&work_ring[frame];
                *w={};
                for (uint32_t layer=0;layer<detail->layerCount;++layer) {
                    const PX_LiveFrameworkLayerWork *l=&detail->layers[layer];
                    w->triangles+=l->triangles;
                    w->alpha_zero+=l->alphaZero;
                    w->alpha_opaque+=l->alphaOpaque;
                    w->alpha_mixed+=l->alphaMixed;
                }
                w->scanlines=detail->scanlines; w->spans=detail->spans;
                w->span_pixels=detail->spanPixels; w->max_span=detail->maxSpanLength;
                w->sample_in=detail->sampleInBounds; w->sample_out=detail->sampleOutOfBounds;
                w->full_spans=detail->fullyInBoundsSpans;
                w->partial_spans=detail->partialOrOobSpans;
            }
            for (int frame=0;frame<MEASURE;++frame) {
                const work_sample_t *w=&work_ring[frame];
                fprintf(out,"L2D_WORK,%s,%d,%d,%" PRIu32 ",%" PRIu32 ",%" PRIu32
                    ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
                    ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n",
                    names[scene],scale,frame+WARMUP,w->triangles,w->scanlines,w->spans,
                    w->span_pixels,w->spans?100*w->span_pixels/w->spans:0,w->max_span,
                    w->sample_in,w->sample_out,w->full_spans,w->partial_spans,
                    w->alpha_zero,w->alpha_opaque,w->alpha_mixed);
            }
            double avg_overdraw = detail->coveredPixels
                ? (double)detail->fragmentOverdrawSum / detail->coveredPixels : 0;
            for (uint32_t layer = 0; layer < detail->layerCount; ++layer) {
                const PX_LiveFrameworkLayerWork *w = &detail->layers[layer];
                fprintf(out, "L2D_DETAIL,%s,%d,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
                    ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
                    ",%" PRIu32 ",%.3f,%" PRIu32 "\n",
                    names[scene], scale, layer, (uint32_t)w->triangles,
                    (uint32_t)w->fragments, (uint32_t)w->samplerCalls,
                    (uint32_t)w->fourTapAlphaZero, (uint32_t)w->alphaZero,
                    (uint32_t)w->alphaOpaque, (uint32_t)w->alphaMixed,
                    (uint32_t)detail->coveredPixels,
                    avg_overdraw, (uint32_t)detail->maxOverdraw);
            }
            fflush(out);
            ESP_LOGI(TAG, "detail %s scale=%d layers=%" PRIu32 " covered=%" PRIu32
                     " max_overdraw=%" PRIu32, names[scene], scale,
                     (uint32_t)detail->layerCount, (uint32_t)detail->coveredPixels,
                     (uint32_t)detail->maxOverdraw);
        }
    }
    printf("L2D_DONE\n");
    fflush(out);
    free(detail); free(overdraw);
}
#endif

#if CONFIG_L2D_PROFILE_VISUAL
static void print_visual_id(const char *id) {
    for (int i = 0; id && id[i] && i < PX_LIVE_ID_MAX_LEN; ++i) {
        unsigned char c = (unsigned char)id[i];
        if (c < 32 || c == ',' || c == '"') c = '_';
        putchar(c);
    }
}
static_assert(sizeof(PX_LiveVisualDiagFrame) == 34 * sizeof(px_dword),
              "L2D_VISUAL_HEADER must match PX_LiveVisualDiagFrame");
static void print_visual_row(scenario_t scene, int frame, const PX_LiveVisualDiagFrame *d) {
    const px_dword *v = &d->physical_ran;
    const int fields = (int)(sizeof(PX_LiveVisualDiagFrame) / sizeof(px_dword));
    printf("L2D_VISUAL,%s,%d,%d", names[scene], frame < WARMUP ? 1 : 0, frame);
    for (int i = 0; i < fields; ++i) printf(",%" PRIu32, (uint32_t)v[i]);
    putchar('\n');
}
static void run_visual(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles,
    int64_t load_us, const char *model_sha256, size_t model_size) {
    PX_LiveVisualDiagLayer *topo = (PX_LiveVisualDiagLayer *)heap_caps_calloc(
        PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER, sizeof(PX_LiveVisualDiagLayer), MALLOC_CAP_SPIRAM);
    if (!topo) {
        ESP_LOGE(TAG, "visual diagnostic allocation failed");
        return;
    }
    printf("L2D_META_BEGIN\nesp_commit=%s\nlive2d_commit=%s\nmodel_sha256=%s\nmodel_size=%u\n"
           "model_load_us=%lld\nmode=visual_diagnostic\nprofile_visual=1\n"
           "scale_q100=100\nwarmup=%d\nmeasure=%d\nrounds=%d\n"
           "note=section_times_include_timer_reads_and_are_not_formal_benchmarks\n"
           "L2D_META_END\n",
           L2D_ESP_COMMIT, L2D_PC_COMMIT, model_sha256, (unsigned)model_size,
           (long long)load_us, WARMUP, MEASURE, ROUNDS);
    fputs("L2D_VISUAL_HEADER,scenario,warmup,frame,physical_ran,history_valid,layers,"
          "get_visual_calls,point_rotate_calls,sin_angle_calls,cos_angle_calls,sind_calls,"
          "sind_transform,sind_final,sind_set_rotation,sind_other,unique_rotation_angles,"
          "unique_local_rotation_angles,unique_point_rotate_angles,point_rotate_angle_samples,"
          "point_rotate_angle_overflow,ancestor_visits,unique_ancestors,depth_sum,max_depth,"
          "rotation_changed,local_rotation_changed,scale_changed,local_translation_changed,"
          "hierarchy_translation_changed,keypoint_changed,parent_visual_changed,traverse_us,"
          "local_translation_rotate_us,relative_rotate_us,final_sincos_us,diag_pose_us,"
          "diag_physical_us\n", stdout);
    fputs("L2D_VISUAL_POSE_HEADER,scenario,index,parent,depth,children,rotation_bits,"
          "local_rotation_bits,scale_bits,local_tx_bits,local_ty_bits,key_x_bits,key_y_bits,id\n",
          stdout);
    live2d_engine_set_render_scale(engine, 1.0f);
    for (int scene_num = 0; scene_num < COUNT; ++scene_num) {
        scenario_t scene = (scenario_t)scene_num;
        if (scene != STATIC && scene != EYE_L && scene != NECK && scene != FACE && scene != MULTI)
            continue;
        if (scene == MOUTH && handles.mouth < 0) continue;
        live2d_engine_reset_realtime(engine);
        for (int frame = 0; frame < WARMUP + MEASURE; ++frame) {
            PX_LiveVisualDiagFrame diag;
            run_frame(engine, renderer, buffer, flush, handles, scene, 1, frame, 100, NULL);
            live2d_engine_visual_diag_copy(engine, &diag);
            print_visual_row(scene, frame, &diag);
            if (frame == WARMUP) {
                int layers = live2d_engine_visual_diag_topology(engine, topo,
                    PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER);
                for (int i = 0; i < layers; ++i) {
                    const PX_LiveVisualDiagLayer *row = &topo[i];
                    printf("L2D_VISUAL_POSE,%s,%d,%d,%d,%d,%08" PRIx32 ",%08" PRIx32
                           ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 ",",
                           names[scene], row->index, row->parent, row->depth, row->children,
                           (uint32_t)row->rotation_bits, (uint32_t)row->local_rotation_bits,
                           (uint32_t)row->scale_bits, (uint32_t)row->local_tx_bits,
                           (uint32_t)row->local_ty_bits, (uint32_t)row->key_x_bits,
                           (uint32_t)row->key_y_bits);
                    print_visual_id(row->id);
                    putchar('\n');
                }
            }
        }
        fflush(stdout);
        ESP_LOGI(TAG, "visual diagnostic %s done", names[scene]);
    }
    free(topo);
    puts("L2D_DONE");
    fflush(stdout);
}
#endif

void l2d_run_profile_suite(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles,
    int64_t load_us, const char *model_sha256, const void *model_data, size_t model_size) {
#if CONFIG_L2D_PROFILE_CORRECTNESS
    (void)load_us;
    run_correctness(engine, renderer, buffer, flush, handles, model_sha256, model_data, model_size);
    return;
#elif CONFIG_L2D_PROFILE_VISUAL
    run_visual(engine, renderer, buffer, flush, handles, load_us, model_sha256, model_size);
    return;
#elif CONFIG_L2D_PROFILE_DETAIL
    (void)load_us;
    run_detail(engine, renderer, buffer, flush, handles, model_sha256, model_size);
    return;
#elif !CONFIG_L2D_PROFILE_TIMING
    (void)engine; (void)renderer; (void)buffer; (void)flush; (void)handles;
    (void)load_us; (void)model_sha256; (void)model_size;
    return;
#else
    ring = (sample_t *)heap_caps_calloc(MEASURE, sizeof(sample_t), MALLOC_CAP_SPIRAM);
    uint32_t *scratch = (uint32_t *)heap_caps_malloc(MEASURE*sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    if (!ring || !scratch) { ESP_LOGE(TAG, "profile ring allocation failed"); return; }
    FILE *csv = stdout;
    FILE *summary = stdout;
    FILE *metadata = stdout;
    live2d_engine_info_t info; live2d_engine_get_info(engine, &info);
    live2d_engine_geometry_t geo; live2d_engine_get_geometry(engine, &geo);
    fputs("L2D_META_BEGIN\n", metadata);
    fprintf(metadata, "esp_commit=%s\nlive2d_commit=%s\n"
        "idf=%s\nchip=ESP32-P4\ncpu_mhz=%d\npsram_mhz=%d\n"
        "optimization=-O2\nrender_task=core0,priority6\nlvgl_task=core0,priority4\n"
        "canvas=%dx%d\nslot=none,synchronous ARGB+RGB565 buffers\n"
        "display=direct panel RGB565\nppa=fill+SRM clients,blocking,blend unused\n"
        "model_sha256=%s\nmodel_size=%u\nmodel_load_us=%lld\n"
        "layers=%d\nvertices=%" PRIu32 "\ntriangles=%" PRIu32
        "\ntextures=%d\ntexture_pixels=%" PRIu32 "\naxes=%d\nsampler=fast-nearest\n"
        "profile_stage=%d\nprofile_mode=%s\nsrm_roi=%d\n"
        "default_clear=%s\ndefault_convert=%s\n"
        "backend_ids=0:PPA/PPA,1:CPU/PPA,2:PPA/CPU,3:CPU/CPU\n"
        "warmup=%d\nmeasure=%d\nrounds=%d\n",
        L2D_ESP_COMMIT, L2D_PC_COMMIT, esp_get_idf_version(),
        CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, CONFIG_SPIRAM_SPEED,
        buffer->width, buffer->height, model_sha256, (unsigned)model_size,
        (long long)load_us, info.layer_count, geo.vertices, geo.triangles,
        info.texture_count, geo.texture_pixels,
        live2d_engine_get_realtime_axis_count(engine), CONFIG_L2D_PROFILE_STAGE,
        CONFIG_L2D_PROFILE_ROI_DIAG ? "roi_diagnostic" :
            (CONFIG_L2D_PROFILE_FINE ? "fine_diagnostic" : "timing"),
        CONFIG_L2D_SRM_ROI,
        CONFIG_L2D_CLEAR_CPU ? "CPU" : "PPA",
        CONFIG_L2D_CONVERT_CPU ? "CPU" : "PPA",
        WARMUP, MEASURE, ROUNDS);
    fputs("L2D_META_END\n", metadata);
    fputs("L2D_FRAME_HEADER,frame_id,scenario,scale_q100,round,frame,slot_wait_us,controller_us,rt30_us,"
          "hierarchy_vertex_us,bounds_us,clear_us,raster_us,cache_sync_us,producer_total_us,"
          "publish_consume_us,ppa_wait_us,ppa_blend_us,panel_submit_us,"
          "ppa_fill_us,ppa_srm_us,ppa_total_us,clear_cache_sync_us,convert_cache_sync_us,"
          "flush_cache_sync_us,cache_sync_total_us,display_lock_wait_us,flush_total_us,frame_total_us,"
          "dirty_x,dirty_y,dirty_w,dirty_h,frame_drop,slot_miss,lock_skip,deadline_miss,backend,convert_us,"
          "keypoint_us,visual_transform_us,stretch_us,vertex_transform_us,uv_update_us,"
          "trig_cache_hit,trig_cache_miss\n", csv);
    fputs("L2D_SUMMARY_HEADER,scenario,scale_q100,round,metric,count,avg,p50,p95,p99,min,max,backend\n", summary);
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
    fputs("L2D_ROI_HEADER,frame_id,scenario,scale_q100,round,frame,current_valid,"
          "current_min_x,current_min_y,current_max_x,current_max_y,current_w,current_h,current_area,"
          "previous_valid,previous_min_x,previous_min_y,previous_max_x,previous_max_y,"
          "union_min_x,union_min_y,union_max_x,union_max_y,union_w,union_h,union_area,"
          "current_area_ratio_q10000,union_area_ratio_q10000,force_full\n",csv);
#endif
    fprintf(summary, "L2D_SUMMARY,LOAD,0,0,load_us,1,%lld,%lld,%lld,%lld,%lld,%lld,0\n",
            (long long)load_us, (long long)load_us, (long long)load_us,
            (long long)load_us, (long long)load_us, (long long)load_us);
    ESP_LOGI(TAG, "profile suite: stage=%d rounds=%d frames=%d", CONFIG_L2D_PROFILE_STAGE, ROUNDS, MEASURE);
    const int backend_count = CONFIG_L2D_PROFILE_STAGE == 2 ? 4 : 1;
    for (int backend = 0; backend < backend_count; ++backend) {
#if CONFIG_L2D_PROFILE_STAGE == 2
        if (!select_backend(renderer, backend)) {
            ESP_LOGE(TAG, "backend %d unavailable", backend);
            continue;
        }
#endif
        active_backend = backend;
    for (int scale : {100}) {
        live2d_engine_set_render_scale(engine, scale/100.0f);
        for (int scene_num = 0; scene_num < COUNT; ++scene_num) {
            scenario_t scene = (scenario_t)scene_num;
            if (scene == MOUTH && handles.mouth < 0) continue;
            if (CONFIG_L2D_PROFILE_STAGE == 0 && scene != STATIC && scene != EYE_L && scene != MULTI) continue;
            if (CONFIG_L2D_PROFILE_STAGE == 1 && scene != STATIC && scene != EYE_L && scene != NECK && scene != FACE && scene != MULTI) continue;
            if (CONFIG_L2D_PROFILE_STAGE == 2 && scene != STATIC && scene != MULTI) continue;
            for (int round = 1; round <= ROUNDS; ++round) {
                live2d_engine_reset_realtime(engine);
                for (int i = 0; i < WARMUP; ++i)
                    run_frame(engine, renderer, buffer, flush, handles, scene, round, i, scale, NULL);
                for (int i = 0; i < MEASURE; ++i) {
                    run_frame(engine, renderer, buffer, flush, handles, scene, round,
                              i+WARMUP, scale, &ring[i]);
                }
                for (int i = 0; i < MEASURE; ++i) write_row(csv, &ring[i]);
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
                for (int i = 0; i < MEASURE; ++i) write_roi_row(csv, &ring[i]);
#endif
                fflush(csv);
                SUM(slot_wait_us); SUM(controller_us); SUM(rt30_us); SUM(hierarchy_vertex_us);
                SUM(bounds_us); SUM(clear_us); SUM(raster_us); SUM(convert_us); SUM(cache_sync_us);
                SUM(producer_total_us); SUM(publish_consume_us); SUM(ppa_wait_us);
                SUM(ppa_blend_us); SUM(panel_submit_us); SUM(frame_drop);
                SUM(ppa_fill_us); SUM(ppa_srm_us); SUM(ppa_total_us);
                SUM(clear_cache_sync_us); SUM(convert_cache_sync_us); SUM(flush_cache_sync_us);
                SUM(cache_sync_total_us); SUM(display_lock_wait_us); SUM(flush_total_us); SUM(frame_total_us);
                SUM(keypoint_us); SUM(visual_transform_us); SUM(stretch_us);
                SUM(vertex_transform_us); SUM(uv_update_us);
                SUM(trig_cache_hit); SUM(trig_cache_miss);
                SUM(slot_miss); SUM(lock_skip); SUM(deadline_miss);
#if CONFIG_L2D_PROFILE_ROI_DIAG || CONFIG_L2D_SRM_ROI
                SUM(roi.current_area); SUM(roi.union_area);
                SUM(roi.current_area_ratio_q10000); SUM(roi.union_area_ratio_q10000);
                SUM(roi.current_w); SUM(roi.current_h); SUM(roi.union_w); SUM(roi.union_h);
#endif
                fflush(summary);
                ESP_LOGI(TAG, "done %s scale=%d round=%d", names[scene], scale, round);
            }
        }
    }
    }
    free(ring); free(scratch); ring = NULL;
    printf("L2D_DONE\n");
    fflush(stdout);
    ESP_LOGI(TAG, "suite complete; serial data ready for PC capture");
#endif
}
