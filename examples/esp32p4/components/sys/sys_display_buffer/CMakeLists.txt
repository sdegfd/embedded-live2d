# 显示缓冲区组件，管理 PSRAM 中的 ARGB8888 渲染目标和 RGB565 帧缓冲
idf_component_register(
    SRCS "sys_display_buffer.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common heap)
