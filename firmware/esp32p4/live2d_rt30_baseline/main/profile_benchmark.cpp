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

#ifndef L2D_ESP_COMMIT
#define L2D_ESP_COMMIT "uncommitted-worktree"
#endif
#ifndef L2D_PC_COMMIT
#define L2D_PC_COMMIT "78fc634"
#endif

static const char *TAG = "l2d_profile";
#if CONFIG_L2D_PROFILE_LONG_RUN
static constexpr int WARMUP = 100, MEASURE = 1000, ROUNDS = 3, FRAME_US = 33333;
#else
static constexpr int WARMUP = 5, MEASURE = 45, ROUNDS = 1, FRAME_US = 33333;
#endif
/* Allocated once in PSRAM, before frame loops; never resized in the hot path. */
typedef struct {
    uint32_t frame_id, round, frame, scenario, scale_q100;
    uint32_t slot_wait_us, controller_us, rt30_us, hierarchy_vertex_us, bounds_us;
    uint32_t clear_us, raster_us, cache_sync_us, producer_total_us;
    uint32_t publish_consume_us, ppa_wait_us, ppa_blend_us, panel_submit_us;
    uint32_t dirty_x, dirty_y, dirty_w, dirty_h;
    uint32_t frame_drop, slot_miss, lock_skip, deadline_miss, checksum;
} sample_t;
static sample_t *ring;
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
#if CONFIG_L2D_PROFILE_LONG_RUN
            q = triangle(frame, 0);
#else
            q = triangle((frame * 58) / (WARMUP + MEASURE - 1), 0);
