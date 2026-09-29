/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file wt99p4c5_s1_board.c
 * @brief WT99P4C5_S1 开发板 BSP 板级支持包实现
 *
 * 本文件实现了 WT99P4C5_S1 开发板的板级初始化功能，包括：
 *   - I2C 主总线（SDA=GPIO7, SCL=GPIO8 → ES8311 + GT911）
 *   - SD 卡存储（SDMMC 4-bit / SPI 模式，LDO_VO4 供电）
 *   - SPIFFS 文件系统
 *   - 音频 ES8311 编解码器（I2S 接口，功放使能 GPIO53）
 *   - MIPI DSI 显示（EK79007, 2-lane, 1024x600, RGB565）
 *   - GT911 电容触摸屏（I2C, RST=GPIO23, INT=GPIO21）
 *   - USB Host 驱动
 *   - 以太网 IP101 PHY（MDIO/MDC/PHY_RST 由 Kconfig 配置）
 *   - LEDC PWM 背光控制（GPIO20）
 */

#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_vfs_fat.h"
#include "usb/usb_host.h"
#include "sd_pwr_ctrl_interface.h"
#include "esp_lcd_ek79007.h"
#include "bsp/wt99p4c5_s1_board.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_touch_gt911.h"
#include "bsp_err_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_eth_driver.h"
#include "esp_eth_netif_glue.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "hal/ldo_ll.h"

#if defined(CONFIG_FATFS_LFN_HEAP) || defined(CONFIG_FATFS_LFN_STACK)
#define BSP_SD_LONG_FILENAME_ENABLED 1
#else
#define BSP_SD_LONG_FILENAME_ENABLED 0
#endif

static const char *TAG = "WT99P4C5_S1_BOARD";

#define BSP_SD_PWR_CTRL_LDO_CHAN           (4)   /**< SD卡电源控制LDO通道号 */
#define BSP_SD_PWR_CTRL_INIT_VOLTAGE_MV    (3300) /**< SD卡初始电压(mV) */

/** SD卡LDO电源控制上下文 */
typedef struct {
    esp_ldo_channel_handle_t ldo_chan;  /**< LDO通道句柄 */
} bsp_sd_pwr_ctrl_ldo_ctx_t;

/** 设置SD卡电源电压。 */
static esp_err_t bsp_sd_pwr_ctrl_set_voltage(void *arg, int voltage_mv)
{
    ESP_RETURN_ON_FALSE(arg, ESP_ERR_INVALID_ARG, TAG, "invalid SD power control context");

    bsp_sd_pwr_ctrl_ldo_ctx_t *ctx = (bsp_sd_pwr_ctrl_ldo_ctx_t *)arg;
    return esp_ldo_channel_adjust_voltage(ctx->ldo_chan, voltage_mv);
}

/** 创建板载LDO的SD卡电源控制驱动。 */
static esp_err_t bsp_sd_pwr_ctrl_new_on_chip_ldo(sd_pwr_ctrl_handle_t *ret_drv)
{
    ESP_RETURN_ON_FALSE(ret_drv, ESP_ERR_INVALID_ARG, TAG, "invalid SD power control handle");

    sd_pwr_ctrl_drv_t *driver = (sd_pwr_ctrl_drv_t *)calloc(1, sizeof(sd_pwr_ctrl_drv_t));
    ESP_RETURN_ON_FALSE(driver, ESP_ERR_NO_MEM, TAG, "no memory for SD power control driver");

    bsp_sd_pwr_ctrl_ldo_ctx_t *ctx = (bsp_sd_pwr_ctrl_ldo_ctx_t *)calloc(1, sizeof(bsp_sd_pwr_ctrl_ldo_ctx_t));
    if (!ctx) {
        free(driver);
        return ESP_ERR_NO_MEM;
    }

    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_SD_PWR_CTRL_LDO_CHAN,
        .voltage_mv = BSP_SD_PWR_CTRL_INIT_VOLTAGE_MV,
        .flags.adjustable = true,
    };
    esp_err_t ret = esp_ldo_acquire_channel(&ldo_cfg, &ctx->ldo_chan);
    if (ret != ESP_OK) {
        free(ctx);
        free(driver);
        return ret;
    }

    driver->set_io_voltage = bsp_sd_pwr_ctrl_set_voltage;
    driver->ctx = ctx;
    *ret_drv = driver;
    return ESP_OK;
}

