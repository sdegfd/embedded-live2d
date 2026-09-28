/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ██  头文件说明 ──────────────────────────────────
 * WT99P4C5_S1 BSP 错误检查宏
 *
 * 本文件定义了 BSP 内部使用的错误处理宏：
 * - BSP_ERROR_CHECK_RETURN_ERR:  出错时返回 esp_err_t 错误码
 * - BSP_ERROR_CHECK_RETURN_NULL: 出错时返回 NULL
 * - BSP_ERROR_CHECK:             出错时返回指定值
 * - BSP_NULL_CHECK:              空指针检查并返回
 * - BSP_NULL_CHECK_GOTO:         空指针检查并跳转
 *
 * 当 CONFIG_BSP_ERROR_CHECK 启用时，以上宏直接调用 ESP_ERROR_CHECK 断言；
 * 否则在出错时返回对应的错误值（非断言模式）。
 */

#pragma once

#include "esp_check.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Assert on error, if selected in menuconfig. Otherwise return error code. */
#if CONFIG_BSP_ERROR_CHECK
#define BSP_ERROR_CHECK_RETURN_ERR(x)    ESP_ERROR_CHECK(x)
#define BSP_ERROR_CHECK_RETURN_NULL(x)   ESP_ERROR_CHECK(x)
#define BSP_ERROR_CHECK(x, ret)          ESP_ERROR_CHECK(x)
#define BSP_NULL_CHECK(x, ret)           assert(x)
#define BSP_NULL_CHECK_GOTO(x, goto_tag) assert(x)
#else
#define BSP_ERROR_CHECK_RETURN_ERR(x) do { \
        esp_err_t err_rc_ = (x);            \
        if (unlikely(err_rc_ != ESP_OK)) {  \
            return err_rc_;                 \
        }                                   \
    } while(0)

#define BSP_ERROR_CHECK_RETURN_NULL(x)  do { \
        if (unlikely((x) != ESP_OK)) {      \
            return NULL;                    \
        }                                   \
    } while(0)

#define BSP_NULL_CHECK(x, ret) do { \
        if ((x) == NULL) {          \
            return ret;             \
        }                           \
    } while(0)

#define BSP_ERROR_CHECK(x, ret)      do { \
        if (unlikely((x) != ESP_OK)) {    \
            return ret;                   \
        }                                 \
    } while(0)

#define BSP_NULL_CHECK_GOTO(x, goto_tag) do { \
        if ((x) == NULL) {      \
            goto goto_tag;      \
        }                       \
    } while(0)
#endif

#ifdef __cplusplus
}
#endif
