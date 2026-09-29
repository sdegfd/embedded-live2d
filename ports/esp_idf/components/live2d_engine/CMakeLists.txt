# Shared Live2D runtime as an ESP-IDF component. Sources stay in the repo
# root. This file does not copy them and does not compile the editor engine.
get_filename_component(L2D_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
include(${L2D_ROOT}/cmake/l2d_sources.cmake)

set(L2D_ESP_SOURCES "")
foreach(src ${L2D_RUNTIME_SOURCES})
    list(APPEND L2D_ESP_SOURCES "${L2D_ROOT}/${src}")
endforeach()
list(APPEND L2D_ESP_SOURCES "${L2D_ROOT}/ports/esp_idf/common/l2d_port_esp.c")

idf_component_register(
    SRCS ${L2D_ESP_SOURCES}
    INCLUDE_DIRS
        "${L2D_ROOT}/include"
    PRIV_INCLUDE_DIRS
        "${L2D_ROOT}/src/engine"
        "${L2D_ROOT}/src/memory"
        "${L2D_ROOT}/src/internal"
        "${L2D_ROOT}/src/internal/pe/core"
        "${L2D_ROOT}/src/internal/pe/kernel"
    REQUIRES esp_common esp_timer heap)

function(l2d_cfg name config_var)
    if(${config_var})
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ${name}=1)
    else()
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ${name}=0)
    endif()
endfunction()

l2d_cfg(L2D_CFG_PROFILE_TIMING CONFIG_L2D_PROFILE_TIMING)
l2d_cfg(L2D_CFG_PROFILE_FINE CONFIG_L2D_PROFILE_FINE)
l2d_cfg(L2D_CFG_PROFILE_VISUAL CONFIG_L2D_PROFILE_VISUAL)
l2d_cfg(L2D_CFG_PROFILE_DETAIL CONFIG_L2D_PROFILE_DETAIL)
l2d_cfg(L2D_CFG_PROFILE_ROI_DIAG CONFIG_L2D_PROFILE_ROI_DIAG)
l2d_cfg(L2D_CFG_SRM_ROI CONFIG_L2D_SRM_ROI)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -Wno-error=array-parameter
    -Wno-error=char-subscripts
    -Wno-error=misleading-indentation
    -Wno-error=strict-aliasing
    -Wno-error=uninitialized
    -Wno-parentheses
    -Wno-format)