/** 删除板载LDO的SD卡电源控制驱动。 */
static esp_err_t bsp_sd_pwr_ctrl_del_on_chip_ldo(sd_pwr_ctrl_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid SD power control handle");

    bsp_sd_pwr_ctrl_ldo_ctx_t *ctx = (bsp_sd_pwr_ctrl_ldo_ctx_t *)handle->ctx;
    esp_err_t ret = ESP_OK;
    if (ctx && ctx->ldo_chan) {
        ret = esp_ldo_release_channel(ctx->ldo_chan);
    }

    free(ctx);
    free(handle);
    return ret;
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static lv_indev_t *disp_indev = NULL;
#endif // (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

static sdmmc_card_t *bsp_sdcard = NULL; // uSD card handle
static bool i2c_initialized = false;               /**< I2C是否已初始化 */
static bool spi_sd_initialized = false;            /**< SPI SD卡是否已初始化 */
static sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL; /**< SD卡LDO电源控制句柄 */
static TaskHandle_t usb_host_task;                  /**< USB Host库任务句柄 */
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static i2c_master_bus_handle_t i2c_handle = NULL; /**< I2C主总线句柄 */
#endif
static i2s_chan_handle_t i2s_tx_chan = NULL;       /**< I2S发送通道句柄 */
static i2s_chan_handle_t i2s_rx_chan = NULL;       /**< I2S接收通道句柄 */
static const audio_codec_data_if_t *i2s_data_if = NULL; /**< Codec数据接口 */
static const audio_codec_gpio_if_t *audio_gpio_if = NULL; /**< Codec GPIO接口 */
static const audio_codec_ctrl_if_t *audio_i2c_ctrl_if = NULL; /**< Codec I2C控制接口 */
static bsp_lcd_handles_t disp_handles;             /**< LCD句柄集合 */
static esp_ldo_channel_handle_t disp_phy_pwr_chan = NULL; /**< 显示PHY电源LDO句柄 */
static esp_lcd_touch_handle_t tp = NULL;            /**< 触摸屏句柄 */
static esp_lcd_panel_io_handle_t tp_io_handle = NULL; /**< 触摸屏I2C IO句柄 */

static esp_eth_handle_t eth_handle = NULL;          /**< 以太网驱动句柄 */
static esp_eth_netif_glue_handle_t glue = NULL;     /**< 以太网netif粘合层 */
static esp_netif_t *eth_netif = NULL;               /**< 以太网网络接口 */

#define BSP_LV_ADAPTER_DISPLAY_ROTATION  ESP_LV_ADAPTER_ROTATE_0     /**< LVGL显示旋转角度 */
#define BSP_LV_ADAPTER_DISPLAY_TEAR_MODE ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT /**< 撕裂避免模式 */
#define BSP_LV_ADAPTER_TASK_STACK_SIZE   (16 * 1024)  /**< LVGL适配器任务栈大小：仅跑 lv_timer_handler+事件分发，FreeType 字形栅格化在 SW draw 线程(LV_FREETYPE_USE_LVGL_PORT=n)，故 16K 足够（实测水位见 sys_monitor "stack lvgl"）*/
#define BSP_LCD_DPI_CLOCK_FREQ_MHZ       ((float)BSP_LCD_PIXEL_CLOCK_MHZ) /**< 使用官方基线 DPI 像素时钟 */

/* 可用于`i2s_std_gpio_config_t`和/或`i2s_std_config_t`初始化 */
#define BSP_I2S_GPIO_CFG           \
    {                              \
        .mclk = BSP_I2S_MCLK,      \
        .bclk = BSP_I2S_SCLK,      \
        .ws = BSP_I2S_LCLK,        \
        .dout = BSP_I2S_DOUT,      \
        .din = BSP_I2S_DSIN,       \
        .invert_flags =            \
            {                      \
                .mclk_inv = false, \
                .bclk_inv = false, \
                .ws_inv = false,   \
            },                     \
    }

/* `bsp_extra_audio_init()`默认使用的配置 */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

/* ── I2C 总线 ────────────────────────────────── */

/** 初始化I2C主总线。 */
esp_err_t bsp_i2c_init(void)
{
    /* I2C已经初始化 */
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

/** 反初始化I2C主总线。 */
esp_err_t bsp_i2c_deinit(void)
{
    if (i2c_initialized && i2c_handle) {
        BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
        i2c_initialized = false;
    }
    return ESP_OK;
}

/** 获取I2C主总线句柄。 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return i2c_handle;
}

/* ── SD 卡存储 ────────────────────────────────── */

/** 获取SD卡句柄。 */
sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}

/** 获取SDMMC主机配置（填充指定槽位）。 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();
    host_config.slot = slot;
    host_config.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

/** 获取SDSPI主机配置。 */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = slot;

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

/** 获取SDMMC槽位配置（4位宽，内部上拉）。 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config)
{
    assert(config);

    *config = (sdmmc_slot_config_t)SDMMC_SLOT_CONFIG_DEFAULT();

    config->cd = SDMMC_SLOT_NO_CD;
    config->wp = SDMMC_SLOT_NO_WP;
    config->width = 4;
    config->flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
}

/** 获取SDSPI槽位设备配置。 */
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    assert(config);
    memset(config, 0, sizeof(sdspi_device_config_t));

    config->gpio_cs = BSP_SD_SPI_CS;
    config->gpio_cd = SDSPI_SLOT_NO_CD;
    config->gpio_wp = SDSPI_SLOT_NO_WP;
    config->gpio_int = GPIO_NUM_NC;
    config->gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW;
    config->host_id = spi_host;
}

