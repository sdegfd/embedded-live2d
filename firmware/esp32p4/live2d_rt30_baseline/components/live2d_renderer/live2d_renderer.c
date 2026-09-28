/**
 * @file live2d_renderer.c
 * @brief Live2D渲染器实现（待集成）
 *
 * ── 渲染管线位置 ──
 *   PainterEngine -> live2d_engine（动画/模型） -> live2d_renderer（格式转换） -> 显示缓冲区
 *
 * 本模块实现Live2D模型的渲染管线，包括ARGB8888到RGB565的像素格式转换，
 * 以及PPA（像素处理加速器）硬件加速转换。
 * 支持自动检测PPA配置参数（rgb_swap和byte_swap）以获得最精确的转换效果。
 *
 * @note 待集成：此模块尚未与 Game UI 框架完全集成，当前作为独立渲染器运行。
 */

#include "live2d_renderer.h"

#include <string.h>

#include "esp_cache.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "live2d_renderer";

#define LIVE2D_RENDERER_LOG_PERIOD_FRAMES 120  /**< 渲染帧统计日志输出周期 */

/* ── 内部辅助函数 ────────────────────────────────── */

static esp_err_t live2d_renderer_convert_to_rgb565_ppa(live2d_renderer_t *renderer);

static void live2d_renderer_update_dirty_rect(live2d_renderer_t *renderer,
                                              int origin_x, int origin_y,
                                              int model_w, int model_h)
{
    (void)origin_x;
    (void)origin_y;
    (void)model_w;
    (void)model_h;

    /* 起跳等大幅动画可能超出模型元数据矩形。overlay 路径用 PPA FILL 清透明，
     * 这里清整张 canvas，确保旧像素和 cache 同步范围都不会漏掉。 */
    renderer->dirty_x = 0;
    renderer->dirty_y = 0;
    renderer->dirty_w = renderer->buffer->width;
    renderer->dirty_h = renderer->buffer->height;
}

static void live2d_renderer_clear_dirty_rect_cpu(live2d_renderer_t *renderer)
{
    uint8_t *base = (uint8_t *)renderer->buffer->render_argb8888;
    const size_t stride = (size_t)renderer->buffer->width * sizeof(px_color);
    const size_t row_bytes = (size_t)renderer->dirty_w * sizeof(px_color);
    const int y_end = renderer->dirty_y + renderer->dirty_h;

    if (renderer->dirty_x == 0 && renderer->dirty_w == renderer->buffer->width) {
        memset(base + (size_t)renderer->dirty_y * stride, 0,
               (size_t)renderer->dirty_h * stride);
        return;
    }

    for (int y = renderer->dirty_y; y < y_end; ++y) {
        memset(base + (size_t)y * stride + (size_t)renderer->dirty_x * sizeof(px_color),
               0, row_bytes);
    }
}

