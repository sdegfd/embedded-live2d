#include "light_model_example.h"
#include "l2d_light_scenarios.h"
#include "bsp/wt99p4c5_s1_board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "live2d_renderer.h"
#include "l2d_p4_pie.h"
#include "mbedtls/sha256.h"
#include "sys_display.h"
#include "sys_display_flush.h"
#include "sys_monitor.h"
#include "sys_storage.h"
#include <inttypes.h>
#include <string.h>

#ifndef CONFIG_L2D_SRM_ROI
#define CONFIG_L2D_SRM_ROI 0
#endif

#if !CONFIG_L2D_LIGHT_FROM_SD
extern const uint8_t light_start[] asm("_binary_light_live_start");
extern const uint8_t light_end[] asm("_binary_light_live_end");
#endif

#define LIGHT_PATH "models/light.live"
#define LIGHT_SD_PATH "/sdcard/light.live"
#define LIGHT_SHA L2D_LIGHT_SHA256
#define LIGHT_BYTES L2D_LIGHT_FILE_BYTES

static const char *TAG = "l2d_light";
static constexpr int FRAME_US = 1000000 / 30;
static constexpr int MAX_WINDOWS = 128;

struct light_context_t {
    l2d_model_t *model;
    l2d_instance_t *instance;
    live2d_renderer_t renderer;
    sys_display_buffer_t buffer;
    sys_display_flush_t flush;
    int handles[L2D_LIGHT_AXES];
};

struct light_stats_t {
    uint32_t frames, over_budget, errors, max_us;
    uint64_t total_us, pose_us, draw_us, clear_us, convert_us, submit_us;
};

struct light_window_t {
    light_stats_t stats;
    int64_t wall_us;
    int cpu0, cpu1;
};

#if CONFIG_L2D_RENDER_RGB565_BANDS
#define LIGHT_RENDER_STACK_BYTES (16*1024)
#define LIGHT_LVGL_STACK_BYTES (32*1024)
#else
#define LIGHT_RENDER_STACK_BYTES (32*1024)
#define LIGHT_LVGL_STACK_BYTES (64*1024)
#endif

/* Fixed storage is reused between phases; no steady-state allocation or UART
 * output. Windows are printed after each measured phase. */
static light_window_t windows[MAX_WINDOWS];

/** @brief 在阶段边界记录内存余量，避免在计时帧内遍历堆。 */
static void log_memory(const char *stage)
{
    TaskHandle_t worker=xTaskGetHandle("l2d_raster"), lvgl=xTaskGetHandle("lvgl");
    printf("L2D_LIGHT_MEMORY,%s,%u,%u,%u,%u,%u,%u,%u\n", stage,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL),
        worker ? (unsigned)uxTaskGetStackHighWaterMark(worker) : 0u,
        lvgl ? (unsigned)uxTaskGetStackHighWaterMark(lvgl) : 0u);
}

