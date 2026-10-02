/**
 * @file main.cpp
 * @brief Live2D RT30 实时多轴基线验证入口
 *
 * 加载设备 SD 上的 esp.live。正式测试只接受与工作区 models/esp.live 相同的五轴文件。
 * 轴为 left_eye、right_eye、neck、face、mouth。SHA256 或轴数不符时停止，不换模型。
 *   - RT30 尾部被正确解析并记录真实轴 ID
 *   - 实时多轴 Q15 融合产生可见姿态变化
 *   - 模式仲裁/局部变换/ESP 渲染优化路径不崩溃
 *
 * 渲染管线：l2d instance（RT30） -> live2d_renderer -> 显示缓冲区
 * 参数按帧更新，求值由 playback frame 的 REALTIME30 分支统一触发（dirty revision）。
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bsp/wt99p4c5_s1_board.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "l2d/l2d.h"
#include "live2d_renderer.h"
#include "profile_benchmark.h"
#include "l2d_preset.h"
#include "mbedtls/sha256.h"
#include "lvgl.h"
#include "sys_display.h"
#include "sys_display_buffer.h"
#include "sys_display_flush.h"
#include "sys_monitor.h"
#include "sys_storage.h"
#if CONFIG_L2D_EXAMPLE_LIGHT
#include "light_model_example.h"
#endif

static const char *TAG = "l2d_baseline";

#define MODEL_PATH        "/sdcard/esp.live"
#define L2D_BASELINE_SHA256 "1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100"
#define L2D_BASELINE_SIZE   800796u
#define TASK_STACK        (32 * 1024)
#define TASK_PRIORITY     6
#define TASK_CORE         0
#define TARGET_FPS        30
#define FRAME_US          (1000000 / TARGET_FPS)

#define STATS_US          1000000
#define WEIGHT_FULL_Q15   32767u

/* 五轴 ID，对应 SD 与工作区里的 esp.live。 */
static const char *AXIS_LEFT_EYE  = "left_eye";
static const char *AXIS_RIGHT_EYE = "right_eye";
static const char *AXIS_NECK      = "neck";
static const char *AXIS_FACE      = "face";
static const char *AXIS_MOUTH     = "mouth";


typedef struct {
    uint32_t frames;
    uint32_t over_budget_frames;
    uint32_t batch_failures;
    uint64_t axis_us;
    uint64_t render_us;
    uint64_t clear_us;
    uint64_t clear_fill_us;
    uint64_t clear_sync_us;
    uint64_t pose_us;
    uint64_t physical_us;
    uint64_t sort_us;
    uint64_t draw_us;
    uint64_t engine_us;
    uint64_t convert_us;
    uint64_t flush_us;
    uint64_t total_us;
    uint64_t budget_remain_us;
    uint64_t actual_wait_us;
    uint64_t frame_period_us;
    uint32_t max_total_us;
} baseline_stats_t;

static void log_heap(const char *stage)
{
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "heap %s: psram_free=%u psram_min=%u internal_free=%u",
             stage ? stage : "", (unsigned)psram_free, (unsigned)psram_min,
             (unsigned)internal_free);
}

static bsp_display_cfg_t make_display_config(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_adapter_cfg = {
            .task_stack_size = 64 * 1024,
            .task_priority = 4,
            .task_core_id = TASK_CORE,
            .tick_period_ms = 5,
            .task_min_delay_ms = 1,
            .task_max_delay_ms = 500,
            .stack_in_psram = false,
        },
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {
            .hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {
                .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            },
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = true,
        },
    };
    return cfg;
}

