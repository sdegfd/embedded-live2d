/**
 * @file sys_monitor.c
 * @brief CPU使用率和FPS监控实现
 *
 * 本模块实现系统性能监控功能，包括：
 * - 双核CPU使用率测量（FreeRTOS 每核 Idle 任务运行时间）
 * - LVGL渲染帧率（FPS）统计
 * 每秒钟更新一次读数，使用相邻采样窗口内的空闲时间计算占用率。
 */

#include "sys_monitor.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sys_monitor";    /**< 日志标签 */

#define MONITOR_UPDATE_INTERVAL_US 1000000  /**< 监控更新间隔：1秒 */

/* ── 任务栈水位监测注册表 ─────────────────────────── */

#define MONITOR_TASK_TABLE_MAX 12  /**< 注册任务上限（internal 调优关注的关键任务数） */

typedef struct {
    char  name[16];      /**< 任务显示名 */
    void *handle_ptr;    /**< 指向各组件 static 句柄(TaskHandle_t)的指针，可为 NULL */
} monitor_task_entry_t;

static monitor_task_entry_t s_task_table[MONITOR_TASK_TABLE_MAX];
static int                  s_task_table_cnt = 0;

/* 默认关心任务名表：运行时用 xTaskGetHandle(name) 查句柄，无需各组件暴露 getter。
 * 依赖 INCLUDE_xTaskGetHandle（IDF FreeRTOSConfig.h 默认=1），不依赖 TRACE_FACILITY。
 * 覆盖 internal SRAM 调优最关心的栈大户；任务未创建时 xTaskGetHandle 返回 NULL，自动跳过。 */
static const char *const s_default_task_names[] = {
    "lvgl",            /* 64K，LVGL adapter worker（lv_timer_handler+事件分发），阶段1a 降栈目标 */
    "swdraw",          /* 32K×DRAW_UNIT_CNT，LVGL SW draw 线程，FreeType 字形栅格化在此（同名多任务，xTaskGetHandle 只测到一个） */
    "page_voice",      /* 24K internal-caps，FOCUS 语音监听 */
    "agent_core",      /* 24K internal-caps，Agent 核心 */
    "agent_asr",       /* 16K internal-caps，ASR */
    "agent_llm_async", /* 8K，LLM 异步（临时，每轮对话） */
    "music_player",    /* 8K internal-caps，MP3 解码 */
    "game_runtime",    /* 32K，运行时事件循环 */
    "sys_sdio",        /* 8K，串行 SD 读 */
    "sys_mon_log",     /* 4K，本打印任务 */
};

/* FreeRTOS 运行时间计数器与 esp_timer 均以微秒计时。仅监控任务写入，
 * render/LVGL 任务只读取缓存值，避免跨核采样状态竞争。 */
static configRUN_TIME_COUNTER_TYPE s_prev_idle_us[2];
static int64_t s_last_cpu_us;
static volatile int s_cpu_usage[2] = {-1, -1};

/* ── FPS跟踪 ──────────────────────────────────────── */

static volatile uint32_t s_render_count;     /**< LVGL渲染完成计数器 */
static uint32_t           s_last_render_count; /**< 上次FPS读数时的渲染计数 */
static int64_t            s_last_fps_us;      /**< 上次FPS更新时间戳 */
static int                s_fps_x10 = -1;     /**< FPS值（10倍，即FPS*10） */

/* 每秒采样一次。CPU1 上运行本任务，使 CPU1 Idle 在采样前发生切换，
 * FreeRTOS 才会把当前运行段计入其累计运行时间。CPU0 渲染任务按帧切换。 */
static void sample_cpu_usage(void)
{
    int64_t now = esp_timer_get_time();
    uint64_t elapsed_us = (uint64_t)(now - s_last_cpu_us);
    if (elapsed_us == 0) return;

    for (int core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; ++core) {
        configRUN_TIME_COUNTER_TYPE idle_now = ulTaskGetIdleRunTimeCounterForCore(core);
        uint64_t idle_us = (uint64_t)(idle_now - s_prev_idle_us[core]);
        if (idle_us > elapsed_us) idle_us = elapsed_us;
        s_cpu_usage[core] = (int)((100 * (elapsed_us - idle_us) + elapsed_us / 2) / elapsed_us);
        s_prev_idle_us[core] = idle_now;
    }
    s_last_cpu_us = now;
}

/* ── LVGL渲染回调 ──────────────────────────────── */

/** LVGL渲染完成事件回调：递增帧计数器。 */
static void on_lvgl_render_ready(lv_event_t *e)
{
    (void)e;
    s_render_count++;
}

/* ── 周期性串口打印 ──────────────────────────────── */

#define MONITOR_LOG_INTERVAL_MS 5000   /**< CPU/FPS 串口打印间隔：5 秒 */
#define MONITOR_LOG_STACK        4096  /**< 打印任务栈大小（ESP_LOGI→uart_write→递归互斥锁链路约 3KB，2048 不足会触发栈尾 watchpoint） */

static TaskHandle_t s_log_task = NULL;  /**< 打印任务句柄 */

/** 周期性打印 CPU 使用率与 FPS，便于排查负载与卡顿（每 5 秒一次）。
 *  单核模式只打印 core0，双核打印 core0/core1。 */
