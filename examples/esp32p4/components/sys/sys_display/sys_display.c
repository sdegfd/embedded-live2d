/**
 * @file sys_display.c
 * @brief 显示系统实现（MIPI-DSI + LVGL封装 + Live2D ARGB8888 PPA 融合）
 *
 * 核心注入点：esp_lv_adapter_set_draw_bitmap_callbacks 注册 custom_draw_bitmap。
 * LVGL flush 完成后、panel push 前，PPA 将 Live2D ARGB8888 overlay 混合到
 * RGB565 帧缓冲上（硬件 alpha blend），消除矩形边界和画布背景问题。
 */

#include "sys_display.h"

#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/ppa.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lv_adapter_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sys_display";

/* Live2D overlay state (set by render task, consumed by draw_bitmap callback) */
static struct {
    const void *argb;           /* ARGB8888 frame buffer (PSRAM, 4 bpp) */
    int x, y, w, h;             /* overlay region in screen coords */
    SemaphoreHandle_t lock;     /* guards overlay update */
    ppa_client_handle_t ppa;    /* PPA blend client */
} s_overlay = {0};

static lv_display_t *s_disp = NULL;  /* stored for callback registration */

/* 屏幕录制帧捕获回调：在 LVGL flush 完成（含 Live2D 融合）后调用，传完整 fb */
static sys_display_capture_cb_t s_capture_cb = NULL;
static void *s_capture_ctx = NULL;

#define LIVE2D_BLEND_PROFILE_PERIOD 120

typedef struct {
    uint32_t frames;
    uint32_t lock_skip;
    uint64_t lock_us;
    uint64_t calc_us;
    uint64_t ppa_us;
    uint64_t total_us;
    uint32_t last_w;
    uint32_t last_h;
} live2d_blend_profile_t;

static live2d_blend_profile_t s_blend_profile = {0};

/**
 * @brief 覆盖 adapter 的弱函数 esp_lv_adapter_live2d_blend_hook。
 *
 * LVGL flush 完成后、面板推送前调用。frame_buffer 是当前 flush 区域的 RGB565 数据。
 * 如果当前有 Live2D ARGB8888 overlay，用 PPA 做 src-over blend（ARGB8888→RGB565,
 * per-pixel alpha）到 frame_buffer 上，原地修改。adapter 随后推送到面板。
 */
