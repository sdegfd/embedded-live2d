# 系统存储组件，统一管理 NVS、SD 卡文件系统、字体和 Live2D 模型文件的加载
idf_component_register(
    SRCS "sys_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES
        nvs_flash            # NVS 非易失性存储
        fatfs                # FAT 文件系统，用于 SD 卡
        esp_common
        heap
        wt99p4c5_s1_board    # 板级支持包，提供 SD 卡引脚和 SPIFFS 配置
        lvgl__lvgl           # LVGL FreeType 字体支持
        espressif__esp_lvgl_adapter # 官方 LVGL/FreeType 适配层
        sys_sdio)            # SD 异步串行分块读取服务
