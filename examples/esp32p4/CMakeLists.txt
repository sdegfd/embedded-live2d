# 根项目 CMakeLists - Live2D RT30 实时多轴基线验证工程
# 基于 ESP-IDF，仅保留 Live2D 渲染链路（live2d_engine/renderer + 显示/SD/监控），
# 隔离成熟工程的 game/agent/audio 等复杂度，用于验证 RT30 实时多轴功能。

# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# esp_hosted/esp_wifi_remote Kconfig currently references $(ESP_IDF_VERSION)
# and expects the major.minor form used by files such as Kconfig.idf_v5.5.in.
# ESP-IDF 5.5 exports IDF_VERSION but not ESP_IDF_VERSION for Kconfig, so keep
# this project-local compatibility value until the managed components fix it.
set(ENV{ESP_IDF_VERSION} "5.5")

# Shared runtime lives at the repo root. The board package stays in this project.
get_filename_component(L2D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${CMAKE_CURRENT_SOURCE_DIR}/components/sys
    ${L2D_ROOT}/Live2D/ports/esp_idf/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(live2d_rt30_baseline)
