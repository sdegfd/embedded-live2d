/**
 * @file sys_sdio.c
 * @brief SD 卡异步串行读取服务实现
 *
 * 单一任务串行消费请求队列，分片 fread(256KB/片 + 5ms 让出)到调用方缓冲。
 * 串行化所有 SD 读，避免多调用方并发争 SDMMC mutex；片间让出喂 IWDT。
 */

#include "sys_sdio.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "sys_sdio";

#define SDIO_TASK_STACK      (8 * 1024)
#define SDIO_TASK_PRIORITY   4
#define SDIO_TASK_CORE      0
#define SDIO_QUEUE_LEN      20            /**< 与基线 hosted SDIO TX/RX 队列深度对齐 */
#define SDIO_CHUNK_BYTES     (256 * 1024)  /**< 每片读取 256KB */
#define SDIO_CHUNK_YIELD_MS  5             /**< 片间让出 5ms,喂 IWDT */

/**
 * 一个 SD 读请求。ret/out_size 是指针（xQueueSend 拷贝 struct 但指针值保留，
 * process_request 通过指针写回调用方栈变量）。done 信号量通知调用方完成。 */
typedef struct {
    const char *path;
    void       *dst;
    size_t      capacity;
    size_t     *out_size;      /* 输出：实际读取字节数（指针→调用方栈） */
    esp_err_t  *ret;            /* 输出：结果码（指针→调用方栈） */
    SemaphoreHandle_t done;
} sys_sdio_request_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t  s_task  = NULL;

static void process_request(const sys_sdio_request_t *req)
{
    sys_sdio_request_t *r = (sys_sdio_request_t *)req;
    esp_err_t result = ESP_FAIL;
    size_t   rd = 0;

    struct stat st;
    if (stat(r->path, &st) != 0) {
        ESP_LOGE(TAG, "stat fail: %s", r->path);
        result = ESP_ERR_NOT_FOUND;
        goto done;
    }
    if (st.st_size <= 0) {
        ESP_LOGE(TAG, "empty file: %s", r->path);
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }
    if ((size_t)st.st_size > r->capacity) {
        ESP_LOGE(TAG, "buffer too small: %s need=%lld cap=%u",
                 r->path, (long long)st.st_size, (unsigned)r->capacity);
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }

    FILE *f = fopen(r->path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen fail: %s", r->path);
        result = ESP_ERR_NOT_FOUND;
        goto done;
    }

    size_t total = (size_t)st.st_size;
    size_t done = 0;
    while (done < total) {
        size_t want = total - done;
        if (want > SDIO_CHUNK_BYTES) { want = SDIO_CHUNK_BYTES; }
        size_t got = fread((uint8_t *)r->dst + done, 1, want, f);
        if (got == 0) { break; }
        done += got;
        if (done < total) {
            vTaskDelay(pdMS_TO_TICKS(SDIO_CHUNK_YIELD_MS));
        }
    }
    rd = done;
    fclose(f);

    if (rd != total) {
        ESP_LOGE(TAG, "short read %s (%u/%u)", r->path, (unsigned)rd, (unsigned)total);
        result = ESP_FAIL;
        goto done;
    }

    result = ESP_OK;
    ESP_LOGD(TAG, "read ok: %s (%uB chunked %uKB/%ums)", r->path, (unsigned)rd,
             (unsigned)(SDIO_CHUNK_BYTES / 1024), SDIO_CHUNK_YIELD_MS);

done:
    if (r->ret)      *r->ret = result;
    if (r->out_size) *r->out_size = rd;
    if (r->done)      xSemaphoreGive(r->done);
}

static void sys_sdio_task(void *arg)
{
    (void)arg;
    sys_sdio_request_t req;
    while (1) {
        if (xQueueReceive(s_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        process_request(&req);
    }
}

esp_err_t sys_sdio_init(void)
{
    if (s_task) return ESP_OK;

    s_queue = xQueueCreate(SDIO_QUEUE_LEN, sizeof(sys_sdio_request_t));
    if (!s_queue) { ESP_LOGE(TAG, "queue create fail"); return ESP_ERR_NO_MEM; }

    BaseType_t ok = xTaskCreatePinnedToCore(sys_sdio_task, "sys_sdio",
                                            SDIO_TASK_STACK, NULL,
                                            SDIO_TASK_PRIORITY, &s_task, SDIO_TASK_CORE);
    if (ok != pdPASS) {
        vQueueDelete(s_queue); s_queue = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "ready: SD serial task core=%d prio=%d queue=%d",
             SDIO_TASK_CORE, SDIO_TASK_PRIORITY, SDIO_QUEUE_LEN);
    return ESP_OK;
}

esp_err_t sys_sdio_read_file(const char *path, void *dst, size_t capacity, size_t *out_size)
{
    if (!path || !dst) return ESP_ERR_INVALID_ARG;
    if (!s_task) {
        ESP_LOGE(TAG, "not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    size_t    rd = 0;

    sys_sdio_request_t req = {
        .path = path,
        .dst  = dst,
        .capacity = capacity,
        .out_size = &rd,
        .ret      = &result,
        .done = xSemaphoreCreateBinary(),
    };
    if (!req.done) return ESP_ERR_NO_MEM;

    if (xQueueSend(s_queue, &req, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "queue full: %s", path);
        vSemaphoreDelete(req.done);
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreTake(req.done, portMAX_DELAY);
    vSemaphoreDelete(req.done);

    if (out_size) *out_size = rd;
    return result;
}