/** 通过SDMMC接口挂载SD卡。 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdmmc_slot_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 64 * 1024};
    assert(cfg);

    if (!cfg->mount) {
        cfg->mount = &mount_config;
    }

    if (!cfg->host) {
        /* 使用 SDMMC 槽位 0（4-bit 模式），该槽位连接板载 MicroSD 卡槽 */
        bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_0, &sdhost);
        cfg->host = &sdhost;
    }

    if (!cfg->slot.sdmmc) {
        bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_0, &sdslot);
        cfg->slot.sdmmc = &sdslot;
    }

    /* 初始化 SD 卡电源：通过板载 LDO_VO4（通道 4, 3.3V）为 SD 卡供电，
     * 提供可调节电压支持以兼容不同电压等级的 SD 卡。 */
    esp_err_t ret = bsp_sd_pwr_ctrl_new_on_chip_ldo(&pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return ret;
    }
    cfg->host->pwr_ctrl_handle = pwr_ctrl_handle;

#if !BSP_SD_LONG_FILENAME_ENABLED
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    ret = esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdmmc, cfg->mount, &bsp_sdcard);
    if (ret != ESP_OK) {
        bsp_sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        pwr_ctrl_handle = NULL;
    }
    return ret;
}

/** 通过SDSPI接口挂载SD卡。 */
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdspi_device_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 64 * 1024};
    assert(cfg);

    ESP_LOGD(TAG, "Initialize SPI bus");
    /* 初始化 SPI 总线用于 SD 卡通信（SPI 模式）。
     * 引脚映射：CLK=GPIO43, MOSI=GPIO44, MISO=GPIO39, CS=GPIO42
     * 注意：SPI 引脚与 SDMMC 模式引脚复用，不能同时使用。 */    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_SD_SPI_CLK,
        .mosi_io_num = BSP_SD_SPI_MOSI,
        .miso_io_num = BSP_SD_SPI_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4000,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_SDSPI_HOST, &buscfg, SDSPI_DEFAULT_DMA), TAG, "SPI init failed");
    spi_sd_initialized = true;

    if (!cfg->mount) {
        cfg->mount = &mount_config;
    }

    if (!cfg->host) {
        bsp_sdcard_get_sdspi_host(SDMMC_HOST_SLOT_0, &sdhost);
        cfg->host = &sdhost;
    }

    if (!cfg->slot.sdspi) {
        bsp_sdcard_sdspi_get_slot(BSP_SDSPI_HOST, &sdslot);
        cfg->slot.sdspi = &sdslot;
    }

    esp_err_t ret = bsp_sd_pwr_ctrl_new_on_chip_ldo(&pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return ret;
    }
    cfg->host->pwr_ctrl_handle = pwr_ctrl_handle;

