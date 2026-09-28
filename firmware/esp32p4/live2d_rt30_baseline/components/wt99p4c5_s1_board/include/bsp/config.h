/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ██  头文件说明 ──────────────────────────────────
 * WT99P4C5_S1 BSP 编译配置
 *
 * 本文件控制 BSP 的编译时选项：
 * - BSP_CONFIG_NO_GRAPHIC_LIB: 是否排除 LVGL 图形库（默认包含）
 */

#pragma once

/**************************************************************************************************
 * BSP configuration
 **************************************************************************************************/
// By default, this BSP is shipped with LVGL graphical library. Enabling this option will exclude it.
// If you want to use BSP without LVGL, select BSP version with 'noglib' suffix.
#if !defined(BSP_CONFIG_NO_GRAPHIC_LIB) // Check if the symbol is not coming from compiler definitions (-D...)
#define BSP_CONFIG_NO_GRAPHIC_LIB (0)
#endif
