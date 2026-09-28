# 显示刷新组件，将 RGB565 帧缓冲通过 MIPI-DSI 或 LVGL 推送到硬件显示
idf_component_register(
    SRCS "sys_display_flush.c"
    INCLUDE_DIRS "include"
    REQUIRES
        sys_display_buffer    # 帧缓冲数据源
        sys_display           # 显示面板和背光控制
        lvgl__lvgl            # LVGL 刷新回调
        esp_mm
        esp_common)
