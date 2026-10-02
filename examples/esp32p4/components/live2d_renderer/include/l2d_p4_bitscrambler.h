#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 创建 GPSPI2 的 DMA 回环转换后端；max_bytes 为最大输入字节数。 */
esp_err_t l2d_p4_bs_create(void **handle, size_t max_bytes);
/** @brief 所有转换完成后释放 BitScrambler、GDMA 和同步资源。 */
void l2d_p4_bs_free(void *handle);
/** @brief 同步执行逐位一致的格式转换；缓冲地址及输出长度须按 cache line 对齐。 */
esp_err_t l2d_p4_bs_convert(void *handle, void *bgra, size_t bgra_bytes,
                            void *rgb565, size_t rgb_bytes);
/** @brief 验证多种长度、颜色、输出长度和尾部保护字；仅初始化时调用。 */
bool l2d_p4_bs_selftest(void *handle);
#ifdef __cplusplus
}
#endif