static esp_err_t init_flush(sys_display_buffer_t *buffer, sys_display_flush_t *flush,
                            int canvas_x, int canvas_y)
{
    if (!sys_display_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    esp_err_t ret = sys_display_flush_init(flush, screen, buffer, canvas_x, canvas_y);
    sys_display_unlock();
    return ret;
}

static uint32_t elapsed_ms_since(int64_t *last_us)
{
    int64_t now_us = esp_timer_get_time();
    int64_t elapsed_us = now_us - *last_us;
    *last_us = now_us;
    if (elapsed_us <= 0) return 1;
    uint32_t ms = (uint32_t)((elapsed_us + 500) / 1000);
    if (ms == 0) ms = 1;
    if (ms > 100) ms = 100;
    return ms;
}

static void delay_until_next_frame(int64_t frame_begin_us, int64_t frame_end_us)
{
    int64_t used_us = frame_end_us - frame_begin_us;
    if (used_us >= FRAME_US) {
        /* 超时也至少 yield 1ms，避免饿死 IDLE0 触发 task watchdog（IDLE0 看门狗） */
        vTaskDelay(pdMS_TO_TICKS(1));
        return;
    }
    int64_t remain_us = FRAME_US - used_us;
    if (remain_us >= 2000) {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)(remain_us / 1000)));
    }
}

static void choose_render_size(const l2d_instance_info_t *info, int *w, int *h)
{
    int rw = info ? info->width : 0;
    int rh = info ? info->height : 0;
    if (rw <= 0 || rh <= 0) { rw = BSP_LCD_H_RES; rh = BSP_LCD_V_RES; }
    if (rw > BSP_LCD_H_RES) rw = BSP_LCD_H_RES;
    if (rh > BSP_LCD_V_RES) rh = BSP_LCD_V_RES;
    *w = rw;
    *h = rh;
}

/*
 * 帧驱动的 RT30 离散三角波。这里刻意不使用 esp_timer，也不设置动作持续时间：
 * 每完成一帧就前进一个样本，到端点后反向，设备运行多久就循环多久。
 */
static uint8_t continuous_sample(uint32_t frame_index, uint32_t phase_offset,
                                 uint8_t min_sample, uint8_t max_sample)
{
    return l2d_preset_triangle_sample(frame_index, phase_offset, min_sample, max_sample);
}

/* 五轴每帧都参与同一个 batch；相位错开以覆盖不同组合姿态。 */
static void compute_axis_samples(uint32_t frame_index, uint8_t *eye, uint8_t *neck,
                                 uint8_t *face, uint8_t *mouth)
{
    *eye = continuous_sample(frame_index, 0, 0, 29);
    *neck = continuous_sample(frame_index, 9, 1, 29);
    *face = continuous_sample(frame_index, 17, 5, 25);
    *mouth = continuous_sample(frame_index, 25, 0, 29);
}

static void render_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Live2D RT30 baseline, target=%d fps, core=%d", TARGET_FPS, TASK_CORE);
    ESP_LOGI(TAG, " model=%s", MODEL_PATH);
    ESP_LOGI(TAG, "========================================");
    log_heap("boot");

    esp_err_t ret = sys_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "storage init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "storage ready at %s", sys_storage_get_mount_point());

    bsp_display_cfg_t display_cfg = make_display_config();
    lv_display_t *disp = sys_display_init_with_config(&display_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "display init failed");
        vTaskDelete(NULL);
        return;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(sys_display_backlight_set(80));
    sys_monitor_start();
    sys_monitor_attach_display(disp);

    void *model_data = NULL;
    size_t model_size = 0;
    ret = sys_storage_read_file(MODEL_PATH, &model_data, &model_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read model failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "model file loaded: %s (%u bytes)", MODEL_PATH, (unsigned)model_size);
    ESP_LOGI(TAG, "model CRC32_LE=0x%08" PRIx32,
             esp_rom_crc32_le(0, (const uint8_t *)model_data, (uint32_t)model_size));
    unsigned char model_sha_bytes[32];
    char model_sha_hex[65];
    if (mbedtls_sha256((const unsigned char *)model_data, model_size,
                       model_sha_bytes, 0) != 0) {
        ESP_LOGE(TAG, "model SHA256 failed");
        vTaskDelete(NULL);
        return;
    }
    for (int i = 0; i < 32; ++i) {
        snprintf(model_sha_hex + i * 2, 3, "%02x", model_sha_bytes[i]);
    }
    model_sha_hex[64] = '\0';
    ESP_LOGI(TAG, "model SHA256=%s", model_sha_hex);

    l2d_model_t *model = NULL;
    l2d_instance_t *instance = NULL;
    int64_t load_begin = esp_timer_get_time();
    l2d_status_t st = l2d_model_load_memory(NULL, model_data, model_size, &model);
    int64_t load_end = esp_timer_get_time();
