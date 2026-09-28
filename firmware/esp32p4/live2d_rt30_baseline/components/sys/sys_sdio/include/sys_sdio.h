/**
 * @file sys_sdio.h
 * @brief SD 卡异步串行读取服务
 *
 * ── 设计目的 ──
 * 单一任务串行化所有 SD 读请求，避免多调用方并发争用 SDMMC mutex；为 game 等
 * 上层提供干净的异步读取框架。
 *
 * ── 工作模型 ──
 *   1. 一个专用任务（core0 低优先级）串行消费队列里的所有 SD 读请求。
 *   2. 每个请求：stat → fopen → 分片 fread 到调用方提供的目标缓冲 →
 *      fclose，片间短暂让出以降低 IWDT/显示链路压力。
 *   3. 调用方发起请求后阻塞等待完成信号量，拿到已落在其缓冲里的数据。
 *
 *   多调用方 → [请求队列] → SD任务串行消费 → 分片读到调用方缓冲 → 完成通知
 *
 * ── 内存归属 ──
 *   调用方负责分配/释放目标缓冲（通常为 PSRAM，大小=文件大小）。sys_sdio 不持有数据缓冲。
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SD 异步读取服务。
 *
 * 在 SD 卡已由 BSP/sys_storage 挂载后，创建 SD 任务与请求队列。由 sys_storage_init
 * 末尾自动调用，也可单独调用。可安全多次调用（已初始化则空操作）。
 *
 * @return ESP_OK 成功，否则错误码
 */
esp_err_t sys_sdio_init(void);

/**
 * @brief 异步（对调用方表现为阻塞）读取整个 SD 文件到调用方提供的缓冲。
 *
 * 请求串行排队，由 SD 任务分片 fread 到 dst。调用方阻塞到读取完成。
 * 多调用方安全（队列串行化，无并发争 SDMMC）。
 *
 * @param path       SD 卡文件路径（如 "/sdcard/release.live"）
 * @param dst        调用方提供的目标缓冲（建议 PSRAM，大小 >= 文件大小）
 * @param capacity   dst 缓冲容量（字节）。文件大于此值返回 ESP_ERR_INVALID_SIZE
 * @param out_size   输出实际读取字节数（可为 NULL）
 * @return ESP_OK 成功；ESP_ERR_NOT_FOUND 文件不存在；ESP_ERR_INVALID_SIZE 缓冲不足；其它错误码
 */
esp_err_t sys_sdio_read_file(const char *path, void *dst, size_t capacity, size_t *out_size);

#ifdef __cplusplus
}
#endif
