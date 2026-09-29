# Live2D RT30 基线验证入口组件
idf_component_register(
    SRCS "main.cpp" "profile_benchmark.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_common
        esp_rom
        mbedtls
        esp_timer
        heap
        lvgl__lvgl
        wt99p4c5_s1_board
        sys_display
        sys_display_buffer
        sys_display_flush
        sys_storage
        sys_monitor
        live2d_engine
        live2d_renderer)

execute_process(COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${PROJECT_DIR}"
    OUTPUT_VARIABLE L2D_ESP_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT L2D_ESP_COMMIT)
    set(L2D_ESP_COMMIT "uncommitted-worktree")
endif()
get_filename_component(L2D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${L2D_ROOT}/bench/scenarios"
    "${L2D_ROOT}/src/engine"
    "${L2D_ROOT}/src/internal"
    "${L2D_ROOT}/src/internal/pe/core"
    "${L2D_ROOT}/src/internal/pe/kernel")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    L2D_ESP_COMMIT="${L2D_ESP_COMMIT}"
    L2D_PC_COMMIT="78fc634")
