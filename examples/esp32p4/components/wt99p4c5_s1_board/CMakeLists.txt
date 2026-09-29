# WT99P4C5-S1 开发板板级支持包(BSP)组件，提供外设初始化和引脚配置
idf_component_register(
    SRCS "wt99p4c5_s1_board.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver vfs fatfs esp_eth esp_netif esp_event espressif__esp_lvgl_adapter lvgl__lvgl
    PRIV_REQUIRES esp_lcd usb spiffs
)