#if !BSP_SD_LONG_FILENAME_ENABLED
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    ret = esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdspi, cfg->mount, &bsp_sdcard);
    if (ret != ESP_OK) {
        bsp_sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        pwr_ctrl_handle = NULL;
    }
    return ret;
}

/** 使用默认配置挂载SD卡（SDMMC方式）。 */
esp_err_t bsp_sdcard_mount(void)
{
    bsp_sdcard_cfg_t cfg = {0};
    return bsp_sdcard_sdmmc_mount(&cfg);
}

/** 卸载SD卡，释放电源控制和SPI总线资源。 */
esp_err_t bsp_sdcard_unmount(void)
{
    esp_err_t ret = ESP_OK;

    if (pwr_ctrl_handle) {
        ret |= bsp_sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        pwr_ctrl_handle = NULL;
    }

    ret |= esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;

    if (spi_sd_initialized) {
        ret |= spi_bus_free(BSP_SDSPI_HOST);
        spi_sd_initialized = false;
    }

    return ret;
}

/* ── SPIFFS 文件系统 ──────────────────────────── */

/** 挂载SPIFFS文件系统分区。 */
esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
        .format_if_mount_failed = false,
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

/** 卸载SPIFFS文件系统分区。 */
esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/* ── 音频（I2S + ES8311 Codec） ────────────────── */

/** 初始化I2S音频外设和codec数据接口。 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (!(i2s_tx_chan && i2s_rx_chan)) {
        /* 配置I2S外设 */
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
        chan_cfg.auto_clear = true; /**< 自动清除 DMA 缓冲区中的遗留数据，防止音频尾音重复播放 */
        chan_cfg.dma_frame_num = 256; /**< 为 ESP32-P4 保持 DMA 缓冲区自然对齐（PSRAM 访问要求 256 字节对齐） */
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));

        /* 配置I2S通道 */
        const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
        const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
        if (i2s_config != NULL) {
            p_i2s_cfg = i2s_config;
        }

        if (i2s_tx_chan != NULL) {
            ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
        }

        if (i2s_rx_chan != NULL) {
            ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_rx_chan, p_i2s_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(i2s_rx_chan));
        }
    }

    if (i2s_data_if == NULL) {
        audio_codec_i2s_cfg_t i2s_cfg = {
            .port = CONFIG_BSP_I2S_NUM,
            .tx_handle = i2s_tx_chan,
            .rx_handle = i2s_rx_chan,
        };
        i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
        ESP_RETURN_ON_FALSE(i2s_data_if, ESP_ERR_NO_MEM, TAG, "audio_codec_new_i2s_data failed");
    }

    return ESP_OK;
}

/** 确保codec控制接口已初始化（I2C、GPIO、I2C控制）。 */
static esp_err_t bsp_audio_ensure_codec_ctrl(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");

    if (audio_gpio_if == NULL) {
        audio_gpio_if = audio_codec_new_gpio();
        ESP_RETURN_ON_FALSE(audio_gpio_if, ESP_ERR_NO_MEM, TAG, "audio_codec_new_gpio failed");
    }

    if (audio_i2c_ctrl_if == NULL) {
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = BSP_I2C_NUM,
            .addr = ES8311_CODEC_DEFAULT_ADDR,
            .bus_handle = i2c_handle,
        };
        audio_i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        ESP_RETURN_ON_FALSE(audio_i2c_ctrl_if, ESP_FAIL, TAG, "audio_codec_new_i2c_ctrl failed");
    }

    return ESP_OK;
}