/** @brief 校验选定的模型身份，加载后才释放 SD 文件缓冲。 */
static bool load_model(light_context_t *ctx)
{
    const void *bytes = nullptr;
    size_t size = 0;
#if CONFIG_L2D_LIGHT_FROM_SD
    void *sd_bytes = nullptr;
    if (sys_storage_init() != ESP_OK ||
        sys_storage_read_file(LIGHT_SD_PATH, &sd_bytes, &size) != ESP_OK) return false;
    bytes = sd_bytes;
    const char *source = LIGHT_SD_PATH;
#else
    bytes = light_start;
    size = (size_t)(light_end - light_start);
    const char *source = "embedded_flash";
#endif
    unsigned char sha[32]; char hex[65] = {};
    bool valid = mbedtls_sha256((const unsigned char *)bytes, size, sha, 0) == 0;
    if (valid) for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", sha[i]);
    valid = valid && size == LIGHT_BYTES && strcmp(hex, LIGHT_SHA) == 0;
    int64_t begin = esp_timer_get_time();
    l2d_status_t st = valid ? l2d_model_load_memory(nullptr, bytes, size, &ctx->model) : L2D_ERR_CORRUPT;
    int64_t load_us = esp_timer_get_time() - begin;
#if CONFIG_L2D_LIGHT_FROM_SD
    sys_storage_free_file(sd_bytes);
#endif
    if (st != L2D_OK) { ESP_LOGE(TAG, "model identity/import failed: %d", (int)st); return false; }
    if (l2d_instance_create(ctx->model, nullptr, &ctx->instance) != L2D_OK) return false;
    l2d_model_info_t info;
    l2d_model_info(ctx->model, &info);
    if (info.width != 480 || info.height != 640 || info.layer_count != 14 ||
        info.texture_count != 14 || info.animation_count != 0 ||
        !l2d_instance_enter_rt30(ctx->instance) ||
        !l2d_light_find_axes(ctx->instance, ctx->handles)) return false;
    l2d_instance_set_render_scale(ctx->instance, L2D_LIGHT_SCALE);
    l2d_instance_info_t instance_info;
    l2d_instance_info(ctx->instance, &instance_info);
    printf("L2D_META_BEGIN\nesp_commit=%s\nmodel_path=" LIGHT_PATH "\nmodel_source=%s\n"
           "model_sha256=%s\nmodel_size=%u\naxis_count=13\nlayer_count=14\n"
           "vertex_count=459\ntriangle_count=534\ntexture_count=14\nanimations=0\n"
           "model_canvas=480x640\ncanvas=480x600\nrender_scale=0.9375\norigin=0,-20\n"
           "profile_mode=light_sustained\nphase_seconds=%d\nwarmup_seconds=2\n"
           "scene_count=7\ncorrectness_poses=%d\n"
           "cpu_mhz=%d\npsram_mhz=%d\nl2_cache_bytes=%d\nl2_cache_line=%d\n"
           "srm_roi=%d\nmodel_load_us=%" PRId64 "\nmodel_reserved=%u\nmodel_used=%u\n"
           "instance_reserved=%u\ninstance_used=%u\nraster_workers=%d\nL2D_META_END\n",
           L2D_ESP_COMMIT, source, hex, (unsigned)size, CONFIG_L2D_LIGHT_PHASE_SECONDS,
           L2D_LIGHT_CRC_POSES,
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, CONFIG_SPIRAM_SPEED, CONFIG_CACHE_L2_CACHE_SIZE,
           CONFIG_CACHE_L2_CACHE_LINE_SIZE, CONFIG_L2D_SRM_ROI, load_us,
           (unsigned)info.pool_bytes, (unsigned)(info.pool_bytes-info.pool_free),
           (unsigned)instance_info.pool_bytes,
           (unsigned)(instance_info.pool_bytes-instance_info.pool_free),
#if CONFIG_L2D_RASTER_WORKERS
           2);
#else
           1);
#endif
    for (int i = 0; i < L2D_LIGHT_AXES; ++i)
        ESP_LOGI(TAG, "axis[%d] %s handle=%d", i, l2d_light_axis_ids[i], ctx->handles[i]);
    return true;
}

/** @brief 初始化面板、PPA 及独立 RGB565 提交路径。 */
static bool setup_display(light_context_t *ctx)
{
    bsp_display_cfg_t cfg = {
        .lvgl_adapter_cfg = {.task_stack_size = LIGHT_LVGL_STACK_BYTES, .task_priority = 4,
            .task_core_id = 0, .tick_period_ms = 5, .task_min_delay_ms = 1,
            .task_max_delay_ms = 500, .stack_in_psram = false},
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE, .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {.hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS}},
        .flags = {.buff_dma = true, .buff_spiram = true, .sw_rotate = true}};
    if (!sys_display_init_with_config(&cfg)) return false;
    if (sys_display_backlight_set(80) != ESP_OK ||
        sys_display_buffer_create(L2D_LIGHT_CANVAS_W, L2D_LIGHT_CANVAS_H, &ctx->buffer) != ESP_OK)
        return false;
    if (!sys_display_lock(0)) return false;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    esp_err_t st = sys_display_flush_init(&ctx->flush, screen, &ctx->buffer,
        (BSP_LCD_H_RES - L2D_LIGHT_CANVAS_W) / 2, 0);
    sys_display_unlock();
    return st == ESP_OK && live2d_renderer_init(&ctx->renderer, &ctx->buffer) == ESP_OK &&
        sys_monitor_start_cpu_only() == ESP_OK;
}

