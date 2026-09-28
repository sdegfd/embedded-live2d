/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ██  头文件说明 ──────────────────────────────────
 * WT99P4C5_S1 BSP 触摸屏驱动接口
 *
 * 触摸硬件配置：
 *   - 触摸 IC: GT911（电容式多点触摸）
 *   - 接口:    I2C (SDA=GPIO7, SCL=GPIO8)
 *   - 复位:    GPIO23
 *   - 中断:    GPIO21
 */

/**
 * @file
 * @brief BSP Touchscreen
 *
 * This file offers API for basic touchscreen initialization.
 * It is useful for users who want to use the touchscreen without the default Graphical Library LVGL.
 *
 * For standard LCD initialization with LVGL graphical library, you can call all-in-one function bsp_display_start().
 */

#pragma once
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup g04_display
 *  @{
 */

/**
 * @brief BSP touch configuration structure
 *
 */
typedef struct {
    void *dummy;    /*!< Prepared for future use. */
} bsp_touch_config_t;

/**
 * @brief Create new touchscreen
 *
 * If you want to free resources allocated by this function, you can use API:
 *
 * \code{.c}
 * bsp_touch_delete();
 * \endcode
 *
 * @param[in]  config    touch configuration
 * @param[out] ret_touch esp_lcd_touch touchscreen handle
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd_touch failure
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

/**
 * @brief Deinitialize touch
 */
void bsp_touch_delete(void);

/** @} */ // end of display
#ifdef __cplusplus
}
#endif