/** 初始化扬声器codec（ES8311），返回codec设备句柄。 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (i2s_data_if == NULL) {
        /* 初始化I2C */
        esp_err_t ret = bsp_i2c_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_i2c_init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        /* 配置I2S外设和功放 */
        ret = bsp_audio_init(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_audio_init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
    }
    if (!i2s_data_if) {
        ESP_LOGE(TAG, "speaker codec init aborted: i2s_data_if is NULL");
        return NULL;
    }

    esp_err_t ret = bsp_audio_ensure_codec_ctrl();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_ensure_codec_ctrl failed: %s", esp_err_to_name(ret));
        return NULL;
    }

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = audio_i2c_ctrl_if,
        .gpio_if = audio_gpio_if,
        .codec_mode = ESP_CODEC_DEV_TYPE_OUT,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    if (!es8311_dev) {
        ESP_LOGE(TAG, "es8311_codec_new failed for speaker path");
        return NULL;
    }

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

/** 初始化麦克风codec（ES8311），返回codec设备句柄。 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (i2s_data_if == NULL) {
        /* Initilize I2C */
        esp_err_t ret = bsp_i2c_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_i2c_init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        /* Configure I2S peripheral and Power Amplifier */
        ret = bsp_audio_init(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_audio_init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
    }
    if (!i2s_data_if) {
        ESP_LOGE(TAG, "microphone codec init aborted: i2s_data_if is NULL");
        return NULL;
    }

    esp_err_t ret = bsp_audio_ensure_codec_ctrl();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_ensure_codec_ctrl failed: %s", esp_err_to_name(ret));
        return NULL;
    }

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = audio_i2c_ctrl_if,
        .gpio_if = audio_gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };

    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    if (!es8311_dev) {
        ESP_LOGE(TAG, "es8311_codec_new failed for microphone path");
        return NULL;
    }

    esp_codec_dev_cfg_t codec_es8311_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es8311_dev_cfg);
}

/* ── 显示（LEDC 背光 + MIPI DSI + EK79007） ── */

/** 表示命令和参数的位编号 */
#define LCD_LEDC_CH CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

/** 初始化LEDC外设用于PWM背光控制。 */
esp_err_t bsp_display_brightness_init(void)
{
    // 配置LEDC外设用于PWM背光控制
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0,
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

/** 反初始化LEDC背光控制。 */
esp_err_t bsp_display_brightness_deinit(void)
{
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = 1,
        .deconfigure = 1,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_pause(LEDC_LOW_SPEED_MODE, 1));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    return ESP_OK;
}

/** 设置LCD背光亮度百分比（0-100）。 */
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC分辨率为10位, 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    return ESP_OK;
}

/** 关闭LCD背光。 */
esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

/** 打开LCD背光（100%亮度）。 */
esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

/** 为MIPI DSI PHY供电，使其从"无电源"状态进入"关闭"状态。 */
static esp_err_t bsp_enable_dsi_phy_power(void)
{
    // 开启MIPI DSI PHY电源，使其从"No Power"状态进入"Shutdown"状态
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &disp_phy_pwr_chan), TAG, "Acquire LDO channel for DPHY failed");
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");

    return ESP_OK;
}

