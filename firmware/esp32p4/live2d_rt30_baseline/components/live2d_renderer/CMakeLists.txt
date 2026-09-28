# Live2D 渲染器组件，将 Live2D 引擎输出转换为显示缓冲区格式，支持 PPA 硬件加速
idf_component_register(
    SRCS "live2d_renderer.c"
    INCLUDE_DIRS "include"
    REQUIRES live2d_engine sys_display_buffer esp_common esp_driver_ppa esp_mm esp_timer)