#if !CONFIG_L2D_PROFILE_CORRECTNESS
    sys_storage_free_file(model_data);
    model_data = NULL;
#endif
    if (st != L2D_OK) {
        ESP_LOGE(TAG, "model import failed: status=%d", (int)st);
        if (model_data) sys_storage_free_file(model_data);
        vTaskDelete(NULL);
        return;
    }
    st = l2d_instance_create(model, NULL, &instance);
    if (st != L2D_OK) {
        ESP_LOGE(TAG, "instance create failed: status=%d", (int)st);
        l2d_model_destroy(model);
        if (model_data) sys_storage_free_file(model_data);
        vTaskDelete(NULL);
        return;
    }

    l2d_instance_info_t info;
    l2d_instance_info(instance, &info);
    ESP_LOGI(TAG,
             "model ready id='%.*s' size=%dx%d layers=%d textures=%d animations=%d load=%" PRId64
             "ms pool_free=%u",
             (int)sizeof(info.id), info.id, info.width, info.height, info.layer_count,
             info.texture_count, info.animation_count, (load_end - load_begin) / 1000,
             (unsigned)info.pool_free);

    /* 进入 RT30 实时模式并缓存五轴 handle（初始化时一次性字符串查找） */
    int axis_count = l2d_instance_axis_count(instance);
    l2d_rt30_stats_t rt_stats;
    l2d_instance_rt30_stats(instance, &rt_stats);
    ESP_LOGI(TAG, "RT30 axis_count=%d static=%u runtime=%u selectedSample=%u",
             axis_count, (unsigned)rt_stats.static_bytes, (unsigned)rt_stats.runtime_bytes,
             (unsigned)rt_stats.selected_sample_bytes);

    if (axis_count <= 0) {
        ESP_LOGE(TAG, "NO RT30 axes! trailer was silently ignored - falling back to static render");
    }

    if (!l2d_instance_enter_rt30(instance)) {
        ESP_LOGE(TAG, "enter_realtime failed");
        vTaskDelete(NULL);
        return;
    }
    int h_eye_l = l2d_instance_find_axis(instance, AXIS_LEFT_EYE);
    int h_eye_r = l2d_instance_find_axis(instance, AXIS_RIGHT_EYE);
    int h_neck  = l2d_instance_find_axis(instance, AXIS_NECK);
    int h_face  = l2d_instance_find_axis(instance, AXIS_FACE);
    int h_mouth = l2d_instance_find_axis(instance, AXIS_MOUTH);
    ESP_LOGI(TAG, "axis handles: left_eye=%d right_eye=%d neck=%d face=%d mouth=%d",
             h_eye_l, h_eye_r, h_neck, h_face, h_mouth);
    if (strcmp(model_sha_hex, L2D_BASELINE_SHA256) != 0 || model_size != L2D_BASELINE_SIZE ||
        axis_count != 5 || h_eye_l < 0 || h_eye_r < 0 || h_neck < 0 || h_face < 0 || h_mouth < 0) {
        ESP_LOGE(TAG,
                 "STOP formal test: %s size=%u sha=%s axes=%d mouth=%d. Expected five-axis esp.live "
                 "sha=%s size=%u. Not switching models.",
                 MODEL_PATH, (unsigned)model_size, model_sha_hex, axis_count, h_mouth,
                 L2D_BASELINE_SHA256, (unsigned)L2D_BASELINE_SIZE);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "benchmark model_path=%s axis_count=%d", MODEL_PATH, axis_count);

    int render_w = 0, render_h = 0;
    choose_render_size(&info, &render_w, &render_h);
    int canvas_x = (BSP_LCD_H_RES - render_w) / 2;
    int canvas_y = (BSP_LCD_V_RES - render_h) / 2;
    if (canvas_x < 0) canvas_x = 0;
    if (canvas_y < 0) canvas_y = 0;
    ESP_LOGI(TAG, "render ROI: %dx%d at (%d,%d), lcd=%dx%d", render_w, render_h, canvas_x,
             canvas_y, BSP_LCD_H_RES, BSP_LCD_V_RES);

    sys_display_buffer_t buffer;
    ret = sys_display_buffer_create(render_w, render_h, &buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display buffer create failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    sys_display_flush_t flush;
    ret = init_flush(&buffer, &flush, canvas_x, canvas_y);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "flush init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    l2d_instance_set_render_scale(instance, 1.0f);

    live2d_renderer_t renderer;
    ret = live2d_renderer_init(&renderer, &buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "renderer init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "render backends: clear=%s convert=%s",
             renderer.use_ppa_clear ? "PPA" : "CPU",
             renderer.use_ppa_convert ? "PPA" : "CPU");
    log_heap("ready");

    /* 渲染任务中的多行周期日志会同步占用 UART，并直接制造 80~95 ms 假慢帧。
     * 保留本入口每秒一行的分阶段汇总，屏蔽其他渲染热路径的 INFO profiling。 */
    esp_log_level_set("l2d.runtime", ESP_LOG_WARN);
    esp_log_level_set("live2d_renderer", ESP_LOG_WARN);
    esp_log_level_set("sys_display_flush", ESP_LOG_WARN);
    esp_log_level_set("sys_monitor", ESP_LOG_WARN);
    ESP_LOGI(TAG, "continuous RT30 drive active on %d model axes", axis_count);

#if CONFIG_L2D_PROFILE_TIMING || CONFIG_L2D_PROFILE_DETAIL || CONFIG_L2D_PROFILE_CORRECTNESS || CONFIG_L2D_PROFILE_VISUAL
    l2d_axis_handles_t handles = {h_eye_l, h_eye_r, h_neck, h_face, h_mouth};
    l2d_run_profile_suite(&instance, &model, &renderer, &buffer, &flush, handles,
                          load_end - load_begin, model_sha_hex, model_data, model_size);
#if CONFIG_L2D_PROFILE_CORRECTNESS
    sys_storage_free_file(model_data);
    model_data = NULL;
#endif
#endif

    baseline_stats_t stats = {};
    int64_t anim_last = esp_timer_get_time();
    int64_t stats_begin = anim_last;
    uint32_t window = 0;
    uint32_t drive_frame = 0;

    while (1) {
        int64_t frame_begin = esp_timer_get_time();
        uint32_t elapsed_ms = elapsed_ms_since(&anim_last);

        /* 按帧推进五轴，不使用动作计时器、停止条件或回 Idle 路径。 */
        int64_t axis_begin = esp_timer_get_time();
        uint8_t eye_s, neck_s, face_s, mouth_s;
        compute_axis_samples(drive_frame, &eye_s, &neck_s, &face_s, &mouth_s);
        l2d_axis_state_t states[5];
        states[0].handle = h_eye_l; states[0].sample = eye_s;  states[0].weight_q15 = WEIGHT_FULL_Q15;
        states[1].handle = h_eye_r; states[1].sample = eye_s;  states[1].weight_q15 = WEIGHT_FULL_Q15;
        states[2].handle = h_neck;  states[2].sample = neck_s; states[2].weight_q15 = WEIGHT_FULL_Q15;
        states[3].handle = h_face;  states[3].sample = face_s; states[3].weight_q15 = WEIGHT_FULL_Q15;
        states[4].handle = h_mouth; states[4].sample = mouth_s; states[4].weight_q15 = WEIGHT_FULL_Q15;
        if (!l2d_instance_set_axes_batch(instance, states, 5)) {
            stats.batch_failures++;
        }
        int64_t axis_end = esp_timer_get_time();

        ret = live2d_renderer_render_frame(&renderer, instance, elapsed_ms);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "render frame failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int64_t flush_begin = esp_timer_get_time();
        ret = sys_display_flush_submit(&flush);
        int64_t flush_end = esp_timer_get_time();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "flush submit failed: %s", esp_err_to_name(ret));
        }

        int64_t frame_end = esp_timer_get_time();
        l2d_frame_profile_t profile;
        l2d_instance_frame_profile(instance, &profile);
        stats.frames++;
        stats.axis_us += (uint32_t)(axis_end - axis_begin);
        stats.render_us += renderer.last_render_us;
        stats.clear_us += renderer.last_clear_us;
        stats.clear_fill_us += renderer.last_clear_fill_us;
        stats.clear_sync_us += renderer.last_clear_sync_us;
        stats.pose_us += profile.pose_us;
        stats.physical_us += profile.physical_us;
        stats.sort_us += profile.sort_us;
        stats.draw_us += profile.draw_us;
        stats.engine_us += renderer.last_engine_us;
        stats.convert_us += renderer.last_convert_us;
        stats.flush_us += (uint32_t)(flush_end - flush_begin);
        uint32_t total_us = (uint32_t)(frame_end - frame_begin);
        stats.total_us += total_us;
        if (total_us > stats.max_total_us) stats.max_total_us = total_us;
        if (total_us > FRAME_US) stats.over_budget_frames++;
        stats.budget_remain_us += total_us < FRAME_US ? FRAME_US - total_us : 0;
        drive_frame++;

        int64_t wait_begin = esp_timer_get_time();
        delay_until_next_frame(frame_begin, frame_end);
        int64_t now = esp_timer_get_time();
        stats.actual_wait_us += (uint32_t)(now - wait_begin);
        stats.frame_period_us += (uint32_t)(now - frame_begin);
        if (now - stats_begin >= STATS_US && stats.frames > 0) {
            float fps = ((float)stats.frames * 1000000.0f) / (float)(now - stats_begin);
            ESP_LOGI(TAG,
                     "perf window=%" PRIu32 " fps=%.2f eye=%u neck=%u face=%u mouth=%u frames=%" PRIu32
                     " over_budget=%" PRIu32 " batch_fail=%" PRIu32
                     " avg_us axis=%" PRIu32 " clear=%" PRIu32 " fill=%" PRIu32 " sync=%" PRIu32
                     " pose=%" PRIu32 " physical=%" PRIu32 " sort=%" PRIu32 " draw=%" PRIu32
                     " engine=%" PRIu32 " render=%" PRIu32 " convert=%" PRIu32 " flush=%" PRIu32
                     " total=%" PRIu32 " budget_remain=%" PRIu32
                     " actual_wait=%" PRIu32 " period=%" PRIu32
                     " max_total=%" PRIu32 " cpu0=%d cpu1=%d",
                     window, (double)fps, eye_s, neck_s, face_s, mouth_s, stats.frames,
                     stats.over_budget_frames, stats.batch_failures,
                     (uint32_t)(stats.axis_us / stats.frames),
                     (uint32_t)(stats.clear_us / stats.frames),
                     (uint32_t)(stats.clear_fill_us / stats.frames),
                     (uint32_t)(stats.clear_sync_us / stats.frames),
                     (uint32_t)(stats.pose_us / stats.frames),
                     (uint32_t)(stats.physical_us / stats.frames),
                     (uint32_t)(stats.sort_us / stats.frames),
                     (uint32_t)(stats.draw_us / stats.frames),
                     (uint32_t)(stats.engine_us / stats.frames),
                     (uint32_t)(stats.render_us / stats.frames),
                     (uint32_t)(stats.convert_us / stats.frames),
                     (uint32_t)(stats.flush_us / stats.frames),
                     (uint32_t)(stats.total_us / stats.frames),
                     (uint32_t)(stats.budget_remain_us / stats.frames),
                     (uint32_t)(stats.actual_wait_us / stats.frames),
                     (uint32_t)(stats.frame_period_us / stats.frames),
                     stats.max_total_us,
                     sys_monitor_get_cpu_usage(0), sys_monitor_get_cpu_usage(1));
            memset(&stats, 0, sizeof(stats));
            stats_begin = now;
            window++;
        }
    }
}

extern "C" void app_main(void)
{
#if CONFIG_L2D_EXAMPLE_LIGHT
    l2d_light_example_start();
#else
    ESP_LOGI(TAG, "=== Live2D RT30 baseline ===");
    xTaskCreatePinnedToCore(render_task, "l2d_base", TASK_STACK, NULL, TASK_PRIORITY, NULL,
                            TASK_CORE);
#endif
}
