/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ██  头文件说明 ──────────────────────────────────
 * WT99P4C5_S1 BSP 板级支持包主头文件
 *
 * 硬件资源分配与引脚映射总览（基于 ESP32-P4）：
 *   I2C:        SDA=GPIO7,  SCL=GPIO8          → ES8311 + GT911
 *   I2S 音频:   MCLK=GPIO13, BCLK=GPIO12,      → ES8311 编解码器
 *               LRCK=GPIO10, DOUT=GPIO9,
 *               DIN=GPIO11
 *   功放:       PA_EN=GPIO53                    → 音频功放使能
 *   MIPI DSI:   2-lane, 1Gbps/lane              → EK79007 驱动 IC
 *   背光 PWM:   BL=GPIO20                       → LEDC 通道
 *   触摸:       RST=GPIO23, INT=GPIO21          → GT911
 *   SDMMC:      CLK=GPIO43, CMD=GPIO44,         → MicroSD 卡槽
 *               D0=GPIO39, D1=GPIO40,
 *               D2=GPIO41, D3=GPIO42
 *   SD SPI:     SCK=GPIO43, MOSI=GPIO44,        → 与 SDMMC 引脚复用
 *               MISO=GPIO39, CS=GPIO42
 *   USB:        DP=GPIO20, DN=GPIO19            → USB Host
 *   以太网:     MDC/MDIO/PHY_RST 由 Kconfig 配置  → IP101 PHY
 *   DSI PHY:    LDO_VO3 (通道 3, 2.5V)          → MIPI DSI PHY 电源
 *   SD 电源:    LDO_VO4 (通道 4, 3.3V)          → SD 卡槽电源
 */

/**
 * @file
 * @brief ESP BSP: ESP32-P4 Function EV Board
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "driver/i2s_std.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "esp_codec_dev.h"
#include "sdkconfig.h"
#include "esp_eth_driver.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "lvgl.h"
#include "esp_lv_adapter.h"
#endif // BSP_CONFIG_NO_GRAPHIC_LIB == 0

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/

/** @defgroup boardname Board Name
 *  @brief BSP Board Name
 *  @{
 */
#define BSP_BOARD_ESP32_P4_FUNCTION_EV_BOARD
/** @} */ // end of boardname

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/

/** @defgroup capabilities Capabilities
 *  @brief BSP Capabilities
 *  @{
 */
#define BSP_CAPS_DISPLAY       1
#define BSP_CAPS_TOUCH         1
#define BSP_CAPS_BUTTONS       0
#define BSP_CAPS_AUDIO         1
#define BSP_CAPS_AUDIO_SPEAKER 1
#define BSP_CAPS_AUDIO_MIC     1
#define BSP_CAPS_SDCARD        1
#define BSP_CAPS_IMU           0
/** @} */ // end of capabilities

/**************************************************************************************************
 *  ESP-BOX pinout
 **************************************************************************************************/

/** @defgroup g01_i2c I2C
 *  @brief I2C BSP API
 *  @{
 */
#define BSP_I2C_SCL (GPIO_NUM_8)   /*!< I2C 时钟线，连接 ES8311 与 GT911 */
#define BSP_I2C_SDA (GPIO_NUM_7)   /*!< I2C 数据线，连接 ES8311 与 GT911 */
/** @} */ // end of i2c

/** @defgroup g03_audio Audio
 *  @brief Audio BSP API
 *  @{
 */
#define BSP_I2S_SCLK     (GPIO_NUM_12)  /*!< I2S 位时钟 BCLK，由主机驱动 */
#define BSP_I2S_MCLK     (GPIO_NUM_13)  /*!< I2S 主时钟 MCLK，提供 ES8311 系统时钟 */
#define BSP_I2S_LCLK     (GPIO_NUM_10)  /*!< I2S 帧时钟 LRCK/WS，标识声道起始 */
#define BSP_I2S_DOUT     (GPIO_NUM_9)   /*!< I2S 数据输出 DOUT，主机发送音频数据 */
#define BSP_I2S_DSIN     (GPIO_NUM_11)  /*!< I2S 数据输入 DIN，主机接收音频数据 */
#define BSP_POWER_AMP_IO (GPIO_NUM_53)  /*!< 音频功放使能引脚，高电平开启 */
/** @} */ // end of audio

/** @defgroup g04_display Display and Touch
 *  @brief Display BSP API
 *  @{
 */
#define BSP_LCD_BACKLIGHT (GPIO_NUM_20)  /*!< LCD 背光 PWM 控制引脚 */
#define BSP_LCD_RST       (GPIO_NUM_NC)  /*!< LCD 硬件复位（由 DSI PHY 控制，不分配专用 GPIO） */
#define BSP_LCD_TOUCH_RST (GPIO_NUM_23)  /*!< 触摸屏 GT911 复位引脚 */
#define BSP_LCD_TOUCH_INT (GPIO_NUM_21)  /*!< 触摸屏 GT911 中断引脚，开漏输出 */
/** @} */ // end of display