/** 创建显示面板并返回面板和IO句柄。 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    bsp_lcd_handles_t handles;
    ret = bsp_display_new_with_handles(config, &handles);

    *ret_panel = handles.panel;
    *ret_io = handles.io;

    return ret;
}

/** 创建MIPI DSI + EK79007显示面板并返回所有句柄。 */
esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t disp_panel = NULL;

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");
    ESP_RETURN_ON_ERROR(bsp_enable_dsi_phy_power(), TAG, "DSI PHY power failed");

    /* 先创建MIPI DSI总线，同时也会初始化DSI PHY */
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = BSP_LCD_MIPI_DSI_LANE_NUM,
        .phy_clk_src = config->dsi_bus.phy_clk_src,
        .lane_bit_rate_mbps = config->dsi_bus.lane_bit_rate_mbps,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus), TAG, "New DSI bus init failed");

    if (config->hdmi_resolution != BSP_HDMI_RES_NONE) {
        ESP_LOGW(TAG, "Please select HDMI in menuconfig, if you want to use it.");
    }

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // 使用DBI接口发送LCD命令和参数
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   /* MIPI DSI DBI 命令宽度为 8 位，符合 EK79007 规格 */
        .lcd_param_bits = 8, /* MIPI DSI DBI 参数宽度为 8 位，符合 EK79007 规格 */
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io), err, TAG, "New panel IO failed");

    // 创建EK79007控制面板
    ESP_LOGI(TAG, "Install EK79007 LCD control panel");

    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_config.dpi_clock_freq_mhz = BSP_LCD_DPI_CLOCK_FREQ_MHZ;
    ESP_LOGI(TAG, "MIPI DSI DPI clock: %.2f MHz", (double)dpi_config.dpi_clock_freq_mhz);
    /* 根据显示撕裂避免模式和旋转角度计算所需的帧缓冲数量。
     * 撕裂避免模式下可能需要 2 个或 3 个帧缓冲来实现平滑刷新。 */
    dpi_config.num_fbs = esp_lv_adapter_get_required_frame_buffer_count(
                             BSP_LV_ADAPTER_DISPLAY_TEAR_MODE,
                             BSP_LV_ADAPTER_DISPLAY_ROTATION);

    ek79007_vendor_config_t vendor_config = {
        .mipi_config =
            {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = &dpi_config,
            },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 16,
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .reset_gpio_num = BSP_LCD_RST,
        .flags.reset_active_high = 1,
        .vendor_config = &vendor_config,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &disp_panel), err, TAG, "New LCD panel EK79007 failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(disp_panel), err, TAG, "LCD panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(disp_panel), err, TAG, "LCD panel init failed");

    /* 返回所有句柄 */
    ret_handles->io = io;
    disp_handles.io = io;
    ret_handles->mipi_dsi_bus = mipi_dsi_bus;
    disp_handles.mipi_dsi_bus = mipi_dsi_bus;
    ret_handles->panel = disp_panel;
    disp_handles.panel = disp_panel;
    ret_handles->control = NULL;
    disp_handles.control = NULL;

    ESP_LOGI(TAG, "Display initialized");

    return ret;

err:
    bsp_display_delete();
    return ret;
}

/** 删除显示面板及相关资源。 */
void bsp_display_delete(void)
{
    if (disp_handles.panel) {
        esp_lcd_panel_del(disp_handles.panel);
        disp_handles.panel = NULL;
    }
    if (disp_handles.io) {
        esp_lcd_panel_io_del(disp_handles.io);
        disp_handles.io = NULL;
    }
    if (disp_handles.mipi_dsi_bus) {
        esp_lcd_del_dsi_bus(disp_handles.mipi_dsi_bus);
        disp_handles.mipi_dsi_bus = NULL;
    }

    if (disp_phy_pwr_chan) {
        esp_ldo_release_channel(disp_phy_pwr_chan);
        disp_phy_pwr_chan = NULL;
    }

    bsp_display_brightness_deinit();
}

/* ── 触摸（GT911） ────────────────────────────── */

/** 创建GT911触摸屏设备。 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* 初始化I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    esp_lcd_touch_io_gt911_config_t dev_addr = {
        .dev_addr = 0x5D,   /* GT911 默认 I2C 设备地址（INT 引脚接低电平时的地址） */
    };

    /* 初始化触摸屏
     * 触摸与 LCD 共用复位引脚，实现同步上电复位。
     * GT911 中断引脚在初始化阶段用于"主机就绪"握手信号。 */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // 与LCD复位共享引脚
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels =
            {
                .reset = 1,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 1,
                .mirror_y = 1,
            },
        .driver_data = (void *)&dev_addr,
    };
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
}

/** 删除触摸屏设备。 */
void bsp_touch_delete(void)
{
    if (tp) {
        esp_lcd_touch_del(tp);
        tp = NULL;
    }
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
        tp_io_handle = NULL;
    }
}

/** 根据缓冲区大小和水平分辨率计算缓冲区高度。 */
static uint16_t bsp_display_calc_buffer_height(uint32_t buffer_size, uint32_t hor_res)
{
    if (hor_res == 0) {
        return 1;
    }

    uint32_t buffer_height = (buffer_size + hor_res - 1) / hor_res;
    if (buffer_height == 0) {
        buffer_height = 1;
    }

    return (uint16_t)buffer_height;
}

