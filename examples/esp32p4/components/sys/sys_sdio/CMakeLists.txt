# SD 卡异步串行读取服务：单任务串行消费请求队列 + 整文件 fread 到调用方 PSRAM。
idf_component_register(
    SRCS "sys_sdio.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        esp_timer
        heap
        freertos
        fatfs
        wt99p4c5_s1_board)