#endif
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
        out->controller_us = controller_us;
        out->rt30_us = p.poseUs; out->hierarchy_vertex_us = p.physicalUs;
        out->bounds_us = renderer->last_bounds_us;
        out->clear_us = renderer->last_clear_us; out->raster_us = p.drawUs;
        out->cache_sync_us = renderer->last_clear_sync_us + flush->last_cache_sync_us;
        out->producer_total_us = (uint32_t)(publish - begin);
        out->publish_consume_us = flush->consume_begin_us >= publish && submit_ret == ESP_OK
            ? (uint32_t)(flush->consume_begin_us - publish) : 0;
        out->ppa_wait_us = renderer->last_clear_fill_us + renderer->last_ppa_convert_us;
        out->panel_submit_us = flush->last_panel_submit_us;
        out->dirty_x = renderer->dirty_x; out->dirty_y = renderer->dirty_y;
        out->dirty_w = renderer->dirty_w; out->dirty_h = renderer->dirty_h;
        out->frame_drop = (!input_ok || render_ret != ESP_OK || submit_ret != ESP_OK ||
                           flush->panel_frame_id != id) ? 1 : 0;
        out->lock_skip = submit_ret == ESP_ERR_TIMEOUT ? 1 : 0;
        out->deadline_miss = end - begin > FRAME_US ? 1 : 0;
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
            ",%" PRIu32 ",%" PRIu32 "\n", names[scene], scale, round, metric,
            MEASURE, avg, p50, p95, p99, scratch[0], scratch[MEASURE-1]);
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
    for (size_t i = 5; i < sizeof(sample_t)/sizeof(uint32_t); ++i)
        fprintf(csv, ",%" PRIu32, v[i]);
    fputc('\n', csv);
}
#if CONFIG_L2D_PROFILE_DETAIL
static void run_detail(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles) {
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
    fputs("L2D_DETAIL_HEADER,scenario,scale_q100,layer,triangles,fragments,sampler_calls,"
          "four_tap_alpha_zero,alpha_zero,alpha_opaque,alpha_mixed,"
          "covered_pixels,avg_overdraw,max_overdraw\n", out);
    for (int scale : {100, 75}) {
        live2d_engine_set_render_scale(engine, scale / 100.0f);
        for (int scene_num = 0; scene_num < COUNT; ++scene_num) {
            scenario_t scene = (scenario_t)scene_num;
            if (scene == MOUTH && handles.mouth < 0) continue;
            live2d_engine_reset_realtime(engine);
            for (int i = 0; i < 10; ++i)
                run_frame(engine, renderer, buffer, flush, handles, scene, 0, i, scale, NULL);
            PX_LiveFrameworkDetailBeginFrame(overdraw, buffer->width, buffer->height);
            run_frame(engine, renderer, buffer, flush, handles, scene, 0, 10, scale, NULL);
            PX_LiveFrameworkDetailGetFrame(detail);
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

void l2d_run_profile_suite(live2d_engine_t *engine, live2d_renderer_t *renderer,
    sys_display_buffer_t *buffer, sys_display_flush_t *flush, l2d_axis_handles_t handles,
    int64_t load_us, const char *model_sha256, size_t model_size) {
#if CONFIG_L2D_PROFILE_DETAIL
    (void)load_us; (void)model_sha256; (void)model_size;
    run_detail(engine, renderer, buffer, flush, handles);
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
        "display=direct panel RGB565\nppa=fill+SRM,blocking,blend unused\n"
        "model_sha256=%s\nmodel_size=%u\nmodel_load_us=%lld\n"
        "layers=%d\nvertices=%" PRIu32 "\ntriangles=%" PRIu32
        "\ntextures=%d\ntexture_pixels=%" PRIu32 "\naxes=%d\nsampler=fast-nearest\n"
        "warmup=%d\nmeasure=%d\nrounds=%d\n",
        L2D_ESP_COMMIT, L2D_PC_COMMIT, esp_get_idf_version(),
        CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, CONFIG_SPIRAM_SPEED,
        buffer->width, buffer->height, model_sha256, (unsigned)model_size,
        (long long)load_us, info.layer_count, geo.vertices, geo.triangles,
        info.texture_count, geo.texture_pixels,
        live2d_engine_get_realtime_axis_count(engine), WARMUP, MEASURE, ROUNDS);
    fputs("L2D_META_END\n", metadata);
    fputs("L2D_FRAME_HEADER,frame_id,scenario,scale_q100,round,frame,slot_wait_us,controller_us,rt30_us,"
          "hierarchy_vertex_us,bounds_us,clear_us,raster_us,cache_sync_us,producer_total_us,"
          "publish_consume_us,ppa_wait_us,ppa_blend_us,panel_submit_us,dirty_x,dirty_y,"
          "dirty_w,dirty_h,frame_drop,slot_miss,lock_skip,deadline_miss,checksum\n", csv);
    fputs("L2D_SUMMARY_HEADER,scenario,scale_q100,round,metric,count,avg,p50,p95,p99,min,max\n", summary);
    fprintf(summary, "L2D_SUMMARY,LOAD,0,0,load_us,1,%lld,%lld,%lld,%lld,%lld,%lld\n",
            (long long)load_us, (long long)load_us, (long long)load_us,
            (long long)load_us, (long long)load_us, (long long)load_us);
    ESP_LOGI(TAG, "profile suite: %d scenarios x 2 scales x %d rounds x %d frames",
             COUNT - (handles.mouth < 0 ? 1 : 0), ROUNDS, MEASURE);
    for (int scale : {100, 75}) {
        live2d_engine_set_render_scale(engine, scale/100.0f);
        for (int scene_num = 0; scene_num < COUNT; ++scene_num) {
            scenario_t scene = (scenario_t)scene_num;
            if (scene == MOUTH && handles.mouth < 0) continue;
            for (int round = 1; round <= ROUNDS; ++round) {
                live2d_engine_reset_realtime(engine);
                for (int i = 0; i < WARMUP; ++i)
                    run_frame(engine, renderer, buffer, flush, handles, scene, round, i, scale, NULL);
                uint32_t checksum = esp_rom_crc32_le(0, (const uint8_t *)buffer->render_argb8888,
                                                     (uint32_t)buffer->render_bytes);
                for (int i = 0; i < MEASURE; ++i) {
                    run_frame(engine, renderer, buffer, flush, handles, scene, round,
                              i+WARMUP, scale, &ring[i]);
                    ring[i].checksum = checksum;
                }
                for (int i = 0; i < MEASURE; ++i) write_row(csv, &ring[i]);
                fflush(csv);
                SUM(slot_wait_us); SUM(controller_us); SUM(rt30_us); SUM(hierarchy_vertex_us);
                SUM(bounds_us); SUM(clear_us); SUM(raster_us); SUM(cache_sync_us);
                SUM(producer_total_us); SUM(publish_consume_us); SUM(ppa_wait_us);
                SUM(ppa_blend_us); SUM(panel_submit_us); SUM(frame_drop);
                SUM(slot_miss); SUM(lock_skip); SUM(deadline_miss);
                fflush(summary);
                ESP_LOGI(TAG, "done %s scale=%d round=%d checksum=%08" PRIx32,
                         names[scene], scale, round, checksum);
            }
        }
    }
    free(ring); free(scratch); ring = NULL;
    printf("L2D_DONE\n");
    fflush(stdout);
    ESP_LOGI(TAG, "suite complete; serial data ready for PC capture");
#endif
}