/** 记录显示适配器操作错误日志。 */
static void bsp_display_log_adapter_error(const char *action, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s failed: %s", action, esp_err_to_name(err));
    }
}

/** 初始化LCD显示并通过LVGL适配器注册。 */
static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new_with_handles(&cfg->hw_cfg, &disp_handles));

    uint32_t display_hres = BSP_LCD_H_RES;
    uint32_t display_vres = BSP_LCD_V_RES;

    ESP_LOGI(TAG, "Display resolution %ldx%ld", display_hres, display_vres);

    /* 添加LCD屏幕 */
    ESP_LOGD(TAG, "Add LCD screen");
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                    disp_handles.panel,
                                                    disp_handles.io,
                                                    display_hres,
                                                    display_vres,
                                                    BSP_LV_ADAPTER_DISPLAY_ROTATION);
    display_cfg.tear_avoid_mode = BSP_LV_ADAPTER_DISPLAY_TEAR_MODE;
    display_cfg.profile.buffer_height = bsp_display_calc_buffer_height(cfg->buffer_size, display_hres);
    display_cfg.profile.use_psram = cfg->flags.buff_spiram;
    display_cfg.profile.require_double_buffer = cfg->double_buffer;
#if CONFIG_SOC_PPA_SUPPORTED
    /* Live2D baseline owns the single PPA SRM path; rotation is disabled, so
     * the adapter-side SRM client is unnecessary for this configuration. */
    display_cfg.profile.enable_ppa_accel = false;
#endif

    return esp_lv_adapter_register_display(&display_cfg);
}

/** 初始化触摸输入设备并通过LVGL适配器注册。 */
static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    if (tp && tp->config.int_gpio_num != GPIO_NUM_NC) {
        int int_pin = tp->config.int_gpio_num;

        /* 完成GT911初始化：I2C地址选择后驱动将INT引脚保持为输入。
         * GT911需要INT引脚的上升沿（"主机就绪"信号）才能开始触摸扫描。
         * 先将INT驱动为高电平5ms，然后重新配置为输入+上拉，
         * 使开漏线在空闲时保持高电平。 */
        gpio_config_t int_high = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(int_pin),
        };
        gpio_config(&int_high);
        gpio_set_level(int_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(5));

        gpio_config_t int_input = {
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = 1,
            .pin_bit_mask = BIT64(int_pin),
        };
        gpio_config(&int_input);

        ESP_LOGI(TAG, "GT911 touch ready on GPIO %d", int_pin);
        tp->config.int_gpio_num = GPIO_NUM_NC;
        /* 主机就绪握手完成后GT911开始扫描。
         * 通过I2C轮询足够——INT线仅在初始握手中使用。 */
    }

    const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    return esp_lv_adapter_register_touch(&touch_cfg);
}

/** 使用默认配置启动显示和LVGL。 */
lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_adapter_cfg = {
            .task_priority = 4,
            .task_stack_size = BSP_LV_ADAPTER_TASK_STACK_SIZE,
            .task_core_id = -1,
            .tick_period_ms = 5,
            .task_min_delay_ms = 1,
            .task_max_delay_ms = 500,
            .stack_in_psram = false,
        },
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg =
            {
                .hdmi_resolution = BSP_HDMI_RES_NONE,
                .dsi_bus =
                    {
                        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
                        .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
                    },
            },
        .flags =
            {
                .buff_dma = true,
                .buff_spiram = true,
                .sw_rotate = true,
            },
    };
    return bsp_display_start_with_config(&cfg);
}

/** 使用自定义配置启动显示和LVGL。 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    lv_display_t *disp = NULL;

    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_init(&cfg->lvgl_adapter_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);
    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);
    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_start());
    return disp;
}

/** 停止显示和LVGL。 */
void bsp_display_stop(lv_display_t *display)
{
    if (disp_indev) {
        bsp_display_log_adapter_error("Unregister touch", esp_lv_adapter_unregister_touch(disp_indev));
        disp_indev = NULL;
    }
    if (display) {
        bsp_display_log_adapter_error("Unregister display", esp_lv_adapter_unregister_display(display));
    }
    bsp_display_log_adapter_error("Deinit LVGL adapter", esp_lv_adapter_deinit());

    /* Deinit touch */
    bsp_touch_delete();

    /* Deinit display */
    bsp_display_delete();

    /* Deinit I2C if initialized */
    bsp_i2c_deinit();
}