/** @defgroup g02_storage SD Card and SPIFFS
 *  @brief SPIFFS and SD card BSP API
 *  @{
 */
/* uSD card MMC */
#define BSP_SD_D0  (GPIO_NUM_39)  /*!< SDMMC 数据线 D0 / SPI MISO 复用 */
#define BSP_SD_D1  (GPIO_NUM_40)  /*!< SDMMC 数据线 D1 */
#define BSP_SD_D2  (GPIO_NUM_41)  /*!< SDMMC 数据线 D2 */
#define BSP_SD_D3  (GPIO_NUM_42)  /*!< SDMMC 数据线 D3 / SPI CS 复用 */
#define BSP_SD_CMD (GPIO_NUM_44)  /*!< SDMMC 命令线 CMD / SPI MOSI 复用 */
#define BSP_SD_CLK (GPIO_NUM_43)  /*!< SDMMC 时钟线 CLK / SPI SCK 复用 */

/* uSD card SPI */
#define BSP_SD_SPI_MISO (GPIO_NUM_39)  /*!< SD SPI MISO（与 SDMMC D0 复用） */
#define BSP_SD_SPI_CS   (GPIO_NUM_42)  /*!< SD SPI 片选 CS（与 SDMMC D3 复用） */
#define BSP_SD_SPI_MOSI (GPIO_NUM_44)  /*!< SD SPI MOSI（与 SDMMC CMD 复用） */
#define BSP_SD_SPI_CLK  (GPIO_NUM_43)  /*!< SD SPI 时钟 SCK（与 SDMMC CLK 复用） */
/** @} */ // end of storage

/** @defgroup g07_usb USB
 *  @brief USB BSP API
 *  @{
 */
#define BSP_USB_POS (GPIO_NUM_20)  /*!< USB D+ 数据正极 */
#define BSP_USB_NEG (GPIO_NUM_19)  /*!< USB D- 数据负极 */
/** @} */ // end of usb

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup g01_i2c
 *  @{
 */

/**************************************************************************************************
 *
 * I2C interface
 *
 * There are multiple devices connected to I2C peripheral:
 *  - Codec ES8311 (configuration only)
 *  - LCD Touch controller
 **************************************************************************************************/
#define BSP_I2C_NUM CONFIG_BSP_I2C_NUM

/**
 * @brief Init I2C driver
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C driver handle
 *
 * @return
 *      - I2C handle
 *
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/** @} */ // end of i2c

/** \addtogroup g03_audio
 *  @{
 */

/**************************************************************************************************
 *
 * I2S audio interface
 *
 * There are two devices connected to the I2S peripheral:
 *  - Codec ES8311 for output(playback) and input(recording) path
 *
 * For speaker initialization use bsp_audio_codec_speaker_init() which is inside initialize I2S with bsp_audio_init().
 * For microphone initialization use bsp_audio_codec_microphone_init() which is inside initialize I2S with
 *bsp_audio_init(). After speaker or microphone initialization, use functions from esp_codec_dev for play/record audio.
 * Example audio play:
 * \code{.c}
 * esp_codec_dev_set_out_vol(spk_codec_dev, DEFAULT_VOLUME);
 * esp_codec_dev_open(spk_codec_dev, &fs);
 * esp_codec_dev_write(spk_codec_dev, wav_bytes, bytes_read_from_spiffs);
 * esp_codec_dev_close(spk_codec_dev);
 * \endcode
 **************************************************************************************************/

/**
 * @brief Init audio
 *
 * @note There is no deinit audio function. Users can free audio resources by calling i2s_del_channel()
 * @warning The type of i2s_config param is depending on IDF version.
 * @param[in]  i2s_config I2S configuration. Pass NULL to use default values (Mono, duplex, 16bit, 22050 Hz)
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_NOT_SUPPORTED The communication mode is not supported on the current chip
 *      - ESP_ERR_INVALID_ARG   NULL pointer or invalid configuration
 *      - ESP_ERR_NOT_FOUND     No available I2S channel found
 *      - ESP_ERR_NO_MEM        No memory for storing the channel information
 *      - ESP_ERR_INVALID_STATE This channel has not initialized or already started
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief Initialize speaker codec device
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief Initialize microphone codec device
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/** @} */ // end of audio

/** \addtogroup g02_storage
 *  @{
 */

/**************************************************************************************************
 *
 * SPIFFS
 *
 * After mounting the SPIFFS, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_SPIFFS_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello World!\n");
 * fclose(f);
 * \endcode
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT CONFIG_BSP_SPIFFS_MOUNT_POINT

/**
 * @brief Mount SPIFFS to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_spiffs_register was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes
 */
esp_err_t bsp_spiffs_mount(void);

/**
 * @brief Unmount SPIFFS from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the partition table does not contain SPIFFS partition with given label
 *      - ESP_ERR_INVALID_STATE if esp_vfs_spiffs_unregister was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes
 */
esp_err_t bsp_spiffs_unmount(void);

/**************************************************************************************************
 *
 * uSD card
 *
 * After mounting the uSD card, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello %s!\n", bsp_sdcard->cid.name);
 * fclose(f);
 * \endcode
 **************************************************************************************************/
#define BSP_SD_MOUNT_POINT CONFIG_BSP_SD_MOUNT_POINT
#define BSP_SDSPI_HOST     (SDSPI_DEFAULT_HOST)

/**
 * @brief BSP SD card configuration structure
 */
typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

/**
 * @brief Mount microSD card to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount microSD card from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the partition table does not contain FATFS partition with given label
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_spiflash_mount was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes from wear levelling library, SPI flash driver, or FATFS drivers
 */
esp_err_t bsp_sdcard_unmount(void);

/**
 * @brief Get SD card handle
 *
 * @return SD card handle
 */
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**
 * @brief Get SD card MMC host config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SD card SPI host config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SD card MMC slot config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);