static void monitor_log_task(void *arg)
{
    (void)arg;
    int64_t last_log_us = esp_timer_get_time();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MONITOR_UPDATE_INTERVAL_US / 1000));
        sample_cpu_usage();
        int64_t now = esp_timer_get_time();
        if (now - last_log_us < MONITOR_LOG_INTERVAL_MS * 1000LL) continue;
        last_log_us = now;
        int cpu0 = sys_monitor_get_cpu_usage(0);
        int fps  = sys_monitor_get_fps_x10();
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
        int cpu1 = sys_monitor_get_cpu_usage(1);
        ESP_LOGI(TAG, "cpu0=%d%% cpu1=%d%% fps=%d.%d",
                 cpu0, cpu1, fps / 10, (fps < 0 ? 0 : (fps % 10)));
#else
        ESP_LOGI(TAG, "cpu0=%d%% fps=%d.%d",
                 cpu0, fps / 10, (fps < 0 ? 0 : (fps % 10)));
#endif
        /* 内存余量：internal 是调优瓶颈，PSRAM 一并观察 */
        ESP_LOGI(TAG, "mem int=%u/%u psram=%u/%u (free/largest B)",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        sys_monitor_log_task_watermarks();
    }
}

/* ── 公共API ────────────────────────────────────────── */

/** 启动CPU监控：读取 FreeRTOS Idle 任务运行时间，无需额外空闲任务或校准。 */
esp_err_t sys_monitor_start(void)
{
    for (int core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; ++core) {
        s_prev_idle_us[core] = ulTaskGetIdleRunTimeCounterForCore(core);
        s_cpu_usage[core] = -1;
    }

    s_last_cpu_us = esp_timer_get_time();
    s_last_fps_us = s_last_cpu_us;

    /* 启动周期性 CPU/FPS 串口打印任务 */
    BaseType_t ret = xTaskCreatePinnedToCore(monitor_log_task, "sys_mon_log",
                      MONITOR_LOG_STACK, NULL, 1, &s_log_task,
                      CONFIG_FREERTOS_NUMBER_OF_CORES > 1 ? 1 : 0);
    if (ret != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "CPU monitor started (FreeRTOS idle run time, 1 s windows)");
    return ESP_OK;
}

/** 将FPS跟踪绑定到指定LVGL显示设备上。 */
void sys_monitor_attach_display(lv_display_t *disp)
{
    if (!disp) return;
    lv_display_add_event_cb(disp, on_lvgl_render_ready,
                            LV_EVENT_RENDER_READY, NULL);
    ESP_LOGI(TAG, "FPS tracking attached to display");
}

/** 获取指定CPU核心的使用率百分比（0-100），-1表示不可用。*/
int sys_monitor_get_cpu_usage(int core)
{
    if (core < 0 || core >= CONFIG_FREERTOS_NUMBER_OF_CORES) return -1;
    return s_cpu_usage[core];
}

/** 获取10倍FPS值。每秒更新一次，-1表示尚未计算。 */
int sys_monitor_get_fps_x10(void)
{
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_last_fps_us;

    if (elapsed >= MONITOR_UPDATE_INTERVAL_US) {
        uint32_t renders = s_render_count;
        uint32_t delta = renders - s_last_render_count;

        if (elapsed > 0) {
            s_fps_x10 = (int)((delta * 10000000LL + (elapsed / 2)) / elapsed);
        }
        s_last_render_count = renders;
        s_last_fps_us = now;
    }
    return s_fps_x10;
}

esp_err_t sys_monitor_register_task(const char *name, void *handle_ptr)
{
    if (!name || !handle_ptr) return ESP_ERR_INVALID_ARG;
    if (s_task_table_cnt >= MONITOR_TASK_TABLE_MAX) return ESP_ERR_NO_MEM;

    monitor_task_entry_t *e = &s_task_table[s_task_table_cnt];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->handle_ptr = handle_ptr;
    s_task_table_cnt++;
    return ESP_OK;
}

void sys_monitor_log_task_watermarks(void)
{
    /* ESP-IDF 版 uxTaskGetStackHighWaterMark 返回字节（非 word），直接用。
     * 依赖 INCLUDE_uxTaskGetStackHighWaterMark（IDF 默认=1），无需 TRACE_FACILITY。 */

    /* 默认关心任务：按名查句柄，零侵入（无需各组件暴露 getter） */
    for (size_t i = 0; i < sizeof(s_default_task_names) / sizeof(s_default_task_names[0]); i++) {
        TaskHandle_t h = xTaskGetHandle(s_default_task_names[i]);
        if (!h) continue;  /* 任务尚未创建或已销毁 */
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(h);
        ESP_LOGI(TAG, "stack %-16s hwm=%u B", s_default_task_names[i], (unsigned)hwm);
    }
    /* 主动注册的任务（句柄指针方案） */
    for (int i = 0; i < s_task_table_cnt; i++) {
        TaskHandle_t h = (s_task_table[i].handle_ptr)
                         ? *(TaskHandle_t *)s_task_table[i].handle_ptr : NULL;
        if (!h) continue;
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(h);
        ESP_LOGI(TAG, "stack %-16s hwm=%u B", s_task_table[i].name, (unsigned)hwm);
    }
}