/** 获取LVGL输入设备句柄。 */
lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

/** 获取LCD面板句柄。 */
esp_lcd_panel_handle_t bsp_display_get_panel_handle(void)
{
    return disp_handles.panel;
}

/** 旋转LVGL显示方向。 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

/** 获取LVGL显示互斥锁。timeout_ms=0表示无限等待。 */
bool bsp_display_lock(uint32_t timeout_ms)
{
    int32_t adapter_timeout_ms = (timeout_ms == 0) ? -1 : (int32_t)timeout_ms;
    return esp_lv_adapter_lock(adapter_timeout_ms) == ESP_OK;
}

/** 释放LVGL显示互斥锁。 */
void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

/* ── USB Host ──────────────────────────────────── */

/** USB Host库事件处理任务。 */
static void usb_lib_task(void *arg)
{
    while (1) {
        // 开始处理系统事件
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
            // 继续处理USB事件以允许设备重新连接
            // 停止此任务的唯一方法是调用bsp_usb_host_stop()
        }
    }
}

/** 启动USB Host驱动和事件处理任务。 */
esp_err_t bsp_usb_host_start(bsp_usb_host_power_mode_t mode, bool limit_500mA)
{
    // 安装USB Host驱动，在整个应用程序中应只调用一次
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    BSP_ERROR_CHECK_RETURN_ERR(usb_host_install(&host_config));

    // 创建处理USB库事件的任务
    if (xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, &usb_host_task) != pdTRUE) {
        ESP_LOGE(TAG, "Creating USB host lib task failed");
        abort();
    }

    return ESP_OK;
}

/** 停止USB Host驱动并删除事件处理任务。 */
esp_err_t bsp_usb_host_stop(void)
{
    usb_host_uninstall();
    if (usb_host_task) {
        vTaskSuspend(usb_host_task);
        vTaskDelete(usb_host_task);
    }
    return ESP_OK;
}

/* ── 以太网（IP101 PHY） ──────────────────────── */

/** 以太网事件处理器。 */
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* 可以从事件数据中获取以太网驱动句柄 */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}

/** IP_EVENT_ETH_GOT_IP事件处理器 */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

/** 初始化以太网驱动。 */
esp_err_t bsp_eth_init(void)
{
    esp_err_t ret = ESP_OK;

    esp_netif_init();
    esp_event_loop_create_default();

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_BSP_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_BSP_ETH_PHY_RST_GPIO;
    // 初始化为默认的供应商特定MAC配置
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // 根据板级配置更新供应商特定MAC配置
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_BSP_ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_BSP_ETH_MDIO_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, ESP_FAIL, err, TAG, "Ethernet driver install failed");

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);
    glue = esp_eth_new_netif_glue(eth_handle);
    ESP_GOTO_ON_FALSE(esp_netif_attach(eth_netif, glue) == ESP_OK, ESP_FAIL, err, TAG, "Failed to attach Ethernet driver to netif");

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    return ESP_OK;

err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    return ret;
}

/** 获取以太网驱动句柄。 */
esp_eth_handle_t bsp_eth_get_handle(void)
{
    return eth_handle;
}

/** 反初始化以太网驱动。 */
void bsp_eth_deinit(void)
{
    esp_eth_stop(eth_handle);
    esp_eth_del_netif_glue(glue);
    esp_netif_destroy(eth_netif);
    esp_netif_deinit();

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
    esp_eth_get_mac_instance(eth_handle, &mac);
    esp_eth_get_phy_instance(eth_handle, &phy);
    esp_eth_driver_uninstall(eth_handle);
    if (mac) {
        mac->del(mac);
    }
    if (phy) {
        phy->del(phy);
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
    ESP_ERROR_CHECK(esp_event_loop_delete_default());

    eth_handle = NULL;
    glue = NULL;
    eth_netif = NULL;
}
