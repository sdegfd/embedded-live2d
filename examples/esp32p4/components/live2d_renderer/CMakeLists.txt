# Live2D 渲染器组件，将 Live2D 引擎输出转换为显示缓冲区格式，支持 PPA 硬件加速
get_filename_component(L2D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

idf_component_register(
    SRCS "live2d_renderer.c"
    INCLUDE_DIRS "include"
    REQUIRES live2d_engine sys_display_buffer esp_common esp_driver_ppa esp_driver_bitscrambler esp_mm esp_timer freertos)

if(CONFIG_L2D_CLEAR_PIE OR CONFIG_L2D_CONVERT_PIE)
    target_sources(${COMPONENT_LIB} PRIVATE "l2d_p4_pie.c" "l2d_p4_pie.S")
    set_source_files_properties(l2d_p4_pie.S PROPERTIES
        COMPILE_OPTIONS "-march=rv32imafc_zicsr_zifencei_xesppie")
endif()

target_include_directories(${COMPONENT_LIB} PRIVATE
    "${L2D_ROOT}/Live2D/src/internal/pe/core"
    "${L2D_ROOT}/Live2D/src/internal/pe/kernel"
    "${L2D_ROOT}/Live2D/src/internal")

if(CONFIG_L2D_CONVERT_BITSCRAMBLER)
    target_sources(${COMPONENT_LIB} PRIVATE "l2d_p4_bitscrambler.c")
    target_bitscrambler_add_src("l2d_bgra_rgb565.bsasm")
endif()
