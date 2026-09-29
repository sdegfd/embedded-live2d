# 系统显示组件，封装 MIPI-DSI 显示初始化和 LVGL 图形库适配
idf_component_register(
    SRCS "sys_display.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        esp_timer
        esp_driver_ppa
        esp_lcd
        wt99p4c5_s1_board
        lvgl__lvgl
        espressif__esp_lvgl_adapter)