static bool live2d_renderer_clear_dirty_rect_ppa(live2d_renderer_t *renderer)
{
    ppa_fill_oper_config_t fill_config = {
        .out.buffer = renderer->buffer->render_argb8888,
        .out.buffer_size = renderer->buffer->render_bytes,
        .out.pic_w = (uint32_t)renderer->buffer->width,
        .out.pic_h = (uint32_t)renderer->buffer->height,
        .out.block_offset_x = (uint32_t)renderer->dirty_x,
        .out.block_offset_y = (uint32_t)renderer->dirty_y,
        .out.fill_cm = PPA_FILL_COLOR_MODE_ARGB8888,
        .fill_block_w = (uint32_t)renderer->dirty_w,
        .fill_block_h = (uint32_t)renderer->dirty_h,
        .fill_argb_color.val = 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    int64_t fill_begin_us = esp_timer_get_time();
    esp_err_t ret = ppa_do_fill(renderer->ppa_fill_handle, &fill_config);
    int64_t sync_begin_us = esp_timer_get_time();
    renderer->last_clear_fill_us = (uint32_t)(sync_begin_us - fill_begin_us);

    if (ret != ESP_OK) {
        renderer->use_ppa_clear = false;
        ESP_LOGW(TAG, "PPA fill clear failed (%s), falling back to CPU clear",
                 esp_err_to_name(ret));
        return false;
    }

    const size_t stride = (size_t)renderer->buffer->width * sizeof(px_color);
    uint8_t *ptr = (uint8_t *)renderer->buffer->render_argb8888 +
                   (size_t)renderer->dirty_y * stride;
    const size_t bytes = (size_t)renderer->dirty_h * stride;
    ret = esp_cache_msync(ptr, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    int64_t sync_end_us = esp_timer_get_time();
    renderer->last_clear_sync_us = (uint32_t)(sync_end_us - sync_begin_us);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PPA fill cache M2C sync failed: %s", esp_err_to_name(ret));
    }
    return true;
}

static void live2d_renderer_clear_dirty_rect(live2d_renderer_t *renderer)
{
    if (!renderer || !renderer->buffer || !renderer->buffer->render_argb8888 ||
        renderer->dirty_w <= 0 || renderer->dirty_h <= 0) {
        return;
    }

    renderer->last_clear_fill_us = 0;
    renderer->last_clear_sync_us = 0;

    if (renderer->use_ppa_clear && renderer->ppa_fill_handle &&
        live2d_renderer_clear_dirty_rect_ppa(renderer)) {
        return;
    }

    int64_t fill_begin_us = esp_timer_get_time();
    live2d_renderer_clear_dirty_rect_cpu(renderer);
    renderer->last_clear_fill_us = (uint32_t)(esp_timer_get_time() - fill_begin_us);
}

/** 检查PPA硬件加速器是否支持当前PainterEngine的像素颜色格式。 */
static bool live2d_renderer_ppa_input_supported(void)
{
#if defined(PX_COLOR_FORMAT_RGBA) || defined(PX_COLOR_FORMAT_BGRA)
    return true;
#else
    return false;
#endif
}

/** 获取PPA默认的RGB交换设置，根据PainterEngine颜色格式决定。 */
static bool live2d_renderer_default_ppa_rgb_swap(void)
{
#if defined(PX_COLOR_FORMAT_RGBA)
    return true;
#else
    return false;
#endif
}

/** 根据性能缩放因子计算缩放后的尺寸。 */
static int live2d_renderer_scaled_dimension(int dimension)
{
    int scaled = (int)((float)dimension * LIVE2D_ENGINE_PERF_RENDER_SCALE + 0.5f);
    if (scaled < 1) {
        return 1;
    }
    if (scaled > dimension) {
        return dimension;
    }
    return scaled;
}

/** 判断PPA转换是否需要身份验证（源块覆盖整个渲染缓冲区）。 */
static bool live2d_renderer_ppa_requires_identity_validation(const live2d_renderer_t *renderer)
{
    return renderer->source_block_x == 0 &&
           renderer->source_block_y == 0 &&
           renderer->source_block_w == renderer->buffer->width &&
           renderer->source_block_h == renderer->buffer->height;
}

/** 将 PainterEngine 的预乘 ARGB8888 颜色值转换为 RGB565。 */
static inline uint16_t live2d_renderer_rgb565(px_color c)
{
#if defined(PX_COLOR_FORMAT_BGRA)
    /* ESP32-P4 little-endian BGRA: one aligned 32-bit load is cheaper than
     * three independent byte loads from PSRAM.  Alpha occupies bits 31:24
     * and is intentionally ignored because PainterEngine already composed RGB. */
    uint32_t bgra = c._argb.ucolor;
    return (uint16_t)(((bgra >> 8) & 0xF800u) |
                      ((bgra >> 5) & 0x07E0u) |
                      ((bgra >> 3) & 0x001Fu));
#else
    uint32_t r = c._argb.r;
    uint32_t g = c._argb.g;
    uint32_t b = c._argb.b;

    /* PainterEngine 已在写入透明画布时把 RGB 与 Alpha 合成；这里若再次乘
     * Alpha 会把半透明轮廓压暗两次，造成明显的黑锐边。 */
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
#endif
}

/** 软件方式将ARGB8888渲染缓冲区逐像素转换为RGB565帧缓冲区。 */
static void live2d_renderer_convert_to_rgb565(sys_display_buffer_t *buffer)
{
    const px_color *src = (const px_color *)buffer->render_argb8888;
    const size_t count = (size_t)buffer->width * (size_t)buffer->height;
    for (size_t i = 0; i < count; ++i) {
        buffer->frame_rgb565[i] = live2d_renderer_rgb565(src[i]);
    }
}

/** 统计PPA转换结果与软件参考值之间的RGB565像素差异数量。 */
static size_t live2d_renderer_count_rgb565_mismatches(sys_display_buffer_t *buffer,
                                                      size_t *first_mismatch,
                                                      uint16_t *first_expected,
                                                      uint16_t *first_actual)
{
    const px_color *src = (const px_color *)buffer->render_argb8888;
    const size_t count = (size_t)buffer->width * (size_t)buffer->height;
    size_t mismatch_count = 0;

    for (size_t i = 0; i < count; ++i) {
        uint16_t expected = live2d_renderer_rgb565(src[i]);
        uint16_t actual = buffer->frame_rgb565[i];
        if (expected != actual) {
            if (mismatch_count == 0) {
                *first_mismatch = i;
                *first_expected = expected;
                *first_actual = actual;
            }
            mismatch_count++;
        }
    }

    return mismatch_count;
}

/** 自动选择最佳的PPA配置参数（rgb_swap和byte_swap组合），遍历4种模式找出最匹配的。 */
static esp_err_t live2d_renderer_select_ppa_config(live2d_renderer_t *renderer)
{
    if (!live2d_renderer_ppa_requires_identity_validation(renderer)) {
        ESP_LOGI(TAG,
                 "Skipping exact PPA RGB565 validation for scaled source block %dx%d@(%d,%d); using rgb_swap=%d byte_swap=%d",
                 renderer->source_block_w, renderer->source_block_h,
                 renderer->source_block_x, renderer->source_block_y,
                 renderer->ppa_rgb_swap, renderer->ppa_byte_swap);
        return live2d_renderer_convert_to_rgb565_ppa(renderer);
    }

    static const bool candidate_swap_modes[4][2] = {
        {false, false},
        {true, false},
        {false, true},
        {true, true},
    };

    size_t best_mismatch_count = SIZE_MAX;
    bool best_rgb_swap = false;
    bool best_byte_swap = false;

    for (size_t i = 0; i < 4; ++i) {
        size_t first_mismatch = 0;
        uint16_t first_expected = 0;
        uint16_t first_actual = 0;

        renderer->ppa_rgb_swap = candidate_swap_modes[i][0];
        renderer->ppa_byte_swap = candidate_swap_modes[i][1];

        esp_err_t ret = live2d_renderer_convert_to_rgb565_ppa(renderer);
        if (ret != ESP_OK) {
            return ret;
        }

        size_t mismatch_count = live2d_renderer_count_rgb565_mismatches(renderer->buffer,
                                                                         &first_mismatch,
                                                                         &first_expected,
                                                                         &first_actual);
        if (mismatch_count < best_mismatch_count) {
            best_mismatch_count = mismatch_count;
            best_rgb_swap = renderer->ppa_rgb_swap;
            best_byte_swap = renderer->ppa_byte_swap;
        }

        if (mismatch_count == 0) {
            ESP_LOGI(TAG,
                     "PPA RGB565 validation passed: rgb_swap=%d byte_swap=%d",
                     renderer->ppa_rgb_swap, renderer->ppa_byte_swap);
            return ESP_OK;
        }

        px_color first_color = ((const px_color *)renderer->buffer->render_argb8888)[first_mismatch];
        ESP_LOGW(TAG,
                 "PPA RGB565 mismatch: rgb_swap=%d byte_swap=%d mismatches=%u first_pixel=%u argb=(%u,%u,%u,%u) ppa=0x%04x sw=0x%04x",
                 renderer->ppa_rgb_swap, renderer->ppa_byte_swap,
                 (unsigned)mismatch_count, (unsigned)first_mismatch,
                 (unsigned)first_color._argb.a, (unsigned)first_color._argb.r,
                 (unsigned)first_color._argb.g, (unsigned)first_color._argb.b,
                 (unsigned)first_actual, (unsigned)first_expected);
    }

    ESP_LOGW(TAG,
             "No exact PPA RGB565 config match found, best was rgb_swap=%d byte_swap=%d with %u mismatches",
             best_rgb_swap, best_byte_swap, (unsigned)best_mismatch_count);
    renderer->ppa_rgb_swap = best_rgb_swap;
    renderer->ppa_byte_swap = best_byte_swap;
    return ESP_ERR_INVALID_RESPONSE;
}

/** 使用PPA硬件SRM（缩放/旋转/镜像）模块执行ARGB8888到RGB565的格式转换。 */
static esp_err_t live2d_renderer_convert_to_rgb565_ppa(live2d_renderer_t *renderer)
{
    int64_t ppa_begin_us = esp_timer_get_time();
    ppa_srm_oper_config_t srm_config = {
        .in.buffer = renderer->buffer->render_argb8888,
        .in.pic_w = (uint32_t)renderer->buffer->width,
        .in.pic_h = (uint32_t)renderer->buffer->height,
        .in.block_w = (uint32_t)renderer->source_block_w,
        .in.block_h = (uint32_t)renderer->source_block_h,
        .in.block_offset_x = (uint32_t)renderer->source_block_x,
        .in.block_offset_y = (uint32_t)renderer->source_block_y,
        .in.srm_cm = PPA_SRM_COLOR_MODE_ARGB8888,
        .out.buffer = renderer->buffer->frame_rgb565,
        .out.buffer_size = renderer->buffer->frame_bytes,
        .out.pic_w = (uint32_t)renderer->buffer->width,
        .out.pic_h = (uint32_t)renderer->buffer->height,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = (float)renderer->buffer->width / (float)renderer->source_block_w,
        .scale_y = (float)renderer->buffer->height / (float)renderer->source_block_h,
        .rgb_swap = renderer->ppa_rgb_swap,
        .byte_swap = renderer->ppa_byte_swap,
        .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    esp_err_t ret = ppa_do_scale_rotate_mirror(renderer->ppa_srm_handle, &srm_config);
    renderer->last_ppa_convert_us = (uint32_t)(esp_timer_get_time() - ppa_begin_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PPA SRM convert failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

/* ── 公开API ────────────────────────────────── */

/** 初始化渲染器，绑定显示缓冲区并注册PPA硬件客户端。
 *
 * 初始化时自动检测像素格式、配置 PPA SRM 硬件并计算居中缩放的源块参数。
 * 若 PPA 硬件不可用，自动回退到软件转换模式。
 *
 * @param renderer  渲染器实例
 * @param buffer    显示缓冲区（需包含 render_argb8888；frame_rgb565 仅 RGB565 输出路径需要）
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t live2d_renderer_init(live2d_renderer_t *renderer, sys_display_buffer_t *buffer)
{
    if (!renderer || !buffer || !buffer->render_argb8888) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(renderer, 0, sizeof(*renderer));
    renderer->buffer = buffer;
    renderer->render_surface.MP = NULL;
    renderer->render_surface.surfaceBuffer = (px_color *)buffer->render_argb8888;
    renderer->render_surface.width = buffer->width;
    renderer->render_surface.height = buffer->height;
    renderer->render_surface.limit_left = 0;
    renderer->render_surface.limit_top = 0;
    renderer->render_surface.limit_right = buffer->width - 1;
    renderer->render_surface.limit_bottom = buffer->height - 1;
    renderer->source_block_w = live2d_renderer_scaled_dimension(buffer->width);
    renderer->source_block_h = live2d_renderer_scaled_dimension(buffer->height);
    renderer->source_block_x = (buffer->width - renderer->source_block_w) / 2;
    renderer->source_block_y = (buffer->height - renderer->source_block_h) / 2;
    renderer->convert_rgb565 = buffer->frame_rgb565 != NULL;
    renderer->ppa_rgb_swap = live2d_renderer_default_ppa_rgb_swap();
    renderer->ppa_byte_swap = false;
    renderer->dirty_x = 0;
    renderer->dirty_y = 0;
    renderer->dirty_w = buffer->width;
    renderer->dirty_h = buffer->height;
    memset(buffer->render_argb8888, 0, buffer->render_bytes);
    esp_cache_msync(buffer->render_argb8888, buffer->render_bytes,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    ppa_client_config_t fill_client_config = {
        .oper_type = PPA_OPERATION_FILL,
        .max_pending_trans_num = 1,
    };
    esp_err_t fill_ret = ppa_register_client(&fill_client_config,
                                             &renderer->ppa_fill_handle);
    if (fill_ret == ESP_OK) {
        renderer->use_ppa_clear = true;
        ESP_LOGI(TAG, "PPA fill transparent clear enabled");
    } else {
        ESP_LOGW(TAG, "PPA fill unavailable for transparent clear (%s), using CPU clear",
                 esp_err_to_name(fill_ret));
    }

    if (!buffer->frame_rgb565) {
        ESP_LOGI(TAG,
                 "Renderer initialized: %dx%d ARGB8888 overlay only, source block=%dx%d@(%d,%d)",
                 buffer->width, buffer->height,
                 renderer->source_block_w, renderer->source_block_h,
                 renderer->source_block_x, renderer->source_block_y);
        return ESP_OK;
    }

    if (!live2d_renderer_ppa_input_supported()) {
        ESP_LOGW(TAG, "PPA conversion disabled: unsupported PainterEngine px_color layout");
        ESP_LOGI(TAG, "Renderer initialized: %dx%d ARGB8888 -> RGB565",
                 buffer->width, buffer->height);
        return ESP_OK;
    }

    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    esp_err_t ret = ppa_register_client(&ppa_client_config, &renderer->ppa_srm_handle);
    if (ret == ESP_OK) {
        renderer->use_ppa_convert = true;
        ESP_LOGI(TAG,
                 "Renderer initialized: %dx%d ARGB8888 -> RGB565 via PPA SRM, rgb_swap=%d, byte_swap=%d, source block=%dx%d@(%d,%d)",
                 buffer->width, buffer->height,
                 renderer->ppa_rgb_swap, renderer->ppa_byte_swap,
                 renderer->source_block_w, renderer->source_block_h,
                 renderer->source_block_x, renderer->source_block_y);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "PPA SRM unavailable for renderer conversion (%s), falling back to software",
             esp_err_to_name(ret));
    ESP_LOGI(TAG, "Renderer initialized: %dx%d ARGB8888 -> RGB565",
             buffer->width, buffer->height);
    return ESP_OK;
}

void live2d_renderer_deinit(live2d_renderer_t *renderer)
{
    if (!renderer) {
        return;
    }

    if (renderer->ppa_srm_handle) {
        esp_err_t ret = ppa_unregister_client(renderer->ppa_srm_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "PPA SRM unregister failed: %s", esp_err_to_name(ret));
        }
    }
    if (renderer->ppa_fill_handle) {
        esp_err_t ret = ppa_unregister_client(renderer->ppa_fill_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "PPA fill unregister failed: %s", esp_err_to_name(ret));
        }
    }
    memset(renderer, 0, sizeof(*renderer));
}

void live2d_renderer_set_rgb565_conversion(live2d_renderer_t *renderer, bool enabled)
{
    if (!renderer) {
        return;
    }
    if (enabled && (!renderer->buffer || !renderer->buffer->frame_rgb565)) {
        ESP_LOGW(TAG, "RGB565 conversion requested without frame buffer; keeping overlay-only mode");
        enabled = false;
    }
    renderer->convert_rgb565 = enabled;
    if (!enabled) {
        renderer->use_ppa_convert = false;
        if (renderer->ppa_srm_handle) {
            esp_err_t ret = ppa_unregister_client(renderer->ppa_srm_handle);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "PPA SRM unregister failed: %s", esp_err_to_name(ret));
            } else {
                renderer->ppa_srm_handle = NULL;
            }
        }
    }
}

/** 渲染一帧Live2D画面：清除表面、渲染模型、执行ARGB到RGB565的转换并统计性能。
 *
 * 内部流程：清空 surface -> 调用 live2d_engine_render -> ARGB8888 转 RGB565（PPA 或软件）-> 性能统计。
 * 首次渲染时自动执行 PPA 配置选择（遍历 4 种 rgb_swap/byte_swap 组合以找到最佳匹配）。
 *
 * @param renderer   渲染器实例
 * @param engine     Live2D 引擎实例（需已加载模型）
 * @param elapsed_ms 距上次渲染的毫秒数，用于驱动动画
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数无效或引擎未加载
 */
esp_err_t live2d_renderer_render_frame(live2d_renderer_t *renderer, live2d_engine_t *engine,
                                       uint32_t elapsed_ms)
{
    if (!renderer || !renderer->buffer || !engine || !live2d_engine_is_loaded(engine)) {
        return ESP_ERR_INVALID_ARG;
    }
    renderer->last_frame_id = renderer->buffer->frame_id;
    renderer->last_ppa_convert_us = 0;
    renderer->last_convert_sync_us = 0;

    live2d_engine_info_t info;
    live2d_engine_get_info(engine, &info);

    int x = (renderer->buffer->width - info.width) / 2;
    int y = (renderer->buffer->height - info.height) / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    renderer->last_origin_x = x;
    renderer->last_origin_y = y;
    int64_t bounds_begin_us = esp_timer_get_time();
    live2d_renderer_update_dirty_rect(renderer, x, y, info.width, info.height);
    renderer->last_bounds_us = (uint32_t)(esp_timer_get_time() - bounds_begin_us);

    int64_t clear_begin_us = esp_timer_get_time();

    /* 阶段1：清空 ARGB8888 透明表面并调用 PainterEngine Live2D 渲染管线 */
    live2d_renderer_clear_dirty_rect(renderer);
    int64_t engine_begin_us = esp_timer_get_time();
    live2d_engine_render(engine, &renderer->render_surface, x, y, PX_ALIGN_LEFTTOP, elapsed_ms);

    int64_t convert_begin_us = esp_timer_get_time();
    /* 阶段2：将 ARGB8888 渲染结果转换为显示所需的 RGB565 格式 */
    if (!renderer->convert_rgb565) {
        /* LVGL overlay 路径直接消费 ARGB8888 + alpha，由 sys_display 的 PPA BLEND
         * 在 flush 前融合到背景层；这里无需再做 RGB565 转换或 PPA 自检。 */
    } else if (renderer->use_ppa_convert) {
        esp_err_t ret = (renderer->rendered_frames == 0)
                            ? live2d_renderer_select_ppa_config(renderer)
                            : live2d_renderer_convert_to_rgb565_ppa(renderer);
        if (ret != ESP_OK) {
            /* PPA 转换失败，回退到软件逐像素转换 */
            renderer->use_ppa_convert = false;
            ESP_LOGW(TAG, "Disabling PPA conversion, reverting to software: %s",
                     esp_err_to_name(ret));
            live2d_renderer_convert_to_rgb565(renderer->buffer);
        }
    } else {
        live2d_renderer_convert_to_rgb565(renderer->buffer);
    }
    int64_t frame_end_us = esp_timer_get_time();

    /* 阶段3：统计渲染和转换耗时，用于性能监控 */
    renderer->last_clear_us = (uint32_t)(engine_begin_us - clear_begin_us);
    renderer->last_engine_us = (uint32_t)(convert_begin_us - engine_begin_us);
    renderer->last_render_us = (uint32_t)(convert_begin_us - clear_begin_us);
    renderer->last_convert_us = (uint32_t)(frame_end_us - convert_begin_us);
    renderer->accumulated_render_us += renderer->last_render_us;
    renderer->accumulated_clear_us += renderer->last_clear_us;
    renderer->accumulated_clear_fill_us += renderer->last_clear_fill_us;
    renderer->accumulated_clear_sync_us += renderer->last_clear_sync_us;
    renderer->accumulated_engine_us += renderer->last_engine_us;
    renderer->accumulated_convert_us += renderer->last_convert_us;

    renderer->rendered_frames++;
    if ((renderer->rendered_frames % LIVE2D_RENDERER_LOG_PERIOD_FRAMES) == 0) {
        renderer->avg_render_us = (uint32_t)(renderer->accumulated_render_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);
        renderer->avg_clear_us = (uint32_t)(renderer->accumulated_clear_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);
        renderer->avg_clear_fill_us = (uint32_t)(renderer->accumulated_clear_fill_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);
        renderer->avg_clear_sync_us = (uint32_t)(renderer->accumulated_clear_sync_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);
        renderer->avg_engine_us = (uint32_t)(renderer->accumulated_engine_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);
        renderer->avg_convert_us = (uint32_t)(renderer->accumulated_convert_us / (uint64_t)LIVE2D_RENDERER_LOG_PERIOD_FRAMES);

        ESP_LOGI(TAG,
                 "Rendered Live2D frame=%u, frame_dt=%u ms, origin=(%d,%d), dirty=%dx%d@(%d,%d), avg_render=%llu us, avg_clear=%llu us(fill=%llu sync=%llu), avg_engine=%llu us, avg_convert=%llu us, clear_mode=%s",
                 (unsigned)renderer->rendered_frames, (unsigned)elapsed_ms, x, y,
                 renderer->dirty_w, renderer->dirty_h, renderer->dirty_x, renderer->dirty_y,
                 (unsigned long long)renderer->avg_render_us,
                 (unsigned long long)renderer->avg_clear_us,
                 (unsigned long long)renderer->avg_clear_fill_us,
                 (unsigned long long)renderer->avg_clear_sync_us,
                 (unsigned long long)renderer->avg_engine_us,
                 (unsigned long long)renderer->avg_convert_us,
                 renderer->use_ppa_clear ? "ppa" : "cpu");

        renderer->accumulated_render_us = 0;
        renderer->accumulated_clear_us = 0;
        renderer->accumulated_clear_fill_us = 0;
        renderer->accumulated_clear_sync_us = 0;
        renderer->accumulated_engine_us = 0;
        renderer->accumulated_convert_us = 0;
    }
    return ESP_OK;
}
