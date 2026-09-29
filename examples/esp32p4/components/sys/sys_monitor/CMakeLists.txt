# 系统监控组件，提供 CPU 使用率和 FPS 统计功能
idf_component_register(
    SRCS "sys_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        esp_timer       # 周期性采样定时器
        lvgl__lvgl)     # LVGL 事件回调，用于 FPS 计数
