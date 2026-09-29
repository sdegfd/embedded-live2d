cmake_minimum_required(VERSION 3.16)

# Headless ESP32-S3 entry. It links the same runtime as the P4 example and
# does not register a panel, PPA, or DSI driver.
set(ENV{ESP_IDF_VERSION} "5.5")
get_filename_component(L2D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${L2D_ROOT}/ports/esp_idf/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(l2d_s3_smoke)