esp_err_t esp_lv_adapter_live2d_blend_hook(lv_display_t *disp,
                                            void *frame_buffer,
                                            uint32_t fb_w,
                                            uint32_t fb_h,
                                            uint8_t color_bytes,
                                            size_t frame_buffer_size,
                                            int rotation)
{
    (void)disp;
    (void)color_bytes;
    (void)rotation;

    if (!s_overlay.ppa || !s_overlay.argb || !frame_buffer) {
        /* 无 Live2D overlay：fb 已是完整 LVGL 帧，渲染完成点抓帧 */
        if (s_capture_cb && frame_buffer) {
            s_capture_cb(frame_buffer, s_capture_ctx);
        }
        return ESP_OK;
    }
    if (color_bytes != 2) return ESP_OK;  /* RGB565 only */
    int64_t total_begin_us = esp_timer_get_time();

    /* 快照 overlay 参数 */
    const void *argb = NULL;
    int ox = 0, oy = 0, ow = 0, oh = 0;
    int64_t lock_begin_us = total_begin_us;
    if (xSemaphoreTake(s_overlay.lock, 0) != pdTRUE) {
        s_blend_profile.lock_skip++;
        return ESP_OK;
    }
    argb = s_overlay.argb;
    ox   = s_overlay.x; oy = s_overlay.y;
    ow   = s_overlay.w; oh = s_overlay.h;
    xSemaphoreGive(s_overlay.lock);
    int64_t lock_done_us = esp_timer_get_time();

    if (!argb || ow <= 0 || oh <= 0) return ESP_OK;

    int px = ox;
    int py = oy;
    int pw = ow;
    int ph = oh;

    /* 如果 overlay 完全不在此帧缓冲内，跳过 */
    if (px >= (int)fb_w || py >= (int)fb_h || px + pw <= 0 || py + ph <= 0) {
        return ESP_OK;
    }

    /* 计算 overlay 在 frame_buffer 中的可见区域 */
    int vis_x1 = (px > 0) ? px : 0;
    int vis_y1 = (py > 0) ? py : 0;
    int vis_x2 = (px + pw < (int)fb_w) ? px + pw - 1 : (int)fb_w - 1;
    int vis_y2 = (py + ph < (int)fb_h) ? py + ph - 1 : (int)fb_h - 1;
    int vis_w = vis_x2 - vis_x1 + 1;
    int vis_h = vis_y2 - vis_y1 + 1;
    if (vis_w <= 0 || vis_h <= 0) return ESP_OK;

    uint8_t *fb = (uint8_t *)frame_buffer;
    int src_off_x = vis_x1 - px;
    int src_off_y = vis_y1 - py;
    int64_t blend_begin_us = esp_timer_get_time();

    ppa_blend_oper_config_t cfg = {
        .in_bg = {
            .buffer         = fb,
            .pic_w          = fb_w,
            .pic_h          = fb_h,
            .block_w        = (uint32_t)vis_w,
            .block_h        = (uint32_t)vis_h,
            .block_offset_x = (uint32_t)vis_x1,
            .block_offset_y = (uint32_t)vis_y1,
            .blend_cm       = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .in_fg = {
            .buffer         = argb,
            .pic_w          = (uint32_t)ow,
            .pic_h          = (uint32_t)oh,
            .block_w        = (uint32_t)vis_w,
            .block_h        = (uint32_t)vis_h,
            .block_offset_x = (uint32_t)src_off_x,
            .block_offset_y = (uint32_t)src_off_y,
            .blend_cm       = PPA_BLEND_COLOR_MODE_ARGB8888,
        },
        .out = {
            .buffer         = fb,
            .buffer_size    = frame_buffer_size,
            .pic_w          = fb_w,
            .pic_h          = fb_h,
            .block_offset_x = (uint32_t)vis_x1,
            .block_offset_y = (uint32_t)vis_y1,
            .blend_cm       = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .bg_alpha_fix_val     = 255,
        .fg_alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    esp_err_t ret = ppa_do_blend(s_overlay.ppa, &cfg);
    int64_t blend_done_us = esp_timer_get_time();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "live2d blend fail: %s", esp_err_to_name(ret));
    }
    /* Live2D 融合完成：fb 为含角色的完整帧，渲染完成点抓帧 */
    if (s_capture_cb && ret == ESP_OK) {
        s_capture_cb(frame_buffer, s_capture_ctx);
    }
    s_blend_profile.frames++;
    s_blend_profile.lock_us += (uint64_t)(lock_done_us - lock_begin_us);
    s_blend_profile.calc_us += (uint64_t)(blend_begin_us - lock_done_us);
    s_blend_profile.ppa_us += (uint64_t)(blend_done_us - blend_begin_us);
    s_blend_profile.total_us += (uint64_t)(blend_done_us - total_begin_us);
    s_blend_profile.last_w = (uint32_t)vis_w;
    s_blend_profile.last_h = (uint32_t)vis_h;
    if (s_blend_profile.frames >= LIVE2D_BLEND_PROFILE_PERIOD) {
        uint32_t frames = s_blend_profile.frames;
        ESP_LOGI(TAG,
                 "Live2D blend profile avg: frames=%lu lock_skip=%lu lock=%llu us calc=%llu us ppa=%llu us total=%llu us region=%lux%lu fb=%lux%lu",
                 (unsigned long)frames,
                 (unsigned long)s_blend_profile.lock_skip,
                 (unsigned long long)(s_blend_profile.lock_us / frames),
                 (unsigned long long)(s_blend_profile.calc_us / frames),
                 (unsigned long long)(s_blend_profile.ppa_us / frames),
                 (unsigned long long)(s_blend_profile.total_us / frames),
                 (unsigned long)s_blend_profile.last_w,
                 (unsigned long)s_blend_profile.last_h,
                 (unsigned long)fb_w,
                 (unsigned long)fb_h);
        memset(&s_blend_profile, 0, sizeof(s_blend_profile));
    }
    return ESP_OK;
}

/* ── 初始化 ──────────────────────────────────── */

lv_display_t *sys_display_init(void)
{
    lv_display_t *disp = bsp_display_start();
    if (!disp) { ESP_LOGE(TAG, "bsp_display_start failed"); return NULL; }
    s_disp = disp;
    ESP_LOGI(TAG, "Display + LVGL initialized (default config)");
    return disp;
}

lv_display_t *sys_display_init_with_config(const bsp_display_cfg_t *cfg)
{
    if (!cfg) return sys_display_init();
    lv_display_t *disp = bsp_display_start_with_config(cfg);
    if (!disp) { ESP_LOGE(TAG, "bsp_display_start_with_config failed"); return NULL; }
    s_disp = disp;
    ESP_LOGI(TAG, "Display + LVGL initialized (custom config)");
    return disp;
}

void sys_display_stop(lv_display_t *disp)
{
    bsp_display_stop(disp);
    s_disp = NULL;
    ESP_LOGI(TAG, "Display stopped");
}

/* ── 背光控制 ─────────────────────────────── */

esp_err_t sys_display_backlight_on(void)       { return bsp_display_backlight_on(); }
esp_err_t sys_display_backlight_off(void)      { return bsp_display_backlight_off(); }
esp_err_t sys_display_backlight_set(int v)     { return bsp_display_brightness_set(v); }

/* ── LVGL互斥锁 ────────────────────────────── */

bool sys_display_lock(uint32_t ms)              { return bsp_display_lock(ms); }
void sys_display_unlock(void)                   { bsp_display_unlock(); }

/* ── 面板句柄 ──────────────────────────── */

esp_lcd_panel_handle_t sys_display_get_panel_handle(void)
{
    return bsp_display_get_panel_handle();
}

void sys_display_set_capture_cb(sys_display_capture_cb_t cb, void *ctx)
{
    s_capture_cb = cb;
    s_capture_ctx = ctx;
}

/* ── Live2D Overlay API ────────────────────── */

esp_err_t sys_display_set_live2d_overlay(lv_display_t *disp,
                                          const void *argb_data,
                                          int x, int y, int w, int h)
{
    (void)disp;
    if (!s_overlay.lock) {
        if (!argb_data || w <= 0 || h <= 0) {
            return ESP_OK;
        }

        /* 首次调用：初始化 PPA blend client + mutex */
        s_overlay.lock = xSemaphoreCreateMutex();
        if (!s_overlay.lock) return ESP_ERR_NO_MEM;

        ppa_client_config_t cfg = {
            .oper_type = PPA_OPERATION_BLEND,
            .max_pending_trans_num = 2,
        };
        esp_err_t ret = ppa_register_client(&cfg, &s_overlay.ppa);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "PPA blend client fail: %s", esp_err_to_name(ret));
            s_overlay.ppa = NULL;
            return ret;
        }

        ESP_LOGI(TAG, "Live2D overlay PPA blend ready");
    }

    if (xSemaphoreTake(s_overlay.lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_overlay.argb = argb_data;
    s_overlay.x = x; s_overlay.y = y;
    s_overlay.w = w; s_overlay.h = h;
    xSemaphoreGive(s_overlay.lock);

    return ESP_OK;
}