/**
 * @brief Get SD card SPI slot config
 *
 * @param spi_host SPI host ID
 * @param config Structure which will be filled
 */
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);

/**
 * @brief Mount microSD card to virtual file system (MMC mode)
 *
 * @param cfg SD card configuration
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Mount microSD card to virtual file system (SPI mode)
 *
 * @param cfg SD card configuration
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/** @} */ // end of storage

/** \addtogroup g04_display
 *  @{
 */

/**************************************************************************************************
 *
 * LCD interface
 *
 * ESP-BOX is shipped with 2.4inch ST7789 display controller.
 * It features 16-bit colors, 320x240 resolution and capacitive touch controller.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/
#define BSP_LCD_PIXEL_CLOCK_MHZ (80)

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

#define BSP_LCD_DRAW_BUFF_SIZE   (BSP_LCD_H_RES * 50) // LVGL9 requires full screen height buffer for layer operations
#define BSP_LCD_DRAW_BUFF_DOUBLE (0)                   // Keep single buffer to save memory

/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    esp_lv_adapter_config_t lvgl_adapter_cfg; /*!< LVGL adapter configuration */
    uint32_t buffer_size;          /*!< Size of the buffer for the screen in pixels */
    bool double_buffer;            /*!< True, if should be allocated two buffers */
    bsp_display_config_t hw_cfg;   /*!< Display HW configuration */
    struct {
        unsigned int buff_dma : 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram : 1; /*!< Allocated LVGL buffer will be in PSRAM */
        unsigned int
            sw_rotate : 1; /*!< Use software rotation (slower), The feature is unavailable under avoid-tear mode */
    } flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize display
 *
 * This function initializes MIPI-DSI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes MIPI-DSI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Deinitialize display
 *
 * This function deinitializes MIPI-DSI, display controller and stops LVGL.
 *
 * @param display Pointer to LVGL display
 */
void bsp_display_stop(lv_display_t *display);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Get active LCD panel handle
 *
 * @return Panel handle or NULL if the display has not been initialized
 */
esp_lcd_panel_handle_t bsp_display_get_panel_handle(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate screen
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @param[in] disp Pointer to LVGL display
 * @param[in] rotation Angle of the display rotation
 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);
#endif // BSP_CONFIG_NO_GRAPHIC_LIB == 0

/** @} */ // end of display

/** \addtogroup g07_usb
 *  @{
 */

/**************************************************************************************************
 *
 * USB
 *
 **************************************************************************************************/

/**
 * @brief Power modes of USB Host connector
 */
typedef enum bsp_usb_host_power_mode_t {
    BSP_USB_HOST_POWER_MODE_USB_DEV, //!< Power from USB DEV port
} bsp_usb_host_power_mode_t;

/**
 * @brief Start USB host
 *
 * This is a one-stop-shop function that will configure the board for USB Host mode
 * and start USB Host library
 *
 * @param[in] mode        USB Host connector power mode (Not used on this board)
 * @param[in] limit_500mA Limit output current to 500mA (Not used on this board)
 * @return
 *     - ESP_OK                 On success
 *     - ESP_ERR_INVALID_ARG    Parameter error
 *     - ESP_ERR_NO_MEM         Memory cannot be allocated
 */
esp_err_t bsp_usb_host_start(bsp_usb_host_power_mode_t mode, bool limit_500mA);

/**
 * @brief Stop USB host
 *
 * USB Host lib will be uninstalled and power from connector removed.
 *
 * @return
 *     - ESP_OK              On success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t bsp_usb_host_stop(void);

/** @} */ // end of usb

esp_err_t bsp_eth_init(void);
esp_eth_handle_t bsp_eth_get_handle(void);
void bsp_eth_deinit(void);

#ifdef __cplusplus
}
#endif