/** @brief 测试每条轴端点，逐像素比较所选后端与 CPU 转换；不计入性能。 */
static bool correctness(light_context_t *ctx)
{
    uint16_t *reference = (uint16_t *)heap_caps_malloc(ctx->buffer.frame_bytes, MALLOC_CAP_SPIRAM);
    if (!reference) return false;
    bool ok = true;
    ctx->renderer.capture_bgra=true;
    printf("L2D_LIGHT_CRC_HEADER,pose,argb_crc,rgb565_crc,render_ok,submit_ok,conversion_ok\n");
    for (int pose = 0; pose < L2D_LIGHT_CRC_POSES; ++pose) {
        char name[48];
        bool rendered = l2d_light_correctness_pose(ctx->instance, ctx->handles, pose, name, sizeof(name)) &&
            live2d_renderer_render_frame(&ctx->renderer, ctx->instance, 33) == ESP_OK;
        bool converted = rendered && l2d_convert_bgra_to_rgb565(ctx->buffer.render_argb8888,
            L2D_LIGHT_CANVAS_W * 4, ctx->buffer.render_bytes, reference,
            L2D_LIGHT_CANVAS_W * 2, ctx->buffer.frame_bytes,
            L2D_LIGHT_CANVAS_W, L2D_LIGHT_CANVAS_H) == L2D_OK &&
            memcmp(reference, ctx->buffer.frame_rgb565, ctx->buffer.frame_bytes) == 0;
        uint32_t argb = rendered ? esp_rom_crc32_le(0,
            (const uint8_t *)ctx->buffer.render_argb8888, ctx->buffer.render_bytes) : 0;
        uint32_t rgb = rendered ? esp_rom_crc32_le(0,
            (const uint8_t *)ctx->buffer.frame_rgb565, ctx->buffer.frame_bytes) : 0;
        bool submitted = rendered && sys_display_flush_submit(&ctx->flush) == ESP_OK;
        printf("L2D_LIGHT_CRC,%s,%08" PRIx32 ",%08" PRIx32 ",%d,%d,%d\n",
            name, argb, rgb, rendered, submitted, converted);
        ok = ok && rendered && submitted && converted;
        /* CPU backends must let IDLE0 run during this untimed CRC suite. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ctx->renderer.capture_bgra=false;
    heap_caps_free(reference);
    return ok;
}

/** @brief 按目标帧率等待；不限速测试也让 Idle 获得一毫秒时间。 */
static void frame_wait(int target_fps, int64_t begin)
{
    int64_t remain = target_fps ? (1000000 / target_fps - (esp_timer_get_time() - begin)) : 0;
    vTaskDelay(pdMS_TO_TICKS(remain >= 1000 ? (uint32_t)(remain / 1000) : 1u));
}

/** @brief 驱动、渲染和提交一帧，并累计实际阶段耗时。 */
static void run_frame(light_context_t *ctx, int scene, unsigned frame, int target,
                       light_stats_t *stats, unsigned motion_ms)
{
    int64_t begin = esp_timer_get_time();
    bool ok = (scene >= L2D_LIGHT_TALK_LOOK ?
        l2d_light_semantic_drive(ctx->instance, ctx->handles, scene, motion_ms) :
        l2d_light_drive(ctx->instance, ctx->handles, scene, frame)) &&
        live2d_renderer_render_frame(&ctx->renderer, ctx->instance, 33) == ESP_OK;
    int64_t submit_begin = esp_timer_get_time();
    if (ok) ok = sys_display_flush_submit(&ctx->flush) == ESP_OK;
    uint32_t submit = (uint32_t)(esp_timer_get_time() - submit_begin);
    uint32_t used = (uint32_t)(esp_timer_get_time() - begin);
    if (ok) ++stats->frames; else ++stats->errors;
    stats->total_us += used;
    stats->submit_us += submit;
    stats->clear_us += ctx->renderer.last_clear_us;
    stats->convert_us += ctx->renderer.last_convert_us;
    l2d_frame_profile_t profile;
    l2d_instance_frame_profile(ctx->instance, &profile);
    stats->pose_us += profile.pose_us;
    stats->draw_us += profile.draw_us;
    if (used > stats->max_us) stats->max_us = used;
    if (used > FRAME_US) ++stats->over_budget;
    frame_wait(target, begin);
}

/** @brief 输出一个完整采样窗口；FPS 使用成功提交帧数除以墙钟时间。 */
static void print_stats(const char *token, const char *scene, int target, int index,
                         const light_window_t *w)
{
    const light_stats_t &s = w->stats;
    unsigned n = s.frames + s.errors;
    if (!n) return;
    printf("%s,%s,%d,%d,%" PRId64 ",%" PRIu32 ",%.3f,%" PRIu32 ",%" PRIu32
           ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
           ",%" PRIu32 ",%d,%d\n", token, scene, target, index, w->wall_us, s.frames,
           s.frames * 1000000.0 / w->wall_us, s.over_budget, s.errors,
           s.total_us/n, s.pose_us/n, s.draw_us/n, s.clear_us/n, s.convert_us/n,
           s.submit_us/n, s.max_us, w->cpu0, w->cpu1);
}

/** @brief 完成带预热的持续测量，窗口日志在测量后统一打印。 */
static bool run_phase(light_context_t *ctx, const char *name, int scene, int target)
{
    ESP_LOGI(TAG, "CONTINUOUS phase=%s axes=%d target_fps=%d duration=%d+2s",
        name, scene >= L2D_LIGHT_ALL_AXES ? L2D_LIGHT_AXES :
            (scene == L2D_LIGHT_HEAD_YAW ? 1 : 0), target, CONFIG_L2D_LIGHT_PHASE_SECONDS);
    l2d_instance_reset_rt30(ctx->instance);
    unsigned frame = 0;
    int64_t warmup = esp_timer_get_time();
    light_stats_t ignored = {};
    while (esp_timer_get_time() - warmup < 2000000)
        run_frame(ctx, scene, frame++, target, &ignored,
            (unsigned)((esp_timer_get_time()-warmup)/1000));
    int64_t start = esp_timer_get_time(), window_start = start;
    light_stats_t current = {}, total = {};
    int count = 0, cpu0_sum = 0, cpu1_sum = 0, cpu_count = 0;
    while (esp_timer_get_time() - start < CONFIG_L2D_LIGHT_PHASE_SECONDS * 1000000LL) {
        run_frame(ctx, scene, frame++, target, &current,
            (unsigned)((esp_timer_get_time()-warmup)/1000));
        int64_t now = esp_timer_get_time();
        if (now - window_start < 1000000 &&
            now - start < CONFIG_L2D_LIGHT_PHASE_SECONDS * 1000000LL) continue;
        int cpu0 = sys_monitor_get_cpu_usage(0), cpu1 = sys_monitor_get_cpu_usage(1);
        if (cpu0 >= 0 && cpu1 >= 0) { cpu0_sum += cpu0; cpu1_sum += cpu1; ++cpu_count; }
        windows[count++] = {current, now - window_start, cpu0, cpu1};
        total.frames += current.frames; total.errors += current.errors;
        total.over_budget += current.over_budget; total.total_us += current.total_us;
        total.pose_us += current.pose_us; total.draw_us += current.draw_us;
        total.clear_us += current.clear_us; total.convert_us += current.convert_us;
        total.submit_us += current.submit_us;
        if (current.max_us > total.max_us) total.max_us = current.max_us;
        current = {}; window_start = now;
    }
    light_window_t summary = {total, esp_timer_get_time() - start,
        cpu_count ? (cpu0_sum + cpu_count/2)/cpu_count : -1,
        cpu_count ? (cpu1_sum + cpu_count/2)/cpu_count : -1};
    print_stats("L2D_LIGHT_SUMMARY", name, target, 0, &summary);
    for (int i = 0; i < count; ++i) print_stats("L2D_LIGHT_WINDOW", name, target, i, &windows[i]);
    log_memory(name);
    return total.errors == 0;
}

/** @brief 执行七段工作负载，完成后保持可观看的 13 轴动作循环。 */
static void light_task(void *)
{
    static light_context_t ctx = {};
#if CONFIG_L2D_CLEAR_PIE || CONFIG_L2D_CONVERT_PIE
    if (!l2d_p4_pie_selftest()) {
        ESP_LOGE(TAG, "STOP: PIE alignment/tail/color selftest failed");
        vTaskDelete(nullptr); return;
    }
    ESP_LOGI(TAG, "PIE alignment/tail/color selftest PASS");
#endif
    puts("L2D_LIGHT_MEMORY_HEADER,stage,psram_free,psram_largest,psram_min,internal_free,render_stack_hwm,worker_stack_hwm,lvgl_stack_hwm");
    log_memory("boot");
    if (!load_model(&ctx) || !setup_display(&ctx)) {
        ESP_LOGE(TAG, "setup failed"); vTaskDelete(nullptr); return;
    }
    esp_log_level_set("l2d.runtime", ESP_LOG_WARN);
    esp_log_level_set("live2d_renderer", ESP_LOG_WARN);
    esp_log_level_set("sys_display_flush", ESP_LOG_WARN);
    printf("L2D_META_BEGIN\nclear_backend=%s\nconvert_backend=%s\nrender_task=core0,priority6\n"
           "cpu_sampling=core1,idle_runtime_delta,1s\ncompiler=-O2,ffp-contract=off\n"
#if CONFIG_L2D_CLEAR_OVERLAP
           "pose_clear_overlap=1\n"
#else
           "pose_clear_overlap=0\n"
#endif
           "L2D_META_END\n",
           ctx.renderer.use_ppa_clear ? "PPA" :
#if CONFIG_L2D_CLEAR_PIE
               "PIE",
#else
               "CPU",
#endif
           ctx.renderer.bitscrambler_handle ? "BITSCRAMBLER" : ctx.renderer.use_ppa_convert ? "PPA" :
#if CONFIG_L2D_CONVERT_PIE
               "PIE");
#else
               "CPU");
#endif
#if CONFIG_L2D_RENDER_RGB565_BANDS
    puts("L2D_META_BEGIN\nraster_pipeline=BGRA_SRAM_bands_to_RGB565\n"
         "clear_backend=SRAM_IN_BAND\nconvert_backend=CPU_IN_BAND\n"
         "pose_clear_overlap=0\nbgra_capture=correctness_only\nraster_us_includes_conversion=1\nL2D_META_END");
#endif
    log_memory("ready");
    if (!correctness(&ctx)) {
        ESP_LOGE(TAG, "STOP: correctness/conversion failed"); vTaskDelete(nullptr); return;
    }
    const char *header = "scenario,target_fps,window,wall_us,frames,fps,over_budget,errors,avg_total_us,avg_pose_us,avg_raster_us,avg_clear_us,avg_convert_us,avg_submit_us,max_total_us,cpu0,cpu1";
    printf("L2D_LIGHT_WINDOW_HEADER,%s\nL2D_LIGHT_SUMMARY_HEADER,%s\n", header, header);
    bool ok = run_phase(&ctx, "ALL_13_AXES", L2D_LIGHT_ALL_AXES, 30);
    ok = run_phase(&ctx, "ALL_13_AXES_UNCAPPED", L2D_LIGHT_ALL_AXES, 0) && ok;
    ok = run_phase(&ctx, "TALK_LOOK", L2D_LIGHT_TALK_LOOK, 30) && ok;
    ok = run_phase(&ctx, "SWAY_WAVE", L2D_LIGHT_SWAY_WAVE, 30) && ok;
    ok = run_phase(&ctx, "STEP", L2D_LIGHT_STEP, 30) && ok;
    ok = run_phase(&ctx, "HEAD_YAW", L2D_LIGHT_HEAD_YAW, 30) && ok;
    ok = run_phase(&ctx, "STATIC", L2D_LIGHT_STATIC, 30) && ok;
    if (!ok) { ESP_LOGE(TAG, "STOP: render/submit failure"); vTaskDelete(nullptr); return; }
    puts("L2D_DONE");
    ESP_LOGI(TAG, "CONTINUOUS 13-axis stress/talk/sway/step cycle indefinitely, target=30 fps");
    l2d_instance_reset_rt30(ctx.instance);
    unsigned frame = 0;
    int64_t demo_begin = esp_timer_get_time();
    unsigned previous_segment = UINT32_MAX;
    while (true) {
        unsigned demo_ms = (unsigned)((esp_timer_get_time()-demo_begin)/1000);
        unsigned segment = demo_ms / 12000u;
        static const int scenes[] = {L2D_LIGHT_ALL_AXES, L2D_LIGHT_TALK_LOOK,
            L2D_LIGHT_SWAY_WAVE, L2D_LIGHT_STEP};
        static const char *const names[] = {"ALL_13_AXES", "TALK_LOOK", "SWAY_WAVE", "STEP"};
        if (segment != previous_segment) {
            l2d_instance_reset_rt30(ctx.instance);
            ESP_LOGI(TAG, "DEMO scene=%s all 13 axis parameters active", names[segment%4u]);
            previous_segment = segment;
        }
        light_stats_t ignored_stats = {};
        run_frame(&ctx, scenes[segment%4u], frame++, 30, &ignored_stats, demo_ms%12000u);
        if (ignored_stats.errors) { ESP_LOGE(TAG, "continuous render failed"); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
}

void l2d_light_example_start(void)
{
    if (xTaskCreatePinnedToCore(light_task, "l2d_light", LIGHT_RENDER_STACK_BYTES, nullptr, 6, nullptr, 0) != pdPASS)
        ESP_LOGE(TAG, "render task allocation failed");
}
