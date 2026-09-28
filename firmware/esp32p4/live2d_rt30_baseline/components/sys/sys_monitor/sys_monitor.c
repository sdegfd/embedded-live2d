/**
 * @file sys_monitor.c
 * @brief CPU使用率和FPS监控实现
 *
 * 本模块实现系统性能监控功能，包括：
 * - 双核CPU使用率测量（通过最低优先级空闲任务计数）
 * - LVGL渲染帧率（FPS）统计
 * 每秒钟更新一次读数，CPU使用率通过空闲任务计数的比例推算。
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
#define MONITOR_CPU_IDLE_STACK     1024     /**< 空闲测量任务栈大小 */

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

/* ── 每核空闲计数器 ────────────────────────────── */

static volatile uint32_t s_idle_ctr[2];   /**< 空闲任务计数器（volatile防止编译器优化） */
static TaskHandle_t       s_idle_tasks[2]; /**< 空闲任务句柄 */
static uint32_t           s_prev_idle[2];  /**< 上次空闲计数值 */
static uint32_t           s_max_idle_rate[2]; /**< 观测到的最大空闲速率 */
static int64_t            s_last_cpu_us;   /**< 上次CPU读数更新时间戳 */
static int                s_cpu_usage[2] = {-1, -1}; /**< CPU使用率百分比 */

/* ── FPS跟踪 ──────────────────────────────────────── */

static volatile uint32_t s_render_count;     /**< LVGL渲染完成计数器 */
static uint32_t           s_last_render_count; /**< 上次FPS读数时的渲染计数 */
static int64_t            s_last_fps_us;      /**< 上次FPS更新时间戳 */
static int                s_fps_x10 = -1;     /**< FPS值（10倍，即FPS*10） */

/* ── 空闲测量任务（优先级0，每核一个） ─── */

/** 空闲测量任务：持续自增计数器以估算CPU空闲率。 */
static void idle_measure_task(void *arg)
{
    uint32_t core = (uint32_t)arg;
    while (1) {
        for (int i = 0; i < 64; ++i) {
            s_idle_ctr[core]++;
        }
        vTaskDelay(1);
    }
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
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MONITOR_LOG_INTERVAL_MS));
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

/** 启动CPU监控：在每个核心上创建最低优先级的空闲测量任务。 */
esp_err_t sys_monitor_start(void)
{
    int num_cores = CONFIG_FREERTOS_NUMBER_OF_CORES;
    for (int core = 0; core < num_cores; core++) {
        s_idle_ctr[core]    = 0;
        s_prev_idle[core]   = 0;
        s_max_idle_rate[core] = 0;
        s_cpu_usage[core]   = -1;

        BaseType_t ret = xTaskCreatePinnedToCore(
            idle_measure_task,
            core == 0 ? "sys_mon0" : "sys_mon1",
            MONITOR_CPU_IDLE_STACK,
            (void *)(uint32_t)core,
            0,                        /* 空闲优先级 */
            &s_idle_tasks[core],
            core);
        if (ret != pdPASS) {
            ESP_LOGW(TAG, "Failed to create idle task on core %d", core);
            s_idle_tasks[core] = NULL;
        }
    }

    s_last_cpu_us = esp_timer_get_time();
    s_last_fps_us = s_last_cpu_us;

    /* 启动周期性 CPU/FPS 串口打印任务 */
    (void)xTaskCreate(monitor_log_task, "sys_mon_log",
                      MONITOR_LOG_STACK, NULL, 1, &s_log_task);

    ESP_LOGI(TAG, "CPU monitor started (per-core idle tasks)");
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
    if (core < 0 || core > 1) return -1;
    if (!s_idle_tasks[core]) return -1;

    /* 间隔足够时间后才更新CPU读数 */
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_last_cpu_us;

    if (elapsed >= MONITOR_UPDATE_INTERVAL_US) {
        float sec = (float)elapsed / 1000000.0f;

        int num_cores = CONFIG_FREERTOS_NUMBER_OF_CORES;
        for (int c = 0; c < num_cores; c++) {
            if (!s_idle_tasks[c]) continue;

            uint32_t cur   = s_idle_ctr[c];
            uint32_t delta = cur - s_prev_idle[c];
            uint32_t rate  = (uint32_t)((float)delta / sec);

            if (rate > s_max_idle_rate[c]) s_max_idle_rate[c] = rate;

            if (s_max_idle_rate[c] > 0) {
                int u = (int)(100.0f * (1.0f - (float)rate / (float)s_max_idle_rate[c]));
                if (u < 0) u = 0;
                if (u > 100) u = 100;
                s_cpu_usage[c] = u;
            }
            s_prev_idle[c] = cur;
        }
        s_last_cpu_us = now;
    }
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
